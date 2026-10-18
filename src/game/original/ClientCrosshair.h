#pragma once
#include "engine/hud_surface.h"
namespace OpenBlood2
{
    // Selected CBloodClientShell::PostUpdate presentation rules. The supported
    // single-player view has no seeing-eye/orb, frag bar or transition third-person mode.
    inline constexpr const char* original_crosshair_bitmap = "interface/crosshairs/crosshair01.pcx";
    bool original_crosshair_visible(bool in_world, bool external_camera, bool dead);
    HudRect original_crosshair_rect(const HudRect& camera, unsigned width, unsigned height);
} // namespace OpenBlood2
