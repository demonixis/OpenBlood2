#include "app/model_scene.h"
#include "client/effect_scene.h"
#include "client/object_scene.h"
#include <algorithm>
namespace OpenBlood2
{
    bool update_explosion_model_scene(GameSession& game, ModelView& scene, std::string& error,
                                      const ClientCameraView* view)
    {
        return game.publish_client_world(error) &&
               update_explosion_model_scene(game, game.client_session.effects, scene, error, view);
    }
    bool update_explosion_model_scene(const GameSession& game, const ClientEffects& effects, ModelView& scene,
                                      std::string& error, const ClientCameraView* view)
    {
        scene.begin_frame();
        if (!append_effect_models(effects, scene, false, game.resources, error))
            return false;
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
        if (!append_effect_models(game.client_session.effects, scene, true, game.resources, error))
            return false;
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
