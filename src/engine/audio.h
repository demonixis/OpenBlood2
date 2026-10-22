#pragma once
#include "basedefs_de.h"
#include "engine/dls_sampler.h"
#include "engine/ima_transport.h"
#include "engine/resources.h"
#include <SDL3/SDL_audio.h>
#include <memory>
namespace OpenBlood2
{
    class PcmStream;
    using SoundId = std::uint64_t;
    struct AudioClip
    {
        unsigned rate = 0, channels = 0, encoding = 0;
        bool loop_metadata = false; // Region loops require a separate validated reader; never silently ignore them.
        std::vector<float> samples;
        std::shared_ptr<PcmStream> stream;
        std::size_t stream_frames = 0;
        std::size_t frames() const
        {
            return stream ? stream_frames : (channels ? samples.size() / channels : 0);
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
        bool streaming = false;       // Bounded disk-backed PCM, using the same local simulation clock.
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
        std::string error; // A failed stream is not successful sound completion.
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
        bool pause_output(bool paused, std::string& error);
        bool set_gains(float sound, float music);
        bool paused() const
        {
            return paused_;
        }
        bool transfer_output_to(AudioMixer& destination, std::string& error);
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
            return clips_.size() + streams_.size();
        }
        std::size_t cached_bytes() const
        {
            return cached_bytes_;
        }
        // Summed PcmStream read cost/count of the cached streams (diagnostic only).
        std::uint64_t stream_read_ns() const;
        std::uint64_t stream_reads() const;
        bool advance(double, DVector listener, DRotation rotation);
        void clear();
        DlsSampler instruments;      // World-owned diagnostic notes share the sound output/clock.
        ImaLoopTransport music_loop; // Prepared-sequence repeat transport, independent of inspection.
        std::string music_error;     // Durable transport/delivery failure; reset with the world or a new start.
        bool inspect_pcm = false;    // Headless simulation can advance exact sample cursors without rendering blocks.
        std::vector<float> mixed;
        // Device output keeps at least `output_lead` seconds queued: frames mixed ahead of the
        // simulation step to reach it are counted in lead_frames. Zero (the default) keeps exact
        // sample stepping; the interactive frontend uses interactive_output_lead. Transferred with
        // the output.
        static constexpr double interactive_output_lead = 0.06;
        double output_lead = 0;
        std::uint64_t mixed_frames = 0, submitted_frames = 0, dropped_frames = 0, lead_frames = 0;
        float peak = 0;
        std::string output_error;

      private:
        std::map<std::string, std::shared_ptr<const AudioClip>> clips_;
        std::map<std::string, std::shared_ptr<const AudioClip>> streams_;
        std::size_t stream_serial_ = 0;           // Window phase of the next stream (reset by clear).
        std::vector<const PcmStream*> validated_; // Streams whose archive this advance already checked.
        bool load_stream(const Resources&, std::string_view, std::shared_ptr<const AudioClip>&, std::string&);
        std::map<SoundId, AudioVoice> voices_;
        std::size_t cached_bytes_ = 0;
        double remainder_ = 0;
        SDL_AudioStream* output_ = nullptr;
        bool audio_initialized_ = false;
        bool paused_ = false;
        float sound_gain_ = 1, music_gain_ = 1;
        std::vector<float> music_mix_;
    };
} // namespace OpenBlood2
