#include "app/model_scene.h"
#include "client/effect_scene.h"
#include "client/object_scene.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
namespace OpenBlood2
{
    bool update_explosion_model_scene(const ClientScene& source, ModelView& scene, std::string& error,
                                      const ClientCameraView* view)
    {
        if (source.world.frame().world != source.effects.frame().world)
        {
            error = "Client scene world/effects identity mismatch";
            return false;
        }
        scene.begin_frame();
        if (!append_effect_models(source.effects, scene, false, source.resources, error))
            return false;
        const auto& camera = view ? *view : source.view;
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
    bool update_model_scene(const ClientScene& source, ModelView& scene, std::string& error,
                            const std::array<ViewWeaponCamera, 2>* weapons)
    {
        if (source.world.frame().world != source.effects.frame().world)
        {
            error = "Client scene world/effects identity mismatch";
            return false;
        }
        scene.begin_frame();
        const auto& client = source;
        const auto& world = source.world;
        if (!append_client_models(world, client.player_visible, scene, source.resources, error))
            return false;
        if (!append_effect_models(source.effects, scene, true, source.resources, error))
            return false;
        for (const auto* visual : client.weapons)
        {
            if (!visual)
                continue;
            const auto& weapon = *visual;
            if (!weapon.model)
                continue;
            const auto* definition = weapon_definition(weapon.type);
            NodePose pose;
            const auto camera = weapons ? (*weapons)[weapon.left ? 0 : 1]
                                        : ViewWeaponCamera{client.pitch, client.yaw, client.camera_position, {}, 1};
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
                               {}, label, source.resources, error, held.hidden, 1, false, held.targets))
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
                           source.resources, error))
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
