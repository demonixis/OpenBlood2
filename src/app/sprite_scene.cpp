#include "app/sprite_scene.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool update_sprite_scene(const GameSession& game, SpriteView& scene, std::string& error, const CameraView* camera,
                             const std::array<ViewWeaponCamera, 2>* weapons)
    {
        const auto& view = camera ? *camera : game.client_shell.view;
        if (!scene.begin_frame(view.position, view.rotation, error))
            return false;
        for (auto id : game.objects.snapshot())
        {
            const auto* record = game.objects.get(id);
            if (!record || record->life != Life::Alive || record->state.m_ObjectType != OT_SPRITE ||
                (record->state.m_Flags & FLAG_SKYOBJECT))
                continue;
            if (!scene.add(record->sprite.get(), record->sprite_track, {record->state.m_Pos, record->state.m_Rotation},
                           record->state.m_Scale, record->state.m_Flags, id, game.resources, error, record->alpha,
                           record->color))
                return false;
        }
        for (const auto& fx : game.server.explosion_sprites.active)
        {
            if (!scene.add(fx.sprite.get(), fx.track, {fx.settings.position, fx.rotation}, fx.scale, fx.flags, {},
                           game.resources, error, fx.alpha))
                return false;
            scene.entries.back().client_effect = fx.id;
        }
        for (const auto& gate : game.server.warp_gates.active)
        {
            if (gate.sprite_ready)
            {
                const auto& fx = gate.sprite;
                auto pose = fx.pose;
                DDWORD flags = FLAG_VISIBLE | FLAG_SPRITECHROMAKEY | (fx.settings.align ? FLAG_ROTATEABLESPRITE : 0);
                // Two negative sprite axes are exactly a half-turn about its normal.
                if (fx.scale < 0)
                {
                    DRotation roll;
                    roll.m_Vec = {0, 0, 1};
                    roll.m_Spin = 0;
                    pose.rotation = compose_rotation(fx.settings.align ? pose.rotation : view.rotation, roll);
                    flags |= FLAG_ROTATEABLESPRITE;
                }
                const float scale = std::abs(fx.scale);
                if (!scene.add(fx.sprite.get(), fx.track, pose, {scale, scale, 0}, flags, {}, game.resources, error,
                               fx.alpha))
                    return false;
                scene.entries.back().warp_gate = gate.id;
            }
            for (unsigned j = 0; j < 2; ++j)
                if (gate.particle_ready[j])
                {
                    const auto& fx = gate.particles[j];
                    for (unsigned i = 0; i < fx.particles.size(); ++i)
                    {
                        const auto& p = fx.particles[i];
                        if (!scene.add_particle(
                                fx.texture, vector_add(fx.pose.position, rotate_vector(fx.pose.rotation, p.position)),
                                fx.settings.radius / 640.f, p.color, fx.alpha, gate.id, i, game.resources, error))
                            return false;
                        scene.entries.back().particle_system = 0;
                        scene.entries.back().warp_gate = gate.id;
                    }
                }
        }
        for (const auto& bolt : game.server.lightning.active)
            for (const auto& segment : bolt.segments)
                for (unsigned i = 0; i < segment.particles.size(); ++i)
                {
                    if (!scene.add_particle(bolt.texture, segment.particles[i].position, segment.radius / 640.f,
                                            segment.color, bolt.alpha, bolt.id, i, game.resources, error))
                        return false;
                    scene.entries.back().particle_system = 0;
                    scene.entries.back().lightning_bolt = bolt.id;
                }
        for (const auto& fx : game.server.sparks.active)
            for (unsigned i = 0; i < fx.particles.size(); ++i)
            {
                const auto& p = fx.particles[i];
                if (!scene.add_particle(fx.settings.texture, vector_add(fx.settings.position, p.position),
                                        fx.settings.radius / 640.f, p.color, std::max(0.f, fx.alpha), fx.id, i,
                                        game.resources, error))
                    return false;
                scene.entries.back().particle_system = 0;
                scene.entries.back().spark_system = fx.id;
            }
        // Blood drops reach thousands after a gib death: one entry and one draw per spurt.
        for (const auto& fx : game.server.blood_spurts.active)
        {
            if (fx.particles.empty())
                continue;
            if (!scene.add_particle_system(fx.texture, fx.settings.position, fx.particles, 200.f / 640, fx.alpha, fx.id,
                                           game.resources, error))
                return false;
            scene.entries.back().particle_system = 0;
            scene.entries.back().blood_spurt = fx.id;
        }
        for (const auto& fx : game.server.blood_splats.active)
        {
            if (!scene.add(fx.sprite.get(), fx.track, {fx.position, fx.rotation}, fx.scale,
                           FLAG_VISIBLE | FLAG_ROTATEABLESPRITE, {}, game.resources, error, fx.alpha, {.1f, .1f, .1f}))
                return false;
            scene.entries.back().blood_splat = fx.id;
        }
        const auto& client = game.client_shell;
        const auto* record = game.objects.get(client.player);
        const auto* player = record ? dynamic_cast<const CPlayerObj*>(record->game.get()) : nullptr;
        if (client.weapons_visible && player && !player->m_damage.IsDead())
            for (const auto& weapon : client.weapons)
            {
                if (!weapon.flash_visible || !weapon.model)
                    continue;
                const auto mode = weapon.flash_visible - 1;
                const auto* definition = weapon_definition(weapon.type);
                const auto placement =
                    weapons
                        ? (*weapons)[weapon.left ? 0 : 1]
                        : ViewWeaponCamera{client.rotation.pitch, client.rotation.yaw, client.camera_position, {}, 1};
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
                               FLAG_VISIBLE | FLAG_NOLIGHT, {}, game.resources, error))
                    return false;
            }
        for (const auto& fx : game.server.shotgun_effects.sprites)
        {
            if (!scene.add(fx.sprite.get(), fx.track, {fx.position, fx.rotation}, fx.scale,
                           FLAG_VISIBLE | FLAG_ROTATEABLESPRITE |
                               (fx.kind == ShotgunSpriteKind::Mark ? 0 : FLAG_SPRITECHROMAKEY),
                           {}, game.resources, error, fx.alpha, fx.color))
                return false;
            if (fx.kind == ShotgunSpriteKind::Mark && !fx.clip_polygon.empty() &&
                !scene.clip_last(fx.clip_polygon, error))
                return false;
        }
        for (const auto& fx : game.server.shotgun_effects.splashes)
            if (!scene.add_particle_system("SpriteTextures/drop32_1.dtx", fx.position, fx.particles, 100.f / 640, 1,
                                           fx.id, game.resources, error))
                return false;
        for (const auto& fx : game.server.weapon_sprites.flashes)
        {
            if (!scene.add(fx.sprite.get(), fx.track, {fx.settings.position, fx.rotation}, fx.scale, fx.flags, {},
                           game.resources, error, fx.alpha))
                return false;
            scene.entries.back().weapon_effect = fx.id;
        }
        for (const auto& fx : game.server.weapon_sprites.smoke)
        {
            if (!scene.add(fx.sprite.get(), fx.track, {fx.position, {}}, fx.scale, fx.flags, {}, game.resources, error,
                           fx.alpha, fx.color))
                return false;
            scene.entries.back().weapon_effect = fx.id;
        }
        for (const auto& fx : game.server.explosion_rings.active)
            for (unsigned i = 0; i < fx.particles.size(); ++i)
            {
                auto local = rotate_vector(fx.rotation, fx.particles[i].position);
                const DVector position{fx.settings.position.x + local.x, fx.settings.position.y + local.y,
                                       fx.settings.position.z + local.z};
                if (!scene.add_particle(fx.texture, position, fx.settings.radius / 640.f, fx.settings.color, fx.alpha,
                                        fx.id, i, game.resources, error))
                    return false;
            }
        for (const auto& trail : game.server.explosion_fragments.trails)
            for (unsigned i = 0; i < trail.particles.size(); ++i)
            {
                const auto& p = trail.particles[i];
                if (!scene.add_particle(trail.texture, vector_add(trail.origin, p.position), trail.radius / 640.f,
                                        p.color, trail.alpha, trail.id, i, game.resources, error))
                    return false;
                scene.entries.back().particle_system = 0;
                scene.entries.back().trail_system = trail.id;
            }
        for (const auto& fx : game.server.explosion_flames.active)
            for (unsigned i = 0; i < fx.particles.size(); ++i)
            {
                const auto& p = fx.particles[i];
                if (!scene.add_particle(fx.texture, vector_add(fx.settings.position, p.position),
                                        fx.settings.radius / 640.f, p.color, fx.alpha, fx.id, i, game.resources, error))
                    return false;
                scene.entries.back().particle_system = 0;
                scene.entries.back().flame_system = fx.id;
            }
        for (const auto& instance : game.server.object_particles.active)
        {
            const auto& fx = instance.effect;
            for (unsigned i = 0; i < fx.particles.size(); ++i)
            {
                const auto& particle = fx.particles[i];
                // MOVINGSOURCE keeps the particle-system transform at its creation origin;
                // particle positions already include the owner's displacement.
                const auto origin = (fx.flags & 0x04) ? fx.origin : fx.position;
                const auto position = vector_add(origin, rotate_vector(fx.system_rotation, particle.position));
                if (!scene.add_particle(fx.texture, position, fx.radius / 640.f, particle.color, fx.alpha, fx.id, i,
                                        game.resources, error))
                    return false;
                scene.entries.back().particle_system = 0;
                scene.entries.back().object_effect = fx.id;
            }
        }
        for (const auto& instance : game.server.particle_streams.active)
        {
            if (!game.objects.get(instance.owner) || game.objects.get(instance.owner)->life != Life::Alive)
                continue;
            const auto& fx = instance.effect;
            if (fx.settings.system && !fx.system_visible)
                continue;
            for (unsigned i = 0; i < fx.particles.size(); ++i)
            {
                const auto& p = fx.particles[i];
                if (!scene.add_particle(fx.texture,
                                        vector_add(fx.position, rotate_vector(fx.system_rotation, p.position)),
                                        fx.settings.radius / 640.f, p.color, fx.alpha, fx.id, i, game.resources, error))
                    return false;
                scene.entries.back().particle_system = 0;
                scene.entries.back().stream_system = fx.id;
            }
        }
        scene.sort();
        return true;
    }
} // namespace OpenBlood2
