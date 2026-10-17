#include "app/sprite_scene.h"
namespace OpenBlood2
{
    bool update_sprite_scene(const GameSession& game, SpriteView& scene, std::string& error)
    {
        const auto& view = game.client_shell.view;
        if (!scene.begin_frame(view.position, view.rotation, error))
            return false;
        for (auto id : game.objects.snapshot())
        {
            const auto* record = game.objects.get(id);
            if (!record || record->life != Life::Alive || record->state.m_ObjectType != OT_SPRITE)
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
        for (const auto& fx : game.server.blood_spurts.active)
            for (unsigned i = 0; i < fx.particles.size(); ++i)
            {
                const auto& p = fx.particles[i];
                if (!scene.add_particle(fx.texture, vector_add(fx.settings.position, p.position), 200.f / 640, p.color,
                                        fx.alpha, fx.id, i, game.resources, error))
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
                const auto position = vector_add(fx.position, rotate_vector(fx.system_rotation, particle.position));
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
            for (unsigned i = 0; i < fx.particles.size(); ++i)
            {
                const auto& p = fx.particles[i];
                if (!scene.add_particle(fx.texture, vector_add(fx.position, p.position), fx.settings.radius / 640.f,
                                        p.color, fx.settings.alpha, fx.id, i, game.resources, error))
                    return false;
                scene.entries.back().particle_system = 0;
                scene.entries.back().stream_system = fx.id;
            }
        }
        scene.sort();
        return true;
    }
} // namespace OpenBlood2
