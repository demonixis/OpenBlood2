#include "app/sprite_scene.h"
#include "client/effect_scene.h"
#include "client/object_scene.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool update_sprite_scene(const ClientScene& source, SpriteView& scene, std::string& error,
                             const ClientCameraView* camera, const std::array<ViewWeaponCamera, 2>* weapons)
    {
        if (source.world.frame().world != source.effects.frame().world)
        {
            error = "Client scene world/effects identity mismatch";
            return false;
        }
        const auto& world = source.world;
        const auto& view = camera ? *camera : source.view;
        if (!scene.begin_frame(view.position, view.rotation, error))
            return false;
        if (!append_client_sprites(world, scene, source.resources, error))
            return false;
        if (!append_effect_sprites(source.effects, scene, view.rotation, true, source.resources, error))
            return false;
        const auto& client = source;
        if (client.weapons_visible && world.frame().player_alive)
            for (const auto* visual : client.weapons)
            {
                if (!visual)
                    continue;
                const auto& weapon = *visual;
                if (!weapon.flash_visible || !weapon.model)
                    continue;
                const auto mode = weapon.flash_visible - 1;
                const auto* definition = weapon_definition(weapon.type);
                const auto placement = weapons
                                           ? (*weapons)[weapon.left ? 0 : 1]
                                           : ViewWeaponCamera{client.pitch, client.yaw, client.camera_position, {}, 1};
                if (!definition || mode >= weapon.flashes.size())
                {
                    error = "Invalid client muzzle flash state";
                    return false;
                }
                NodePose pose;
                float scale = placement.scale;
                if (placement.arms)
                {
                    ViewArmRig rig;
                    ViewArmPose held;
                    const auto offset = definition->m_vViewModelOffset;
                    if (!rig_view_arms(*weapon.model, weapon.model_path, weapon.left, rig) ||
                        !pose_view_arms(*weapon.model, weapon.animation, rig, *placement.arms,
                                        {weapon.left ? -offset.x : offset.x, offset.y, offset.z}, held))
                    {
                        error = "Muzzle flash arm placement failed";
                        return false;
                    }
                    pose = held.model;
                    scale = placement.arms->scale;
                }
                else
                {
                    if (!weapon.AdjustAngleAndSetPos(placement.pitch, placement.yaw, placement.eye, pose))
                        return false;
                    pose.position =
                        vector_add(placement.eye, vector_scale(vector_subtract(pose.position, placement.eye), scale));
                }
                pose.position =
                    vector_add(pose.position,
                               rotate_vector(pose.rotation, vector_scale(weapon.FlashOffset(!placement.arms), scale)));
                // Desktop x10 flash projection needs the authored .19 scale; at a real XR muzzle
                // undo that magnification before applying the model's world scale.
                float size = definition->m_fFlashScale * scale * (placement.arms ? .1f : 1.f);
                // FLAG_SPRITEBIAS: camera-directed -20-unit bias with size preserved (drawsprite).
                // Tracked XR weapons use their physical muzzle instead of desktop projection/bias.
                if (!placement.arms)
                {
                    const auto relative = vector_subtract(pose.position, view.position);
                    const float z = vector_dot(relative, rotate_vector(view.rotation, {0, 0, 1}));
                    if (z > world_near)
                    {
                        const float factor = std::max(world_near, z - 20.f) / z;
                        pose.position = vector_add(view.position, vector_scale(relative, factor));
                        size *= factor;
                    }
                }
                if (!scene.add(weapon.flashes[mode].get(), weapon.flash_tracks[mode], pose, {size, size, size},
                               FLAG_VISIBLE | FLAG_NOLIGHT, {}, source.resources, error))
                    return false;
            }
        if (!append_effect_sprites(source.effects, scene, view.rotation, false, source.resources, error))
            return false;
        scene.sort();
        return true;
    }
} // namespace OpenBlood2
