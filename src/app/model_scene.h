#pragma once
#include "engine/model_view.h"
#include "engine/view_arms.h"
#include "game/server_adapter.h"
#include <array>
#include <optional>
namespace OpenBlood2
{
    // Original view-weapon placement inputs. XR gives each tracked hand `arms`: the model is then
    // held at its controller at `view_model_meters` scale, its arms solved from virtual shoulders.
    // Otherwise the original placement is enlarged about the eye by `scale` (XR's untracked hand).
    struct ViewWeaponCamera
    {
        float pitch = 0, yaw = 0;
        DVector eye{};
        std::optional<ViewArmPlacement> arms;
        float scale = 1;
    };
    // camera: the presentation camera when it differs from the game view (XR head).
    bool update_explosion_model_scene(const GameSession&, ModelView&, std::string&, const CameraView* camera = nullptr);
    // weapons: {left hand, right hand} cameras replacing the client's pitch/yaw/eye (XR).
    bool update_model_scene(GameSession&, ModelView&, std::string&,
                            const std::array<ViewWeaponCamera, 2>* weapons = nullptr);
} // namespace OpenBlood2
