#pragma once
#include "client/camera.h"
#include "client/effects.h"
#include "client/weapon_view.h"
#include "engine/world.h"
#include <string_view>
namespace OpenBlood2
{
    // Borrowed only for one update/record call; all inputs belong to the client.
    // Static DAT/resources are local content, dynamic state is a passive replica.
    // GPU caches remain owned by ScenePresenter and are reset by world identity.
    struct ClientScene
    {
        const World& geometry;
        const Resources& resources;
        const ClientWorld& world;
        const ClientEffects& effects;
        std::uint64_t world_revision = 0;
        std::string_view world_path{};
        ClientCameraView view{};
        DVector light_scale{1, 1, 1};
        float fov_x = 1.5707963267948966f;
        bool in_world = false, night_goggles = false;
        bool player_visible = false, weapons_visible = true;
        float pitch = 0, yaw = 0;
        DVector camera_position{};
        std::array<const ClientWeaponView*, 2> weapons{};
    };
} // namespace OpenBlood2
