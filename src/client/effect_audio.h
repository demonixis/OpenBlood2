#pragma once
#include "engine/audio.h"
namespace OpenBlood2
{
    class Resources;
    class EffectAudio
    {
      public:
        virtual ~EffectAudio() = default;
        virtual SoundId play_effect(const Resources&, const SoundParameters&, DVector, std::string&) = 0;
    };
} // namespace OpenBlood2
