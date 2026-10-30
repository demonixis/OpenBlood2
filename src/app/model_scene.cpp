#include "app/model_scene.h"
#include "client/object_scene.h"
#include <algorithm>
namespace OpenBlood2
{
    bool update_explosion_model_scene(const GameSession& game, ModelView& scene, std::string& error,
                                      const ClientCameraView* view)
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
        for (const auto& fx : game.server.destruction_debris.active)
        {
            if (!scene.add(fx.model, fx.animation, fx.skin_path, {fx.position, fx.rotation}, fx.scale, true, false, {},
                           fx.model_path, game.resources, error, {}, fx.alpha, true))
                return false;
            if (fx.alpha < 1)
                scene.mark_translucent(scene.entries.size() - 1);
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
        return game.publish_client_world(error) &&
               update_model_scene(game, game.client_session.world, scene, error, weapons);
    }
    bool update_model_scene(const GameSession& game, const ClientWorld& world, ModelView& scene, std::string& error,
                            const std::array<ViewWeaponCamera, 2>* weapons)
    {
        scene.begin_frame();
        const auto& client = game.client_shell;
        if (!append_client_models(world, client.player_visible, scene, game.resources, error))
            return false;
        for (const auto& fx : game.server.shotgun_effects.casings)
        {
            if (!scene.add(fx.model, fx.animation, fx.skin_path, {fx.position, fx.rotation},
                           {fx.scale, fx.scale, fx.scale}, fx.visible, false, {}, fx.model_path, game.resources, error,
                           {}, 1, true))
                return false;
        }
        for (const auto& weapon : client.weapons)
        {
            if (!weapon.model)
                continue;
            const auto* definition = weapon_definition(weapon.type);
            NodePose pose;
            const auto camera =
                weapons ? (*weapons)[weapon.left ? 0 : 1]
                        : ViewWeaponCamera{client.rotation.pitch, client.rotation.yaw, client.camera_position, {}, 1};
            const bool visible = world.frame().player_alive && client.weapons_visible;
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
