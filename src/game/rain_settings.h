#pragma once
#include "game/legacy/basedefs_de.h"
#include <cmath>

namespace OpenBlood2
{
    struct RainSettings
    {
        float density = 0, lifetime = 1, scale = 1, spread = 0, time_limit = 0, pulse = 0;
        DVector dims{}, direction{}, color1{200, 255, 255}, color2{40, 50, 50};
        bool gravity = true;
        DDWORD flags = 0;
    };

    inline bool valid_rain(const RainSettings& s)
    {
        auto bound = [](float v, float min, float max) { return std::isfinite(v) && v >= min && v <= max; };
        auto vector = [&](DVector v, float min, float max)
        { return bound(v.x, min, max) && bound(v.y, min, max) && bound(v.z, min, max); };
        return bound(s.density, 0, 10000) && vector(s.dims, 0, 10000) && vector(s.direction, -1e6f, 1e6f) &&
               bound(s.density * s.dims.x * s.dims.z / 250, 0, 100000) && bound(s.lifetime, 0, 600) &&
               bound(s.scale, 0, 3200) && bound(s.spread, 0, 10000) && bound(s.time_limit, 0, 3600) &&
               bound(s.pulse, 0, 600) && vector(s.color1, 0, 255) && vector(s.color2, 0, 255);
    }
} // namespace OpenBlood2
