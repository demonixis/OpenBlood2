#pragma once
#include "client/world.h"
#include "engine/audio.h"
#include <set>
namespace OpenBlood2
{
    struct ClientSound
    {
        SoundId id = 0;
        ObjectId owner, speaker;
        SoundParameters parameters;
        DVector position{};
        unsigned rate = 0, channels = 0;
        std::uint64_t frames = 0, loops = 0;
        double cursor = 0;
        bool done = false, speaker_lost = false, world_owned = false, auto_remove = false;
        std::string error;
    };
    struct ClientAudioFrame
    {
        std::uint64_t world = 0, sequence = 0;
        std::vector<ClientSound> sounds;
    };
    class ClientAudio
    {
      public:
        bool begin(std::uint64_t world);
        bool receive(ClientAudioFrame&&, const Resources&, std::string& error);
        SoundId play_effect(const Resources&, const SoundParameters&, DVector, std::string& error);
        bool advance(double seconds, DVector listener, DRotation rotation);
        std::uint64_t effects_started = 0, effects_failed = 0, effects_displaced = 0;
        // A mirrored voice whose clip cannot be prepared here stays silent on this
        // client only; the authoritative timeline and the game keep running.
        std::uint64_t mirror_failed = 0;
        std::string mirror_error;
        const ClientAudioFrame& frame() const
        {
            return frame_;
        }
        const ClientSound* descriptor(SoundId id) const;
        ObjectId owner(SoundId id) const
        {
            const auto* s = descriptor(id);
            return s ? s->owner : ObjectId{};
        }
        ObjectId speaker(SoundId id) const
        {
            const auto* s = descriptor(id);
            return s ? s->speaker : ObjectId{};
        }
        AudioMixer mixer; // Only this client owns output, PCM, music and playback caches.
      private:
        ClientAudioFrame frame_;
        SoundId high_id_ = 0; // A removed sound cannot restart in the same world.
        std::set<SoundId> unplayable_; // Mirrored voices this client could not prepare.
    };
} // namespace OpenBlood2
