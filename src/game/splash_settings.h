#pragma once
#include "game/legacy/basedefs_de.h"
#include <string>

namespace OpenBlood2
{
    struct SplashSettings
    {
        DVector position{}, direction{0, 1, 0}, color1{}, color2{}, ripple_scale{};
        float radius = 100, position_radius = 1, height = 175, density = 4, spread = 10;
        float spray_time = .3f, duration = .75f, gravity = -500;
        bool ripple = false;
        std::string texture = "SpriteTextures/drop32_1.dtx";
    };
} // namespace OpenBlood2
