#include "engine/audio.h"
#include "engine/audio_stream.h"
#include "engine/collision.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace OpenBlood2
{
    namespace
    {
        std::uint16_t u16(std::span<const std::uint8_t> b, std::size_t p)
        {
            return b[p] | (unsigned(b[p + 1]) << 8);
        }
        std::uint32_t u32(std::span<const std::uint8_t> b, std::size_t p)
        {
            return u16(b, p) | (unsigned(u16(b, p + 2)) << 16);
        }
        constexpr std::size_t max_clip_bytes = 64 * 1024 * 1024;
        SoundId next_sound = 1; // Simulation thread only; never reset on unload/reload or a new mixer.
    } // namespace
    bool read_wave(std::span<const std::uint8_t> bytes, AudioClip& out, std::string& error)
    {
        error.clear();
        auto fail = [&](const char* why)
        {
            error = why;
            return false;
        };
        if (bytes.size() < 12 || bytes.size() > 16 * 1024 * 1024 || std::memcmp(bytes.data(), "RIFF", 4) ||
            std::memcmp(bytes.data() + 8, "WAVE", 4) ||
            (std::uint64_t(u32(bytes, 4)) + 8 != bytes.size() && std::uint64_t(u32(bytes, 4)) + 9 != bytes.size()))
            return fail("Invalid/bounded RIFF WAVE envelope (16 MiB encoded limit)");
        std::span<const std::uint8_t> fmt, data;
        const auto riff_end = std::uint64_t(u32(bytes, 4)) + 8;
        bool final_odd_data = false;
        unsigned chunks = 0;
        bool seen_fmt = false, seen_data = false, loop_metadata = false;
        for (std::size_t pos = 12; pos < bytes.size();)
        {
            if (++chunks > 1024 || bytes.size() - pos < 8)
                return fail("Invalid WAVE chunk list");
            auto n = u32(bytes, pos + 4);
            const auto payload_end = std::uint64_t(pos) + 8 + n;
            const bool is_data = !std::memcmp(bytes.data() + pos, "data", 4);
            const bool last_odd_data =
                is_data && (n & 1) && (payload_end == bytes.size() || payload_end + 1 == bytes.size());
            // Retail PCM8 files may omit the terminal data pad, or exclude it from RIFF size.
            // Only this final odd data byte is exceptional; payloads and other chunks remain complete.
            const auto end = payload_end + ((last_odd_data && payload_end == bytes.size()) ? 0 : (n & 1));
            final_odd_data = last_odd_data;
            if (end > bytes.size())
                return fail("Truncated WAVE chunk/padding");
            auto chunk = bytes.subspan(pos + 8, n);
            if (!std::memcmp(bytes.data() + pos, "cue ", 4) || !std::memcmp(bytes.data() + pos, "smpl", 4) ||
                (!std::memcmp(bytes.data() + pos, "LIST", 4) && n >= 4 && !std::memcmp(chunk.data(), "adtl", 4)))
                loop_metadata = true;
            if (!std::memcmp(bytes.data() + pos, "fmt ", 4))
            {
                if (seen_fmt)
                    return fail("Duplicate WAVE format");
                seen_fmt = true;
                fmt = chunk;
            }
            if (!std::memcmp(bytes.data() + pos, "data", 4))
            {
                if (seen_data)
                    return fail("Duplicate WAVE data");
                seen_data = true;
                data = chunk;
            }
            pos = std::size_t(end);
        }
        if (riff_end != bytes.size() && (!final_odd_data || riff_end + 1 != bytes.size()))
            return fail("RIFF length differs outside terminal odd data padding");
        if (fmt.size() < 16 || data.empty())
            return fail("Missing WAVE format/data");
        const auto encoding = u16(fmt, 0), channels = u16(fmt, 2), block = u16(fmt, 12), bits = u16(fmt, 14);
        const auto rate = u32(fmt, 4);
        if (channels < 1 || channels > 2 || rate < 8000 || rate > 96000 || !block || block > 16384)
            return fail("Unsupported WAVE rate/channels/block alignment");
        std::uint64_t frames = 0;
        if (encoding == 1)
        {
            if ((bits != 8 && bits != 16) || block != channels * (bits / 8) || data.size() % block ||
                u32(fmt, 8) != rate * block)
                return fail("Invalid/unsupported PCM layout");
            frames = data.size() / block;
        }
        else if (encoding == 2 || encoding == 17)
        {
            if (bits != 4 || fmt.size() < 20 || u16(fmt, 16) < 2 || u16(fmt, 16) > fmt.size() - 18 ||
                data.size() % block || block < channels * (encoding == 2 ? 7 : 4))
                return fail("Invalid ADPCM block layout");
            const auto samples = u16(fmt, 18);
            const auto expected =
                encoding == 2 ? (block - 7 * channels) * 2 / channels + 2 : (block - 4 * channels) * 2 / channels + 1;
            if (samples != expected ||
                (encoding == 2 && (fmt.size() < 22 || !u16(fmt, 20) || u16(fmt, 20) > 256 ||
                                   (fmt.size() < 22 + 4u * u16(fmt, 20) || u16(fmt, 16) < 4 + 4u * u16(fmt, 20)))))
                return fail("Invalid ADPCM samples/coefficient table");
            frames = std::uint64_t(data.size() / block) * samples;
        }
        else
            return fail("Unsupported WAVE encoding (PCM8/16, MS/IMA ADPCM only)");
        if (!frames || frames * channels * sizeof(float) > max_clip_bytes)
            return fail("Decoded clip exceeds 64 MiB");
        auto* io = SDL_IOFromConstMem(bytes.data(), bytes.size());
        if (!io)
        {
            error = SDL_GetError();
            return false;
        }
        SDL_AudioSpec source{};
        Uint8* decoded = nullptr;
        Uint32 length = 0;
        if (!SDL_LoadWAV_IO(io, true, &source, &decoded, &length))
        {
            error = SDL_GetError();
            return false;
        }
        if (source.freq != int(rate) || source.channels != channels || length > max_clip_bytes ||
            length > std::numeric_limits<int>::max())
        {
            SDL_free(decoded);
            return fail("Unexpected decoded WAVE layout");
        }
        SDL_AudioSpec dest{SDL_AUDIO_F32, int(channels), int(rate)};
        Uint8* converted = nullptr;
        int converted_length = 0;
        const bool converted_ok =
            SDL_ConvertAudioSamples(&source, decoded, int(length), &dest, &converted, &converted_length);
        SDL_free(decoded);
        if (!converted_ok)
        {
            error = SDL_GetError();
            return false;
        }
        if (converted_length <= 0 || std::size_t(converted_length) > max_clip_bytes ||
            converted_length % (channels * sizeof(float)))
        {
            SDL_free(converted);
            return fail("Invalid converted PCM length");
        }
        AudioClip value;
        value.rate = rate;
        value.channels = channels;
        value.encoding = encoding;
        value.loop_metadata = loop_metadata;
        value.samples.resize(std::size_t(converted_length) / sizeof(float));
        std::memcpy(value.samples.data(), converted, converted_length);
        SDL_free(converted);
        if (!std::all_of(value.samples.begin(), value.samples.end(), [](float f) { return std::isfinite(f); }))
            return fail("Nonfinite decoded samples");
        out = std::move(value);
        return true;
    }
    bool AudioMixer::load(const Resources& resources, std::string_view path, std::shared_ptr<const AudioClip>& out,
                          std::string& error)
    {
        std::string name;
        if (!resource_path(path, name))
        {
            error = "Invalid sound resource path";
            return false;
        }
        if (auto it = clips_.find(name); it != clips_.end())
        {
            out = it->second;
            return true;
        }
        const auto* resource = resources.find(name);
        if (!resource || resource->size > 16 * 1024 * 1024)
        {
            error = "Missing or oversized sound resource";
            return false;
        }
        if (cached_clips() >= 128)
        {
            error = "Sound cache exceeds 128 clips";
            return false;
        }
        std::vector<std::uint8_t> bytes;
        auto clip = std::make_shared<AudioClip>();
        if (!resources.read(name, bytes, error) || !read_wave(bytes, *clip, error))
            return false;
        const auto size = clip->samples.size() * sizeof(float);
        if (size > 256 * 1024 * 1024 - cached_bytes_)
        {
            error = "Sound cache exceeds 256 MiB";
            return false;
        }
        cached_bytes_ += size;
        clips_.emplace(name, clip);
        out = std::move(clip);
        return true;
    }
    bool AudioMixer::load_stream(const Resources& resources, std::string_view path,
                                 std::shared_ptr<const AudioClip>& out, std::string& error)
    {
        std::string name;
        if (!resource_path(path, name))
        {
            error = "Invalid streaming sound resource path";
            return false;
        }
        if (auto it = streams_.find(name); it != streams_.end())
        {
            if (!it->second->stream->validate(error))
                return false;
            out = it->second;
            return true;
        }
        const auto* resource = resources.find(name);
        if (!resource)
        {
            error = "Missing streaming sound resource";
            return false;
        }
        if (cached_clips() >= 128 || PcmStream::buffer_bytes > 256 * 1024 * 1024 - cached_bytes_)
        {
            error = "Streaming sound cache budget exceeded";
            return false;
        }
        auto stream = PcmStream::open(*resource, error);
        if (!stream)
            return false;
        auto clip = std::make_shared<AudioClip>();
        clip->rate = stream->rate;
        clip->channels = stream->channels;
        clip->encoding = 1;
        clip->stream_frames = stream->frames;
        clip->loop_metadata = stream->loop_metadata;
        clip->stream = std::move(stream);
        streams_.emplace(name, clip);
        cached_bytes_ += PcmStream::buffer_bytes;
        out = std::move(clip);
        return true;
    }
    SoundId AudioMixer::play(const Resources& resources, HOBJECT owner, const SoundRequest& request, DVector position,
                             std::string& error)
    {
        error.clear();
        if (voices_.size() >= 128 || !next_sound || next_sound == std::numeric_limits<SoundId>::max() ||
            !physics_vector(position) || !std::isfinite(request.volume) || request.volume < 0 || request.volume > 1 ||
            !std::isfinite(request.inner_radius) || !std::isfinite(request.outer_radius) || request.inner_radius < 0 ||
            request.outer_radius <= request.inner_radius || request.outer_radius > 1e6f ||
            request.resource.size() > 100 ||
            (request.space != SoundSpace::Local && request.space != SoundSpace::Ambient &&
             request.space != SoundSpace::Spatial))
        {
            error = "Invalid or over-limit sound request";
            return 0;
        }
        std::shared_ptr<const AudioClip> clip;
        if (!(request.streaming ? load_stream(resources, request.resource, clip, error)
                                : load(resources, request.resource, clip, error)))
            return 0;
        if (request.loop && clip->loop_metadata)
        {
            error = "WAVE region-loop metadata is not supported; refusing whole-clip substitution";
            return 0;
        }
        const auto id = next_sound++;
        AudioVoice voice;
        voice.id = id;
        voice.owner = owner;
        voice.request = request;
        voice.clip = std::move(clip);
        voice.position = position;
        voices_.emplace(id, std::move(voice));
        return id;
    }
    const AudioVoice* AudioMixer::get(SoundId id) const
    {
        auto it = voices_.find(id);
        return it == voices_.end() ? nullptr : &it->second;
    }
    AudioVoice* AudioMixer::get(SoundId id)
    {
        auto it = voices_.find(id);
        return it == voices_.end() ? nullptr : &it->second;
    }
    bool AudioMixer::kill(SoundId id)
    {
        return voices_.erase(id) != 0;
    }
    bool AudioMixer::advance(double seconds, DVector listener, DRotation rotation)
    {
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 60 || !physics_vector(listener) ||
            !physics_vector(rotation.m_Vec) || !std::isfinite(rotation.m_Spin))
            return false;
        const float norm = vector_dot(rotation.m_Vec, rotation.m_Vec) + rotation.m_Spin * rotation.m_Spin;
        if (!std::isfinite(norm) || std::abs(norm - 1) > 0.001f)
            return false;
        const double total = seconds * rate + remainder_;
        const auto frames = std::uint64_t(std::floor(total + 1e-8));
        remainder_ = std::max(0.0, total - double(frames));
        const bool rendering = inspect_pcm || output_;
        const auto count =
            rendering ? std::min<std::uint64_t>(frames, 12000) : 0; // 250 ms bounded mixer/output work after a stall.
        mixed.assign(count * 2, 0);
        peak = 0;
        const auto right = rotate_vector({1, 0, 0}, rotation);
        bool stream_failed = false;
        for (auto& [id, v] : voices_)
        {
            (void)id;
            if (!v.error.empty())
            {
                stream_failed = true;
                continue;
            }
            if (v.done)
                continue;
            if (v.clip->stream && !v.clip->stream->validate(v.error))
            {
                stream_failed = true;
                continue;
            }
            float gain = v.request.volume, pan = 0;
            if (v.request.space != SoundSpace::Local)
            {
                const auto direction = vector_subtract(v.position, listener);
                const auto distance = std::sqrt(vector_dot(direction, direction));
                gain *=
                    std::clamp((v.request.outer_radius - distance) / (v.request.outer_radius - v.request.inner_radius),
                               0.0f, 1.0f);
                if (v.request.space == SoundSpace::Spatial && distance > 1e-6f)
                    pan = std::clamp(vector_dot(direction, right) / distance, -1.0f, 1.0f);
            }
            v.left = gain * (v.request.space != SoundSpace::Spatial ? 1 : std::sqrt((1 - pan) * .5f));
            v.right = gain * (v.request.space != SoundSpace::Spatial ? 1 : std::sqrt((1 + pan) * .5f));
            const auto& clip = *v.clip;
            const double increment = double(clip.rate) / rate;
            for (std::size_t frame = 0; frame < count; ++frame)
            {
                auto cursor = v.cursor + frame * increment;
                if (v.request.loop)
                    cursor = std::fmod(cursor, double(clip.frames()));
                else if (cursor >= clip.frames())
                    break;
                const auto a = std::size_t(cursor);
                const auto b = v.request.loop ? (a + 1) % clip.frames() : std::min(a + 1, clip.frames() - 1);
                const float blend = float(cursor - a);
                float x[2]{}, y[2]{};
                bool ok = true;
                for (unsigned channel = 0; channel < clip.channels; ++channel)
                {
                    if (clip.stream)
                        ok = ok && clip.stream->sample(a, channel, x[channel], v.error) &&
                             clip.stream->sample(b, channel, y[channel], v.error);
                    else
                    {
                        x[channel] = clip.samples[a * clip.channels + channel];
                        y[channel] = clip.samples[b * clip.channels + channel];
                    }
                }
                if (!ok)
                {
                    stream_failed = true;
                    break;
                }
                mixed[frame * 2] += (x[0] + (y[0] - x[0]) * blend) * v.left;
                auto channel = clip.channels - 1;
                mixed[frame * 2 + 1] += (x[channel] + (y[channel] - x[channel]) * blend) * v.right;
            }
            if (!v.error.empty())
                continue;
            const auto advanced = v.cursor + frames * increment;
            if (v.request.loop)
            {
                const auto wraps = std::uint64_t(advanced / clip.frames());
                v.loops += std::min(wraps, std::numeric_limits<std::uint64_t>::max() - v.loops);
                v.cursor = std::fmod(advanced, double(clip.frames()));
            }
            else
            {
                v.cursor = std::min(double(clip.frames()), advanced);
                v.done = v.cursor >= clip.frames();
            }
        }
        if (!instruments.advance(frames, mixed))
            return false;
        if (stream_failed)
        {
            mixed.clear();
            peak = 0;
            output_error = "Streaming audio resource read failed; inspect voice errors";
            return false;
        } // Never submit a partial failed read.
        for (auto& value : mixed)
        {
            value = std::clamp(value, -1.0f, 1.0f);
            peak = std::max(peak, std::abs(value));
        }
        mixed_frames += count;
        if (rendering)
            dropped_frames += frames - count;
        if (output_ && count)
        {
            const auto queued = SDL_GetAudioStreamQueued(output_);
            if (queued < 0)
            {
                output_error = SDL_GetError();
                return true;
            }
            if (queued > int(rate * 2 * sizeof(float) / 4))
            {
                dropped_frames += std::uint64_t(queued) / (2 * sizeof(float));
                if (!SDL_ClearAudioStream(output_))
                {
                    output_error = SDL_GetError();
                    return true;
                }
            }
            if (!SDL_PutAudioStreamData(output_, mixed.data(), int(mixed.size() * sizeof(float))))
                output_error = SDL_GetError();
            else
                submitted_frames += count;
        }
        return true;
    }
    bool AudioMixer::open_output(std::string& error)
    {
        if (output_)
            return true;
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            error = output_error = SDL_GetError();
            return false;
        }
        audio_initialized_ = true;
        SDL_AudioSpec spec{SDL_AUDIO_F32, 2, int(rate)};
        output_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!output_ || !SDL_ResumeAudioStreamDevice(output_))
        {
            error = output_error = SDL_GetError();
            if (output_)
                SDL_DestroyAudioStream(output_);
            output_ = nullptr;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            audio_initialized_ = false;
            return false;
        }
        output_error.clear();
        return true;
    }
    bool AudioMixer::transfer_output_to(AudioMixer& destination, std::string& error)
    {
        if (&destination == this || destination.output_ || destination.audio_initialized_)
        {
            error = "Audio output transfer requires a separate offline destination";
            return false;
        }
        // Last fallible operation before committing a level: old queued PCM must
        // not play in the destination. Voices and caches remain with their world.
        if (output_ && !SDL_ClearAudioStream(output_))
        {
            error = SDL_GetError();
            return false;
        }
        destination.output_ = output_;
        destination.audio_initialized_ = audio_initialized_;
        destination.inspect_pcm = inspect_pcm;
        output_ = nullptr;
        audio_initialized_ = false;
        return true;
    }
    void AudioMixer::clear()
    {
        voices_.clear();
        instruments.clear();
        clips_.clear();
        streams_.clear();
        cached_bytes_ = 0;
        mixed.clear();
        remainder_ = 0;
        peak = 0;
        mixed_frames = submitted_frames = dropped_frames = 0;
        output_error.clear();
        if (output_ && !SDL_ClearAudioStream(output_))
            output_error = SDL_GetError();
    }
    AudioMixer::~AudioMixer()
    {
        if (output_)
            SDL_DestroyAudioStream(output_);
        if (audio_initialized_)
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
} // namespace OpenBlood2
