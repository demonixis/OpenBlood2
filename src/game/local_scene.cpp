#include "game/local_scene.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    ClientScene local_scene(const GameSession& game, const ClientWorld* world, const ClientEffects* effects)
    {
        const auto& client = game.client_shell;
        ClientScene out{game.world, game.resources, world ? *world : game.client_session.world,
                        effects ? *effects : game.client_session.effects};
        out.world_revision = game.world_revision;
        out.world_path = game.world_path;
        out.view = client.view;
        out.light_scale = client.SceneLightScale();
        out.fov_x = client.ViewFovX();
        out.in_world = client.m_bInWorld;
        out.night_goggles = client.night_goggles;
        out.player_visible = client.player_visible;
        out.weapons_visible = client.weapons_visible;
        out.pitch = client.rotation.pitch;
        out.yaw = client.rotation.yaw;
        out.camera_position = client.camera_position;
        out.weapons = {&client.weapons[0], &client.weapons[1]};
        return out;
    }
} // namespace OpenBlood2
