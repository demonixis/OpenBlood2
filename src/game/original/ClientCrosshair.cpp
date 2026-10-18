#include "game/original/ClientCrosshair.h"
namespace OpenBlood2
{
    bool original_crosshair_visible(bool in_world, bool external_camera, bool dead)
    {
        return in_world && !external_camera && !dead;
    }
    HudRect original_crosshair_rect(const HudRect& camera, unsigned width, unsigned height)
    {
        // CBloodClientShell::PostUpdate: native bitmap size and integer centering
        // within GetCameraRect (the full viewport for the current first-person view).
        const int x = camera.left + (((camera.right - camera.left) - int(width)) >> 1);
        const int y = camera.top + (((camera.bottom - camera.top) - int(height)) >> 1);
        return {x, y, x + int(width), y + int(height)};
    }
} // namespace OpenBlood2
