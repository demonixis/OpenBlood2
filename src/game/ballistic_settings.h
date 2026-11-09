#pragma once
#include "game/original/SurfaceTypes.h"
#include "game/original/WeaponDefs.h"
#include <string>
#include <vector>
namespace OpenBlood2
{
    // Original WeaponFX payload after UpdateWeaponFX has consumed the one-shot ejection bit.
    struct ShotgunEffectSettings
    {
        DVector source{}, destination{}, forward{}, normal{}, color1{}, color2{};
        SurfaceType surface = SURFTYPE_UNKNOWN;
        bool eject = false, left = false;
        DBYTE ammo = AMMO_SHELL;
        std::vector<DVector> clip_polygon; // Copied world-space BSP ring; no retained object handle.
    };
    struct ImpactMarkSettings
    {
        DVector position{}, normal{};
        float scale = .05f;
        std::string sprite;
    };
} // namespace OpenBlood2
