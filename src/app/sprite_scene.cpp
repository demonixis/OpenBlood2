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
                           record->state.m_Scale, record->state.m_Flags, id, game.resources, error))
                return false;
        }
        for (const auto& fx : game.server.explosion_sprites.active)
        {
            if (!scene.add(fx.sprite.get(), fx.track, {fx.settings.position, fx.rotation}, fx.scale, fx.flags, {},
                           game.resources, error, fx.alpha))
                return false;
            scene.entries.back().client_effect = fx.id;
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
        scene.sort();
        return true;
    }
} // namespace OpenBlood2
