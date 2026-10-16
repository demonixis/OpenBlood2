#pragma once
#include <cstdint>
// Original Shared/SharedDefs.h surface identifiers. DAT texture flags are distinct
// from polygon render/collision flags; unknown identifiers are not guessed.
enum SurfaceType : std::uint32_t
{
    SURFTYPE_UNKNOWN = 0,
    SURFTYPE_STONE = 10,
    SURFTYPE_METAL = 20,
    SURFTYPE_WOOD = 30,
    SURFTYPE_ENERGY = 40,
    SURFTYPE_GLASS = 50,
    SURFTYPE_BUILDING = 60,
    SURFTYPE_TERRAIN = 70,
    SURFTYPE_CLOTH = 80,
    SURFTYPE_PLASTIC = 90,
    SURFTYPE_FLESH = 100,
    SURFTYPE_SKY = 110,
    SURFTYPE_FIRETHROUGH = 120,
    SURFTYPE_LIQUID = 200,
    SURFTYPE_MAX
};
