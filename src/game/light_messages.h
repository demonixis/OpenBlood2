#pragma once
#include "basedefs_de.h"
#include <string>
namespace OpenBlood2
{
    // Original Shared/ClientServerShared.h numeric wave identifiers.
    enum WaveTypes : DBYTE
    {
        WAVE_NONE,
        WAVE_SQUARE,
        WAVE_SAW,
        WAVE_RAMPUP,
        WAVE_RAMPDOWN,
        WAVE_SINE,
        WAVE_FLICKER1,
        WAVE_FLICKER2,
        WAVE_FLICKER3,
        WAVE_FLICKER4,
        WAVE_STROBE,
        WAVE_SEARCH
    };
    struct LightWave
    {
        float min = 0, max = 0, frequency = 0, phase = 0;
        DBYTE kind = WAVE_NONE;
    };
    struct LightSettings
    {
        DVector color{255, 255, 255};
        DDWORD flags = 0;
        LightWave intensity{.5f, 1, 4, 0, WAVE_NONE}, radius{0, 500, 0, 4, WAVE_NONE};
        std::string ramp_up_sound, ramp_down_sound;
    };
    bool valid_light_settings(const LightSettings&);
} // namespace OpenBlood2
