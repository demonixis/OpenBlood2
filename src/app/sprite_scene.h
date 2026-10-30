#pragma once
#include "app/model_scene.h"
#include "engine/sprite_view.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    // camera: the presentation camera when it differs from the game view (XR head).
    bool update_sprite_scene(GameSession&, SpriteView&, std::string& error, const ClientCameraView* camera = nullptr,
                             const std::array<ViewWeaponCamera, 2>* weapons = nullptr);
    bool update_sprite_scene(const GameSession&, const ClientWorld&, SpriteView&, std::string&,
                             const ClientCameraView* camera = nullptr,
                             const std::array<ViewWeaponCamera, 2>* weapons = nullptr);
} // namespace OpenBlood2
