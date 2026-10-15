#pragma once
#include "basedefs_de.h"
namespace OpenBlood2
{
    // Owned equivalent of the original persistent SFX_CAMERA_ID descriptor.
    struct CameraSettings
    {
        DBYTE type = 0;
        bool player_movement = false, hide_player = false, listener = true;
    };
    struct CameraView
    {
        HOBJECT object = nullptr;
        CameraSettings settings;
        DVector position{};
        DRotation rotation{};
    };
} // namespace OpenBlood2
