#include "client/effect_snapshot.h"
#include "client/original_effects.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    void append_runtime_effects(const OriginalEffects& runtime, const ClientWorld& world, ClientEffectsFrame& frame)
    {
        auto sprite = [&](EffectKind kind, std::uint64_t id, std::shared_ptr<const Sprite> asset, SpriteTrack track,
                          NodePose pose, DVector scale, DDWORD flags, float alpha = 1,
                          DVector color = DVector{1, 1, 1}) -> EffectSprite&
        {
            EffectSprite s;
            s.sprite = std::move(asset);
            s.track = track;
            s.pose = pose;
            s.scale = scale;
            s.flags = flags;
            s.alpha = alpha;
            s.color = color;
            frame.billboards.push_back({{kind, id}, std::move(s)});
            return std::get<EffectSprite>(frame.billboards.back().value);
        };
        auto particles = [&](EffectKind kind, std::uint64_t id, std::uint32_t part, std::string texture, float extent,
                             float alpha) -> EffectParticles&
        {
            EffectParticles p;
            p.texture = std::move(texture);
            p.half_extent = extent;
            p.alpha = alpha;
            frame.billboards.push_back({{kind, id, part}, std::move(p)});
            return std::get<EffectParticles>(frame.billboards.back().value);
        };
        for (const auto& fx : runtime.explosion_sprites.active)
            sprite(EffectKind::ExplosionSprite, fx.id, fx.sprite, fx.track, {fx.settings.position, fx.rotation},
                   fx.scale, fx.flags, fx.alpha);
        for (const auto& gate : runtime.warp_gates.active)
        {
            if (gate.sprite_ready)
            {
                const auto& fx = gate.sprite;
                DDWORD flags = FLAG_VISIBLE | FLAG_SPRITECHROMAKEY | (fx.settings.align ? FLAG_ROTATEABLESPRITE : 0);
                auto pose = fx.pose;
                bool camera_half_turn = false;
                if (fx.scale < 0)
                {
                    DRotation roll;
                    roll.m_Vec = {0, 0, 1};
                    roll.m_Spin = 0;
                    if (fx.settings.align)
                        pose.rotation = compose_rotation(pose.rotation, roll);
                    else
                        camera_half_turn = true;
                    flags |= FLAG_ROTATEABLESPRITE;
                }
                const float scale = std::abs(fx.scale);
                sprite(EffectKind::WarpSprite, gate.id, fx.sprite, fx.track, pose, {scale, scale, 0}, flags, fx.alpha)
                    .camera_half_turn = camera_half_turn;
            }
            for (unsigned j = 0; j < 2; ++j)
                if (gate.particle_ready[j])
                {
                    const auto& fx = gate.particles[j];
                    auto& out = particles(EffectKind::WarpParticles, gate.id, j, fx.texture, fx.settings.radius / 640.f,
                                          fx.alpha);
                    out.particles.reserve(fx.particles.size());
                    for (const auto& p : fx.particles)
                        out.particles.push_back(
                            {vector_add(fx.pose.position, rotate_vector(fx.pose.rotation, p.position)),
                             {},
                             0,
                             p.color});
                }
        }
        for (const auto& bolt : runtime.lightning.active)
            for (unsigned j = 0; j < bolt.segments.size(); ++j)
            {
                const auto& segment = bolt.segments[j];
                auto& out =
                    particles(EffectKind::Lightning, bolt.id, j, bolt.texture, segment.radius / 640.f, bolt.alpha);
                out.particles.reserve(segment.particles.size());
                for (const auto& p : segment.particles)
                    out.particles.push_back({p.position, {}, 0, segment.color});
            }
        for (const auto& fx : runtime.sparks.active)
        {
            auto& out = particles(EffectKind::Sparks, fx.id, 0, fx.settings.texture, fx.settings.radius / 640.f,
                                  std::max(0.f, fx.alpha));
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
                out.particles.push_back({vector_add(fx.settings.position, p.position), {}, 0, p.color});
        }
        for (const auto& fx : runtime.blood_spurts.active)
            if (!fx.particles.empty())
            {
                auto& out = particles(EffectKind::BloodSpurt, fx.id, 0, fx.texture, 200.f / 640, fx.alpha);
                out.origin = fx.settings.position;
                out.particles = fx.particles;
                out.batched = true;
            }
        for (const auto& fx : runtime.blood_splats.active)
            sprite(EffectKind::BloodSplat, fx.id, fx.sprite, fx.track, {fx.position, fx.rotation}, fx.scale,
                   FLAG_VISIBLE | FLAG_ROTATEABLESPRITE, fx.alpha, {.1f, .1f, .1f});
        // The local view-weapon muzzle is inserted here by the client scene consumer.
        for (const auto& fx : runtime.shotgun_effects.sprites)
        {
            auto& out = sprite(
                EffectKind::BallisticSprite, fx.id, fx.sprite, fx.track, {fx.position, fx.rotation}, fx.scale,
                FLAG_VISIBLE | FLAG_ROTATEABLESPRITE | (fx.kind == ShotgunSpriteKind::Mark ? 0 : FLAG_SPRITECHROMAKEY),
                fx.alpha, fx.color);
            if (fx.kind == ShotgunSpriteKind::Mark)
                out.clip_polygon = fx.clip_polygon;
        }
        for (const auto& fx : runtime.shotgun_effects.splashes)
        {
            auto& out = particles(EffectKind::BallisticSplash, fx.id, 0, fx.settings.texture, fx.settings.radius / 640, 1);
            out.origin = fx.settings.position;
            out.particles = fx.particles;
            out.batched = true;
        }
        for (const auto& fx : runtime.weapon_sprites.flashes)
            sprite(EffectKind::WeaponFlash, fx.id, fx.sprite, fx.track, {fx.settings.position, fx.rotation}, fx.scale,
                   fx.flags, fx.alpha);
        for (const auto& fx : runtime.weapon_sprites.smoke)
            sprite(EffectKind::WeaponSmoke, fx.id, fx.sprite, fx.track, {fx.position, {}}, fx.scale, fx.flags, fx.alpha,
                   fx.color);
        for (const auto& fx : runtime.explosion_rings.active)
        {
            auto& out =
                particles(EffectKind::ExplosionRing, fx.id, 0, fx.texture, fx.settings.radius / 640.f, fx.alpha);
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
            {
                const auto local = rotate_vector(fx.rotation, p.position);
                out.particles.push_back({{fx.settings.position.x + local.x, fx.settings.position.y + local.y,
                                          fx.settings.position.z + local.z},
                                         {},
                                         0,
                                         fx.settings.color});
            }
        }
        for (const auto& fx : runtime.explosion_fragments.trails)
        {
            auto& out = particles(EffectKind::ExplosionTrail, fx.id, 0, fx.texture, fx.radius / 640.f, fx.alpha);
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
                out.particles.push_back({vector_add(fx.origin, p.position), {}, 0, p.color});
        }
        for (const auto& fx : runtime.explosion_flames.active)
        {
            auto& out =
                particles(EffectKind::ExplosionFlame, fx.id, 0, fx.texture, fx.settings.radius / 640.f, fx.alpha);
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
                out.particles.push_back({vector_add(fx.settings.position, p.position), {}, 0, p.color});
        }
        for (const auto& instance : runtime.object_particles.active)
        {
            const auto& fx = instance.effect;
            auto& out = particles(EffectKind::ObjectParticles, fx.id, 0, fx.texture, fx.radius / 640.f, fx.alpha);
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
            {
                const auto origin = (fx.flags & 0x04) ? fx.origin : fx.position;
                out.particles.push_back(
                    {vector_add(origin, rotate_vector(fx.system_rotation, p.position)), {}, 0, p.color});
            }
        }
        for (const auto& instance : runtime.particle_streams.active)
        {
            const auto* owner = world.get(instance.owner);
            if (!owner)
                continue;
            const auto& fx = instance.effect;
            if (fx.settings.system && !fx.system_visible)
                continue;
            auto& out =
                particles(EffectKind::ParticleStream, fx.id, 0, fx.texture, fx.settings.radius / 640.f, fx.alpha);
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
                out.particles.push_back(
                    {vector_add(fx.position, rotate_vector(fx.system_rotation, p.position)), {}, 0, p.color});
        }
        auto model = [&](EffectKind kind, std::uint64_t id, const auto& fx, NodePose pose, DVector scale, bool visible,
                         float alpha, bool require_skin) -> EffectModel&
        {
            frame.models.push_back(
                {{kind, id}, fx.model, fx.animation, fx.skin_path, {}, pose, scale, alpha, visible, require_skin});
            if constexpr (requires { fx.model_path; })
                frame.models.back().label = fx.model_path;
            return frame.models.back();
        };
        for (const auto& fx : runtime.explosion_models.active)
            model(EffectKind::ExplosionModel, fx.id, fx, {fx.settings.position, fx.rotation}, fx.scale,
                  fx.scale.x > 0 && fx.scale.y > 0 && fx.scale.z > 0, fx.alpha, true);
        for (const auto& fx : runtime.explosion_fragments.active)
        {
            const auto s = fx.settings.scale;
            model(EffectKind::ExplosionFragment, fx.id, fx, {fx.position, fx.rotation}, s,
                  s.x > 0 && s.y > 0 && s.z > 0, fx.alpha, true);
        }
        for (const auto& fx : runtime.flesh_gibs.active)
            model(EffectKind::FleshGib, fx.id, fx, {fx.position, fx.rotation}, fx.scale, true, 1, false);
        for (const auto& fx : runtime.destruction_debris.active)
            model(EffectKind::DestructionDebris, fx.id, fx, {fx.position, fx.rotation}, fx.scale, true, fx.alpha, true)
                .translucent = fx.alpha < 1;
        for (const auto& fx : runtime.laser_beams.active)
            model(EffectKind::Laser, fx.id, fx, {fx.position, fx.rotation}, fx.scale, fx.updates != 0, fx.alpha, true)
                .label = "Original laser beam";
        for (const auto& fx : runtime.shotgun_effects.casings)
            model(EffectKind::Casing, fx.id, fx, {fx.position, fx.rotation}, {fx.scale, fx.scale, fx.scale}, fx.visible,
                  1, true);
        for (const auto& instance : runtime.polygrids.active)
        {
            const auto* owner = world.get(instance.owner);
            if (!owner)
                continue;
            const auto& fx = instance.effect;
            EffectPolyGrid p;
            p.id.value = fx.id;
            p.owner = owner->id;
            p.pose = fx.pose;
            p.dimensions = fx.settings.dims;
            p.width = fx.width;
            p.heights = fx.heights;
            p.palette = fx.palette;
            p.scale = fx.scale;
            p.pan = fx.sprite ? fx.settings.pan : std::array<float, 2>{};
            p.alpha = fx.settings.alpha;
            p.visible = (owner->user_flags & 1u) != 0;
            if (fx.sprite && fx.track.frame >= fx.sprite->frames.size())
                p.error = "PolyGrid current sprite frame unavailable";
            else if (fx.sprite && fx.track.has_frame)
                p.texture = fx.sprite->frames[fx.track.frame];
            frame.polygrids.push_back(std::move(p));
        }
        for (const auto& fx : runtime.shotgun_effects.tracers)
            frame.tracers.push_back({{EffectKind::Tracer, fx.id}, fx.center, fx.forward, fx.right});
        for (const auto& fx : runtime.detached_lines)
            if (frame.time < fx.expires)
                frame.lines.push_back({{EffectKind::DetachedLine, fx.id}, fx.line});

        auto add = [&](DVector position, DVector color, float radius, DDWORD flags = 0)
        {
            if (!(radius > 0) || radius > 1e7f || !std::isfinite(radius) || !physics_vector(position) ||
                !physics_vector(color) || color.x < 0 || color.y < 0 || color.z < 0 || color.x > 1 || color.y > 1 ||
                color.z > 1)
                return;
            const float brightness = std::max({color.x, color.y, color.z});
            if (!(brightness > 0))
                return;
            if (flags & (1u << 7))
            {
                ++frame.fog_lights_unsupported;
                return;
            }
            if (flags & (1u << 3))
                ++frame.shadow_lights_unshadowed;
            frame.lights.push_back({{position, color, radius}, flags});
        };
        for (const auto& light : runtime.light_effects)
            if (light.visible)
                add(light.position, light.color, light.radius, light.settings.flags);
        auto explosion = [&](const CExplosionLightFX& light)
        {
            if ((light.flags & FLAG_VISIBLE) && !light.delay)
                add(light.settings.position, light.color, light.radius);
        };
        for (const auto& light : runtime.explosion_lights.active)
            explosion(light);
        for (const auto& bolt : runtime.lightning.active)
            for (const auto& light : bolt.lights)
                explosion(light);
    }
} // namespace OpenBlood2
