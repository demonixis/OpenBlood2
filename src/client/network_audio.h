#pragma once
#include "client/world.h"
#include "engine/audio.h"
#include "net/admission.h"
#include "net/audio_events.h"
namespace OpenBlood2
{
    class NetworkAudio
    {
      public:
        bool begin(const Net::Joined&);
        bool receive(const Net::AudioEvent&, const Resources&, std::string& error);
        bool advance(double seconds, const ClientWorld&, DVector listener, DRotation rotation);
        SoundId play_voice(const Resources&, const SoundParameters&, std::string& error);
        bool stop_voice(SoundId);
        bool voice_done(SoundId, bool&) const;
        bool voice_duration(SoundId, float&) const;
        void clear();
        AudioMixer mixer;
        std::uint64_t started = 0, stopped = 0, completed = 0, displaced = 0;
        std::uint64_t local_started = 0, local_failed = 0, local_displaced = 0;
        std::uint64_t sequence() const
        {
            return sequence_;
        }

      private:
        struct Entry
        {
            ObjectId owner, speaker;
            bool retired = false;
        };
        Net::Joined joined_;
        std::uint64_t sequence_ = 0, high_id_ = 0;
        std::map<SoundId, Entry> voices_;
        struct LocalVoice
        {
            float duration = 0;
            double completion_frame = 0;
            bool failed = false;
        };
        // Retained presentation handles survive mixer preemption until their
        // original owner releases them. Silent completion uses only client PCM time.
        std::map<SoundId, LocalVoice> local_voices_;
    };
} // namespace OpenBlood2
