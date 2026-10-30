#pragma once
#include "basedefs_de.h"
#include <cstdint>
#include <string>
namespace OpenBlood2
{
    enum class SoundSpace
    {
        Local,
        Ambient,
        Spatial
    };
    // Copied playback values; no original handles or decoded asset pointers.
    struct SoundParameters
    {
        std::string resource;
        SoundSpace space = SoundSpace::Spatial;
        float inner_radius = 250, outer_radius = 1000, volume = 1;
        bool loop = false, streaming = false;
        DBYTE priority = 0;
        bool reverb_requested = true;
        friend bool operator==(const SoundParameters&, const SoundParameters&) = default;
    };
} // namespace OpenBlood2
