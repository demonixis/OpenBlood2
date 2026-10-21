#pragma once
#include "engine/model_view.h"
#include "game/server_adapter.h"
#include <array>
namespace OpenBlood2
{
    // Original view-weapon placement inputs. XR gives each hand its controller: `eye` is then the
    // grip point and the camera eye is derived behind it from the weapon's flash offset, so the
    // life-size view model's hand lands on the controller (`at_hand`).
    struct ViewWeaponCamera
    {
        float pitch = 0, yaw = 0;
        DVector eye{};
        bool at_hand = false;
    };
    // camera: the presentation camera when it differs from the game view (XR head).
    bool update_explosion_model_scene(const GameSession&, ModelView&, std::string&, const CameraView* camera = nullptr);
    // weapons: {left hand, right hand} cameras replacing the client's pitch/yaw/eye (XR).
    bool update_model_scene(GameSession&, ModelView&, std::string&,
                            const std::array<ViewWeaponCamera, 2>* weapons = nullptr);
} // namespace OpenBlood2
