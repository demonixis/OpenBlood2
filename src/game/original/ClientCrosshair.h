#pragma once
#include "basedefs_de.h"
#include "engine/hud_surface.h"
#include <string>
namespace OpenBlood2
{
    // Selected CBloodClientShell::PostUpdate presentation rules. The supported
    // single-player view has no seeing-eye/orb, frag bar or transition third-person mode.
    inline constexpr const char* original_crosshair_bitmap = "interface/crosshairs/crosshair01.pcx";
    // PostUpdate file name for m_nCrosshair (two digits below 10; zoom types 98/99 included).
    std::string original_crosshair_path(DBYTE number);
    // Original CBloodClientShell FOV_NORMAL / FOV_ZOOMED, in radians.
    inline constexpr float original_fov_normal = 1.5707963267948966f, original_fov_zoomed = 0.21816615649929116f;
    // Projection scale for a horizontal FOV relative to the 90-degree presentation default.
    float original_fov_scale(float fov_x);
    bool original_crosshair_visible(bool in_world, bool external_camera, bool dead);
    HudRect original_crosshair_rect(const HudRect& camera, unsigned width, unsigned height);
} // namespace OpenBlood2
