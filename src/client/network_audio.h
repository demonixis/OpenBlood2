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
        void clear();
        AudioMixer mixer;
        std::uint64_t started = 0, stopped = 0, completed = 0, displaced = 0;
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
    };
} // namespace OpenBlood2
