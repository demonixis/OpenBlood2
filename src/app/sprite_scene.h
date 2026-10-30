#pragma once
#include "app/client_scene.h"
#include "app/model_scene.h"
#include "engine/sprite_view.h"
namespace OpenBlood2
{
    class GameSession;
    bool update_sprite_scene(const ClientScene&, SpriteView&, std::string&, const ClientCameraView* = nullptr,
                             const std::array<ViewWeaponCamera, 2>* = nullptr);
    // camera: the presentation camera when it differs from the game view (XR head).
    bool update_sprite_scene(GameSession&, SpriteView&, std::string& error, const ClientCameraView* camera = nullptr,
                             const std::array<ViewWeaponCamera, 2>* weapons = nullptr);
    bool update_sprite_scene(const GameSession&, const ClientWorld&, SpriteView&, std::string&,
                             const ClientCameraView* camera = nullptr,
                             const std::array<ViewWeaponCamera, 2>* weapons = nullptr);
} // namespace OpenBlood2
