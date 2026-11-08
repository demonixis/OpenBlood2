#include "app/startup_movies.h"
#include "engine/config.h"
#include "engine/smacker.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <spdlog/spdlog.h>

namespace OpenBlood2
{
    namespace
    {
        constexpr std::array names{"Monolith.smk", "Gti.smk", "Intro.smk"};

        std::filesystem::path find_entry(const std::filesystem::path& root, const char* name, bool directory = false)
        {
            if (root.empty())
                return {};
            std::error_code error;
            const auto matches_type = [&](const std::filesystem::path& path)
            {
                std::error_code type_error;
                return directory ? std::filesystem::is_directory(path, type_error)
                                 : std::filesystem::is_regular_file(path, type_error);
            };
            const auto exact = root / name;
            if (matches_type(exact))
                return exact;
            std::filesystem::path match;
            for (std::filesystem::directory_iterator it(root, error), end; !error && it != end; it.increment(error))
            {
                const auto candidate = path_text(it->path().filename());
                if (SDL_strcasecmp(candidate.c_str(), name) || !matches_type(it->path()))
                    continue;
                if (!match.empty())
                    return {}; // An exact name is required to disambiguate case variants.
                match = it->path();
            }
            return error ? std::filesystem::path{} : match;
        }
    } // namespace

    StartupMovies::StartupMovies() = default;

    StartupMovies::~StartupMovies()
    {
        stop_clip();
    }

    void StartupMovies::start(const Config& config, float gain)
    {
        if (config.demo)
            return;
        root_ = config.data_dir;
        movies_ = find_entry(root_, "Movies", true);
        count_ = config.nightmares ? 2 : unsigned(names.size());
        current_ = 0;
        gain_ = gain;
        next_clip();
    }

    std::string_view StartupMovies::name() const
    {
        return active() ? names[current_ - 1] : std::string_view{};
    }

    const HudSurface& StartupMovies::surface() const
    {
        return scaled_.rgba.empty() ? decoder_->video() : scaled_;
    }

    void StartupMovies::stop_audio()
    {
        if (audio_)
            SDL_DestroyAudioStream(audio_);
        audio_ = nullptr;
        if (audio_initialized_)
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audio_initialized_ = false;
    }

    void StartupMovies::stop_clip()
    {
        stop_audio();
        decoder_.reset();
        scaled_ = {};
        focused_ = false;
        elapsed_ = 0;
        submitted_audio_ = 0;
    }

    void StartupMovies::next_clip()
    {
        stop_clip();
        while (current_ < count_)
        {
            const auto* filename = names[current_++];
            auto path = find_entry(root_, filename);
            if (path.empty())
                path = find_entry(movies_, filename);
            if (path.empty())
            {
                spdlog::info("Startup movie absent: {}", filename);
                continue;
            }
            auto decoder = std::make_unique<SmackerDecoder>();
            std::string error;
            if (!decoder->open(path, error) || !decoder->next(error))
            {
                spdlog::warn("Startup movie {}: {}", path_text(path), error);
                continue;
            }
            const auto& info = decoder->info();
            if (info.width * 3 != info.height * 4)
            {
                spdlog::warn("Startup movie {}: expected a 4:3 frame", path_text(path));
                continue;
            }
            decoder_ = std::move(decoder);
            if (info.width != 640 || info.height != 480)
                scaled_ = {640, 480, std::vector<std::uint8_t>(640 * 480 * 4)};
            if (info.audio_rate && info.audio_channels)
            {
                audio_initialized_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
                if (audio_initialized_)
                {
                    const SDL_AudioSpec format{SDL_AUDIO_S16, int(info.audio_channels), int(info.audio_rate)};
                    audio_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
                }
                if (!audio_ || !SDL_SetAudioStreamGain(audio_, gain_))
                {
                    spdlog::warn("Startup movie audio: {}", SDL_GetError());
                    stop_audio();
                }
            }
            queue_audio();
            publish_frame();
            spdlog::info("Startup movie: {} ({}x{}, {} frames)", filename, info.width, info.height, info.frames);
            return;
        }
    }

    void StartupMovies::queue_audio()
    {
        if (!audio_)
            return;
        const auto samples = decoder_->audio();
        const auto& info = decoder_->info();
        const auto queued = SDL_GetAudioStreamQueued(audio_);
        // The stock first packet contains a second of audio. Keep a bounded margin around that lead.
        const auto capacity = std::size_t(info.audio_rate) * info.audio_channels * sizeof(std::int16_t) * 4;
        if (queued < 0 || std::size_t(queued) + samples.size_bytes() > capacity ||
            (!samples.empty() && !SDL_PutAudioStreamData(audio_, samples.data(), int(samples.size_bytes()))) ||
            (decoder_->decoded_frames() == info.frames && !SDL_FlushAudioStream(audio_)))
        {
            spdlog::warn("Startup movie audio stopped: queue error or limit exceeded");
            stop_audio();
            return;
        }
        submitted_audio_ += samples.size_bytes();
    }

    void StartupMovies::publish_frame()
    {
        if (!scaled_.rgba.empty())
            hud_scale(scaled_, decoder_->video());
        ++revision_;
    }

    void StartupMovies::advance(double seconds, bool focused, bool skip)
    {
        if (!active())
            return;
        if (skip)
        {
            spdlog::info("Startup movie skipped: {}", name());
            next_clip();
            return;
        }
        if (focused != focused_)
        {
            focused_ = focused;
            if (audio_ && !(focused ? SDL_ResumeAudioStreamDevice(audio_) : SDL_PauseAudioStreamDevice(audio_)))
            {
                spdlog::warn("Startup movie audio: {}", SDL_GetError());
                stop_audio();
            }
            return; // The current delta belongs to the previous focus state.
        }
        if (!focused)
            return;
        const auto& info = decoder_->info();
        const auto queued = audio_ ? SDL_GetAudioStreamQueued(audio_) : -1;
        if (queued > 0 && std::uint64_t(queued) <= submitted_audio_)
        {
            // SDL counts queued input in the original PCM format. Keep picture and captions
            // on that sample clock, including when rendering stalls or the window is resized.
            const auto bytes_per_second = info.audio_rate * info.audio_channels * sizeof(std::int16_t);
            elapsed_ = std::max(elapsed_, double(submitted_audio_ - queued) / double(bytes_per_second));
        }
        else
            elapsed_ += std::min(seconds, .25); // Silent playback and an exhausted audio tail still advance.
        const double duration = info.frames * info.frame_seconds;
        const auto wanted = unsigned(std::min(elapsed_ / info.frame_seconds, double(info.frames - 1))) + 1;
        const auto decoded = decoder_->decoded_frames();
        const auto target = std::min(wanted, decoded + 32);
        while (decoder_->decoded_frames() < target)
        {
            std::string error;
            if (!decoder_->next(error))
            {
                spdlog::warn("Startup movie {}: {}", name(), error);
                next_clip();
                return;
            }
            queue_audio();
        }
        if (decoded != target)
            publish_frame();
        if (target == info.frames && elapsed_ >= duration &&
            (!audio_ || SDL_GetAudioStreamQueued(audio_) <= 0 || elapsed_ >= duration + .25))
        {
            spdlog::info("Startup movie completed: {}", name());
            next_clip();
        }
    }
} // namespace OpenBlood2
