#pragma once
#include "engine/model_view.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    // Original view-weapon placement inputs; XR replaces the client's pitch/yaw/eye with the head's.
    struct ViewWeaponCamera
    {
        float pitch = 0, yaw = 0;
        DVector eye{};
    };
    // camera: the presentation camera when it differs from the game view (XR head).
    bool update_explosion_model_scene(const GameSession&, ModelView&, std::string&, const CameraView* camera = nullptr);
    bool update_model_scene(GameSession&, ModelView&, std::string&, const ViewWeaponCamera* weapons = nullptr);
} // namespace OpenBlood2
