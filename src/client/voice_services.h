#pragma once
#include "engine/audio.h"
namespace OpenBlood2
{
    // Original VoiceMgr/CommLink need only the owning client's clock and retained
    // local voices. No player/server handle crosses this presentation boundary.
    class VoiceServices
    {
      public:
        virtual ~VoiceServices() = default;
        virtual float time() const = 0;
        virtual SoundId play_voice(const SoundParameters&, std::string& error) = 0;
        virtual bool stop_voice(SoundId) = 0;
        virtual bool voice_done(SoundId, bool&) const = 0;
        virtual bool voice_duration(SoundId, float&) const = 0;
    };
} // namespace OpenBlood2
