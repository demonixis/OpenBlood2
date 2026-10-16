#pragma once
#include "basedefs_de.h"
#include "engine/resources.h"
#include <SDL3/SDL_audio.h>
#include <memory>
namespace OpenBlood2
{
    using SoundId = std::uint64_t;
    struct AudioClip
    {
        unsigned rate = 0, channels = 0, encoding = 0;
        bool loop_metadata = false; // Region loops require a separate validated reader; never silently ignore them.
        std::vector<float> samples;
        std::size_t frames() const
        {
            return channels ? samples.size() / channels : 0;
        }
        double duration() const
        {
            return rate ? double(frames()) / rate : 0;
        }
    };
    bool read_wave(std::span<const std::uint8_t>, AudioClip&, std::string&);
    enum class SoundSpace
    {
        Local,
        Ambient,
        Spatial
    };
    struct SoundRequest
    {
        std::string resource;
        HOBJECT speaker = nullptr;
        SoundSpace space = SoundSpace::Spatial;
        float inner_radius = 250, outer_radius = 1000, volume = 1;
        bool loop = false;
        DBYTE priority = 0;           // Authored intent; priority arbitration is unavailable.
        bool reverb_requested = true; // Original cinematic flag; dry output currently only.
    };
    struct AudioVoice
    {
        SoundId id = 0;
        HOBJECT owner = nullptr;
        SoundRequest request;
        std::shared_ptr<const AudioClip> clip;
        DVector position{};
        double cursor = 0;
        float left = 1, right = 1;
        bool done = false, speaker_lost = false;
        bool world_owned = false; // Owner is provenance only for transferred, unattached sounds.
        bool auto_remove = false; // Fire-and-forget one-shots release their voice on completion.
        std::uint64_t loops = 0;
    };
    class AudioMixer
    {
      public:
        static constexpr unsigned rate = 48000;
        AudioMixer() = default;
        AudioMixer(const AudioMixer&) = delete;
        AudioMixer& operator=(const AudioMixer&) = delete;
        ~AudioMixer();
        bool open_output(std::string& error);
        bool has_output() const
        {
            return output_ != nullptr;
        }
        bool load(const Resources&, std::string_view, std::shared_ptr<const AudioClip>&, std::string&);
        SoundId play(const Resources&, HOBJECT owner, const SoundRequest&, DVector, std::string&);
        bool kill(SoundId);
        const AudioVoice* get(SoundId) const;
        AudioVoice* get(SoundId);
        const auto& voices() const
        {
            return voices_;
        }
        std::size_t cached_clips() const
        {
            return clips_.size();
        }
        std::size_t cached_bytes() const
        {
            return cached_bytes_;
        }
        bool advance(double, DVector listener, DRotation rotation);
        void clear();
        bool inspect_pcm = false; // Headless simulation can advance exact sample cursors without rendering blocks.
        std::vector<float> mixed;
        std::uint64_t mixed_frames = 0, submitted_frames = 0, dropped_frames = 0;
        float peak = 0;
        std::string output_error;

      private:
        std::map<std::string, std::shared_ptr<const AudioClip>> clips_;
        std::map<SoundId, AudioVoice> voices_;
        std::size_t cached_bytes_ = 0;
        double remainder_ = 0;
        SDL_AudioStream* output_ = nullptr;
        bool audio_initialized_ = false;
    };
} // namespace OpenBlood2
