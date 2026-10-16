#include "app/model_scene.h"
#include <algorithm>
namespace OpenBlood2
{
    bool update_explosion_model_scene(const GameSession& game, ModelView& scene, std::string& error)
    {
        scene.begin_frame();
        for (const auto& fx : game.server.explosion_models.active)
        {
            const bool visible = fx.scale.x > 0 && fx.scale.y > 0 && fx.scale.z > 0;
            if (!scene.add(fx.model.get(), fx.animation, fx.skin_path, {fx.settings.position, fx.rotation}, fx.scale,
                           visible, false, {}, fx.model_path, game.resources, error, {}, fx.alpha, true))
                return false;
            scene.entries.back().client_effect = fx.id;
        }
        for (const auto& fx : game.server.explosion_fragments.active)
        {
            const auto scale = fx.settings.scale;
            if (!scene.add(fx.model.get(), fx.animation, fx.skin_path, {fx.position, fx.rotation}, scale,
                           scale.x > 0 && scale.y > 0 && scale.z > 0, false, {}, fx.model_path, game.resources, error,
                           {}, fx.alpha, true))
                return false;
            scene.entries.back().fragment_effect = fx.id;
        }
        const auto& camera = game.client_shell.view;
        const auto forward = rotate_vector(camera.rotation, {0, 0, 1});
        auto depth = [&](const WorldDraw& d)
        {
            return (d.position.x - camera.position.x) * forward.x + (d.position.y - camera.position.y) * forward.y +
                   (d.position.z - camera.position.z) * forward.z;
        };
        std::stable_sort(scene.world_draws.begin(), scene.world_draws.end(),
                         [&](const auto& a, const auto& b) { return depth(a) > depth(b); });
        return true;
    }
    bool update_model_scene(GameSession& game, ModelView& scene, std::string& error)
    {
        scene.begin_frame();
        auto& client = game.client_shell;
        for (auto id : game.objects.snapshot())
        {
            const auto* record = game.objects.get(id);
            if (!record || record->life != Life::Alive || record->state.m_ObjectType != OT_MODEL)
                continue;
            bool visible = (record->state.m_Flags & FLAG_VISIBLE) != 0;
            bool unresolved = false;
            if (record->attachment)
            {
                const auto relation = game.objects.attachments().find(record->attachment);
                unresolved = relation == game.objects.attachments().end() || !relation->second.resolved;
                if (unresolved)
                    visible = false;
            }
            if (!client.player_visible)
            {
                if (record->handle == client.player)
                    visible = false;
                if (record->attachment)
                {
                    const auto relation = game.objects.attachments().find(record->attachment);
                    if (relation != game.objects.attachments().end() && relation->second.parent == client.player)
                        visible = false;
                }
            }
            if (!scene.add(record->model.get(), record->animation, record->state.m_SkinName,
                           {record->state.m_Pos, record->state.m_Rotation}, record->state.m_Scale, visible, false, id,
                           record->state.m_Filename, game.resources, error, record->hidden_nodes))
                return false;
            if (unresolved)
                scene.entries.back().error = "Unresolved attachment pose";
        }
        const auto* owner = game.server_shell.Player(game.local_client);
        for (const auto& weapon : client.weapons)
        {
            if (!weapon.model)
                continue;
            const auto* definition = weapon_definition(weapon.type);
            NodePose pose;
            if (!definition ||
                !weapon.AdjustAngleAndSetPos(client.rotation.pitch, client.rotation.yaw, client.camera_position, pose))
            {
                error = "Original view-weapon placement failed";
                return false;
            }
            if (!scene.add(weapon.model.get(), weapon.animation, definition->m_szViewModelSkin, pose, {1, 1, 1},
                           owner && !owner->m_damage.IsDead() && client.weapons_visible, true, {},
                           weapon.left ? "left view weapon" : "right view weapon", game.resources, error))
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
