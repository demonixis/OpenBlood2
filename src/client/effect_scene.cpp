#include "client/effect_scene.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    namespace
    {
        void tag(SpriteViewEntry& e, EffectId id)
        {
            switch (id.kind)
            {
                case EffectKind::ExplosionSprite:
                    e.client_effect = id.value;
                    break;
                case EffectKind::WarpSprite:
                case EffectKind::WarpParticles:
                    e.particle_system = 0;
                    e.warp_gate = id.value;
                    break;
                case EffectKind::Lightning:
                    e.particle_system = 0;
                    e.lightning_bolt = id.value;
                    break;
                case EffectKind::Sparks:
                    e.particle_system = 0;
                    e.spark_system = id.value;
                    break;
                case EffectKind::BloodSpurt:
                    e.particle_system = 0;
                    e.blood_spurt = id.value;
                    break;
                case EffectKind::BloodSplat:
                    e.blood_splat = id.value;
                    break;
                case EffectKind::WeaponFlash:
                case EffectKind::WeaponSmoke:
                    e.weapon_effect = id.value;
                    break;
                case EffectKind::ExplosionTrail:
                    e.particle_system = 0;
                    e.trail_system = id.value;
                    break;
                case EffectKind::ExplosionFlame:
                    e.particle_system = 0;
                    e.flame_system = id.value;
                    break;
                case EffectKind::ObjectParticles:
                    e.particle_system = 0;
                    e.object_effect = id.value;
                    break;
                case EffectKind::ParticleStream:
                    e.particle_system = 0;
                    e.stream_system = id.value;
                    break;
                default:
                    break;
            }
        }
    } // namespace
    bool append_effect_sprites(const ClientEffects& effects, SpriteView& scene, DRotation camera, bool before_muzzle,
                               const Resources& resources, std::string& error)
    {
        for (const auto& item : effects.frame().billboards)
        {
            if ((item.id.kind <= EffectKind::BloodSplat) != before_muzzle)
                continue;
            if (const auto* s = std::get_if<EffectSprite>(&item.value))
            {
                auto pose = s->pose;
                if (s->camera_half_turn)
                {
                    DRotation roll;
                    roll.m_Vec = {0, 0, 1};
                    roll.m_Spin = 0;
                    pose.rotation = compose_rotation(camera, roll);
                }
                if (!scene.add(s->sprite.get(), s->track, pose, s->scale, s->flags, {}, resources, error, s->alpha,
                               s->color))
                    return false;
                tag(scene.entries.back(), item.id);
                if (!s->clip_polygon.empty() && !scene.clip_last(s->clip_polygon, error))
                    return false;
            }
            else
            {
                const auto& p = std::get<EffectParticles>(item.value);
                if (p.batched)
                {
                    if (!scene.add_particle_system(p.texture, p.origin, p.particles, p.half_extent, p.alpha,
                                                   item.id.value, resources, error))
                        return false;
                    tag(scene.entries.back(), item.id);
                }
                else
                    for (unsigned i = 0; i < p.particles.size(); ++i)
                    {
                        const auto& particle = p.particles[i];
                        if (!scene.add_particle(p.texture, particle.position, p.half_extent, particle.color, p.alpha,
                                                item.id.value, i, resources, error))
                            return false;
                        tag(scene.entries.back(), item.id);
                    }
            }
        }
        return true;
    }
    bool append_effect_models(const ClientEffects& effects, ModelView& scene, bool casings, const Resources& resources,
                              std::string& error)
    {
        for (const auto& fx : effects.frame().models)
        {
            if ((fx.id.kind == EffectKind::Casing) != casings)
                continue;
            if (!scene.add(fx.model, fx.animation, fx.skin, fx.pose, fx.scale, fx.visible, false, {}, fx.label,
                           resources, error, {}, fx.alpha, fx.require_skin))
                return false;
            if (fx.translucent)
                scene.mark_translucent(scene.entries.size() - 1);
            auto& e = scene.entries.back();
            if (fx.id.kind == EffectKind::ExplosionModel)
                e.client_effect = fx.id.value;
            if (fx.id.kind == EffectKind::ExplosionFragment)
                e.fragment_effect = fx.id.value;
            if (fx.id.kind == EffectKind::Laser)
                e.laser_effect = fx.id.value;
        }
        return true;
    }
    bool append_effect_polygrids(const ClientEffects& effects, PolyGridView& scene, const Resources& resources,
                                 std::string& error)
    {
        for (const auto& p : effects.frame().polygrids)
        {
            if (!p.error.empty())
            {
                error = p.error;
                return false;
            }
            if (!scene.add(p.id.value, p.visible, p.pose, p.dimensions, p.width, p.heights, p.palette, p.scale, p.pan,
                           p.alpha, p.texture, resources, error))
                return false;
        }
        return true;
    }
    SceneEffects gather_scene_effects(const ClientEffects& effects, DVector eye, DRotation view)
    {
        const auto& frame = effects.frame();
        SceneEffects out;
        out.lights = frame.lights;
        out.fog_lights_unsupported = frame.fog_lights_unsupported;
        out.shadow_lights_unshadowed = frame.shadow_lights_unshadowed;
        auto fog = [&](const EffectFog& f)
        {
            const float near = std::trunc(f.near), far = std::trunc(f.far);
            out.uniform.fog_color = {};
            if (!std::isfinite(near) || !std::isfinite(far) || far <= near || !physics_vector(f.color))
                out.invalid_fog = true;
            else
            {
                auto channel = [](float c) { return std::clamp(std::trunc(c), 0.f, 255.f) / 255.f; };
                out.uniform.fog_color = {channel(f.color.x), channel(f.color.y), channel(f.color.z), 1};
                out.uniform.fog_eye_near = {eye.x, eye.y, eye.z, near};
                const auto forward = rotate_vector(view, {0, 0, 1});
                out.uniform.fog_forward_far = {forward.x, forward.y, forward.z, far};
            }
        };
        if (frame.fog.enabled)
            fog(frame.fog);
        for (const auto& volume : frame.volumes)
        {
            if (!volume.shape->contains(eye))
                continue;
            if (volume.visible && volume.fog.enabled)
            {
                out.uniform.fog_color = {};
                out.invalid_fog = false;
                fog(volume.fog);
            }
            break;
        }
        return out;
    }
} // namespace OpenBlood2
