#include "game/original/ClientCrosshair.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace OpenBlood2
{
    bool original_crosshair_visible(bool in_world, bool external_camera, bool dead)
    {
        return in_world && !external_camera && !dead;
    }
    std::string original_crosshair_path(DBYTE number)
    {
        char path[64];
        std::snprintf(path, sizeof path, "interface/crosshairs/crosshair%02u.pcx", unsigned(number));
        return path;
    }
    float original_fov_scale(float fov_x)
    {
        if (!std::isfinite(fov_x) || fov_x <= 0 || fov_x >= 3.1f)
            return 1;
        return 1 / std::tan(fov_x / 2);
    }
    float presented_fov_x(float view, float user)
    {
        if (!std::isfinite(view) || view <= 0 || !std::isfinite(user) || user <= original_fov_zoomed || user >= 3.f ||
            user == original_fov_normal)
            return view;
        if (view <= original_fov_zoomed)
            return view;
        if (view == original_fov_normal)
            return user;
        if (view <= original_fov_normal)
            return original_fov_zoomed + (view - original_fov_zoomed) * (user - original_fov_zoomed) /
                                             (original_fov_normal - original_fov_zoomed);
        return std::min(view + (user - original_fov_normal), 3.f);
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
