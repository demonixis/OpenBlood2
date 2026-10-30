#pragma once
#include "game/legacy/basedefs_de.h"
namespace OpenBlood2
{
    // Owned equivalent of the original persistent SFX_CAMERA_ID descriptor.
    struct CameraSettings
    {
        DBYTE type = 0;
        bool player_movement = false, hide_player = false, listener = true;
        bool operator==(const CameraSettings&) const = default;
    };
} // namespace OpenBlood2
