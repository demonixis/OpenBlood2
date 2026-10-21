#include "app/model_scene.h"
#include <algorithm>
namespace OpenBlood2
{
    bool update_explosion_model_scene(const GameSession& game, ModelView& scene, std::string& error,
                                      const CameraView* view)
    {
        scene.begin_frame();
        for (const auto& fx : game.server.explosion_models.active)
        {
            const bool visible = fx.scale.x > 0 && fx.scale.y > 0 && fx.scale.z > 0;
            if (!scene.add(fx.model, fx.animation, fx.skin_path, {fx.settings.position, fx.rotation}, fx.scale, visible,
                           false, {}, fx.model_path, game.resources, error, {}, fx.alpha, true))
                return false;
            scene.entries.back().client_effect = fx.id;
        }
        for (const auto& fx : game.server.explosion_fragments.active)
        {
            const auto scale = fx.settings.scale;
            if (!scene.add(fx.model, fx.animation, fx.skin_path, {fx.position, fx.rotation}, scale,
                           scale.x > 0 && scale.y > 0 && scale.z > 0, false, {}, fx.model_path, game.resources, error,
                           {}, fx.alpha, true))
                return false;
            scene.entries.back().fragment_effect = fx.id;
        }
        for (const auto& fx : game.server.flesh_gibs.active)
        {
            if (!scene.add(fx.model, fx.animation, fx.skin_path, {fx.position, fx.rotation}, fx.scale, true, false, {},
                           fx.model_path, game.resources, error))
                return false;
        }
        for (const auto& fx : game.server.laser_beams.active)
        {
            if (!scene.add(fx.model, fx.animation, fx.skin_path, {fx.position, fx.rotation}, fx.scale, fx.updates != 0,
                           false, {}, "Original laser beam", game.resources, error, {}, fx.alpha, true))
                return false;
            scene.entries.back().laser_effect = fx.id;
        }
        const auto& camera = view ? *view : game.client_shell.view;
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
    bool update_model_scene(GameSession& game, ModelView& scene, std::string& error,
                            const std::array<ViewWeaponCamera, 2>* weapons)
    {
        scene.begin_frame();
        auto& client = game.client_shell;
        for (auto id : game.objects.snapshot())
        {
            const auto* record = game.objects.get(id);
            if (!record || record->life != Life::Alive || record->state.m_ObjectType != OT_MODEL)
                continue;
            bool visible = (record->state.m_Flags & FLAG_VISIBLE) != 0;
            if (record->handle == client.seeing_eye || record->handle == client.orb_view)
                visible = false; // The camera is inside this model; do not draw its interior.
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
            if (!scene.add(record->model, record->animation, record->state.m_SkinName,
                           {record->state.m_Pos, record->state.m_Rotation}, record->state.m_Scale, visible,
                           (record->state.m_Flags & FLAG_REALLYCLOSE) != 0, id, record->state.m_Filename,
                           game.resources, error, record->hidden_nodes, record->alpha))
                return false;
            if (record->alpha < 1)
                scene.mark_translucent(scene.entries.size() - 1);
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
            const auto camera =
                weapons ? (*weapons)[weapon.left ? 0 : 1]
                        : ViewWeaponCamera{client.rotation.pitch, client.rotation.yaw, client.camera_position, {}, 1};
            const bool visible = owner && !owner->m_damage.IsDead() && client.weapons_visible;
            const auto* label = weapon.left ? "left view weapon" : "right view weapon";
            if (definition && camera.arms)
            {
                const auto& offset = definition->m_vViewModelOffset;
                ViewArmRig rig;
                ViewArmPose held;
                if (!rig_view_arms(*weapon.model, weapon.model_path, weapon.left, rig) ||
                    !pose_view_arms(*weapon.model, weapon.animation, rig, *camera.arms,
                                    {weapon.left ? -offset.x : offset.x, offset.y, offset.z}, held))
                {
                    error = "View-weapon arm placement failed";
                    return false;
                }
                const auto s = camera.arms->scale;
                if (!scene.add(weapon.model, weapon.animation, weapon.skin_path, held.model, {s, s, s}, visible, true,
                               {}, label, game.resources, error, held.hidden, 1, false, held.targets))
                    return false;
                continue;
            }
            if (!definition || !weapon.AdjustAngleAndSetPos(camera.pitch, camera.yaw, camera.eye, pose))
            {
                error = "Original view-weapon placement failed";
                return false;
            }
            const auto s = camera.scale;
            if (s != 1)
                pose.position = vector_add(camera.eye, vector_scale(vector_subtract(pose.position, camera.eye), s));
            if (!scene.add(weapon.model, weapon.animation, weapon.skin_path, pose, {s, s, s}, visible, true, {}, label,
                           game.resources, error))
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
