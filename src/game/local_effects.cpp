#include "game/local_effects.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/FlareProjectile.h"
#include "game/original/LeechProjectile.h"
#include "game/original/TeslaProjectile.h"
#include "game/original/VolumeBrush.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
namespace OpenBlood2
{
    bool LocalEffects::publish(const GameSession& game, ClientEffects& client, std::string& error)
    {
        if (game.objects.busy())
        {
            error = "Effect publication during a server callback rejected";
            return false;
        }
        ClientEffectsFrame frame;
        frame.world = game.client_session.world.frame().world;
        frame.sequence = client.frame().sequence + 1;
        frame.time = game.server.world_time();
        const auto& server = game.client_effects;
        const auto& flashlight = game.client_shell.flashlight;
        const auto* source = game.objects.get(flashlight.source);
        const auto* owner = game.objects.get(flashlight.owner);
        if (flashlight.visible && source && source->life == Life::Alive && owner && owner->life == Life::Alive &&
            (source->user_flags & 1u))
            frame.flashlight = flashlight.light;
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
        for (const auto& fx : server.explosion_sprites.active)
            sprite(EffectKind::ExplosionSprite, fx.id, fx.sprite, fx.track, {fx.settings.position, fx.rotation},
                   fx.scale, fx.flags, fx.alpha);
        for (const auto& gate : server.warp_gates.active)
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
        for (const auto& bolt : server.lightning.active)
            for (unsigned j = 0; j < bolt.segments.size(); ++j)
            {
                const auto& segment = bolt.segments[j];
                auto& out =
                    particles(EffectKind::Lightning, bolt.id, j, bolt.texture, segment.radius / 640.f, bolt.alpha);
                out.particles.reserve(segment.particles.size());
                for (const auto& p : segment.particles)
                    out.particles.push_back({p.position, {}, 0, segment.color});
            }
        for (const auto& fx : server.sparks.active)
        {
            auto& out = particles(EffectKind::Sparks, fx.id, 0, fx.settings.texture, fx.settings.radius / 640.f,
                                  std::max(0.f, fx.alpha));
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
                out.particles.push_back({vector_add(fx.settings.position, p.position), {}, 0, p.color});
        }
        for (const auto& fx : server.blood_spurts.active)
            if (!fx.particles.empty())
            {
                auto& out = particles(EffectKind::BloodSpurt, fx.id, 0, fx.texture, 200.f / 640, fx.alpha);
                out.origin = fx.settings.position;
                out.particles = fx.particles;
                out.batched = true;
            }
        for (const auto& fx : server.blood_splats.active)
            sprite(EffectKind::BloodSplat, fx.id, fx.sprite, fx.track, {fx.position, fx.rotation}, fx.scale,
                   FLAG_VISIBLE | FLAG_ROTATEABLESPRITE, fx.alpha, {.1f, .1f, .1f});
        // The local view-weapon muzzle is inserted here by the client scene consumer.
        for (const auto& fx : server.shotgun_effects.sprites)
        {
            auto& out = sprite(
                EffectKind::BallisticSprite, fx.id, fx.sprite, fx.track, {fx.position, fx.rotation}, fx.scale,
                FLAG_VISIBLE | FLAG_ROTATEABLESPRITE | (fx.kind == ShotgunSpriteKind::Mark ? 0 : FLAG_SPRITECHROMAKEY),
                fx.alpha, fx.color);
            if (fx.kind == ShotgunSpriteKind::Mark)
                out.clip_polygon = fx.clip_polygon;
        }
        for (const auto& fx : server.shotgun_effects.splashes)
        {
            auto& out = particles(EffectKind::BallisticSplash, fx.id, 0, "SpriteTextures/drop32_1.dtx", 100.f / 640, 1);
            out.origin = fx.position;
            out.particles = fx.particles;
            out.batched = true;
        }
        for (const auto& fx : server.weapon_sprites.flashes)
            sprite(EffectKind::WeaponFlash, fx.id, fx.sprite, fx.track, {fx.settings.position, fx.rotation}, fx.scale,
                   fx.flags, fx.alpha);
        for (const auto& fx : server.weapon_sprites.smoke)
            sprite(EffectKind::WeaponSmoke, fx.id, fx.sprite, fx.track, {fx.position, {}}, fx.scale, fx.flags, fx.alpha,
                   fx.color);
        for (const auto& fx : server.explosion_rings.active)
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
        for (const auto& fx : server.explosion_fragments.trails)
        {
            auto& out = particles(EffectKind::ExplosionTrail, fx.id, 0, fx.texture, fx.radius / 640.f, fx.alpha);
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
                out.particles.push_back({vector_add(fx.origin, p.position), {}, 0, p.color});
        }
        for (const auto& fx : server.explosion_flames.active)
        {
            auto& out =
                particles(EffectKind::ExplosionFlame, fx.id, 0, fx.texture, fx.settings.radius / 640.f, fx.alpha);
            out.particles.reserve(fx.particles.size());
            for (const auto& p : fx.particles)
                out.particles.push_back({vector_add(fx.settings.position, p.position), {}, 0, p.color});
        }
        for (const auto& instance : server.object_particles.active)
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
        for (const auto& instance : server.particle_streams.active)
        {
            const auto* owner = game.objects.get(instance.owner);
            if (!owner || owner->life != Life::Alive)
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
        for (const auto& fx : server.explosion_models.active)
            model(EffectKind::ExplosionModel, fx.id, fx, {fx.settings.position, fx.rotation}, fx.scale,
                  fx.scale.x > 0 && fx.scale.y > 0 && fx.scale.z > 0, fx.alpha, true);
        for (const auto& fx : server.explosion_fragments.active)
        {
            const auto s = fx.settings.scale;
            model(EffectKind::ExplosionFragment, fx.id, fx, {fx.position, fx.rotation}, s,
                  s.x > 0 && s.y > 0 && s.z > 0, fx.alpha, true);
        }
        for (const auto& fx : server.flesh_gibs.active)
            model(EffectKind::FleshGib, fx.id, fx, {fx.position, fx.rotation}, fx.scale, true, 1, false);
        for (const auto& fx : server.destruction_debris.active)
            model(EffectKind::DestructionDebris, fx.id, fx, {fx.position, fx.rotation}, fx.scale, true, fx.alpha, true)
                .translucent = fx.alpha < 1;
        for (const auto& fx : server.laser_beams.active)
            model(EffectKind::Laser, fx.id, fx, {fx.position, fx.rotation}, fx.scale, fx.updates != 0, fx.alpha, true)
                .label = "Original laser beam";
        for (const auto& fx : server.shotgun_effects.casings)
            model(EffectKind::Casing, fx.id, fx, {fx.position, fx.rotation}, {fx.scale, fx.scale, fx.scale}, fx.visible,
                  1, true);
        for (const auto& instance : server.polygrids.active)
        {
            const auto* owner = game.objects.get(instance.owner);
            if (!owner || owner->life != Life::Alive)
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
        for (const auto& fx : server.shotgun_effects.tracers)
            frame.tracers.push_back({{EffectKind::Tracer, fx.id}, fx.center, fx.forward, fx.right});

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
        for (const auto& light : game.client_effects.light_effects)
            if (light.visible)
                add(light.position, light.color, light.radius, light.settings.flags);
        auto explosion = [&](const CExplosionLightFX& light)
        {
            if ((light.flags & FLAG_VISIBLE) && !light.delay)
                add(light.settings.position, light.color, light.radius);
        };
        for (const auto& light : game.client_effects.explosion_lights.active)
            explosion(light);
        for (const auto& bolt : game.client_effects.lightning.active)
            for (const auto& light : bolt.lights)
                explosion(light);
        // Presentation-only equivalent of the source's owned projectile OT_LIGHT child.
        // No extra server record, game RNG draw or saved-payload change.
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->life != Life::Alive)
                continue;
            const auto* projectile = dynamic_cast<const CProjectile*>(r->game.get());
            if (!projectile || !projectile->configured || projectile->failed || projectile->exploded)
                continue;
            const auto position = r->state.m_Pos;
            if (auto* flare = dynamic_cast<const CFlareProjectile*>(projectile))
                add(position, flare->color, 200);
            else if (auto* flare = dynamic_cast<const CFlareAltProjectile*>(projectile))
                add(position, flare->color, 200);
            else if (auto* flare = dynamic_cast<const CFlareBurstProjectile*>(projectile))
                add(position, flare->color, 75);
            else if (auto* leech = dynamic_cast<const CLeechPrimeProjectile*>(projectile))
                add(position, leech->color, 100);
            else if (dynamic_cast<const CTeslaProjectile*>(projectile) ||
                     dynamic_cast<const CTeslaBoltProjectile*>(projectile) ||
                     dynamic_cast<const CTeslaBallProjectile*>(projectile))
                add(position, {.65f, .65f, 1}, 100);
            else if (dynamic_cast<const CNapalmProjectile*>(projectile) ||
                     dynamic_cast<const CNapalmFireball*>(projectile) ||
                     dynamic_cast<const CHowitzerAltFrag*>(projectile))
                add(position, {1, .75f, 0}, 100);
            else if (dynamic_cast<const CGroundStrikeProjectile*>(projectile))
                add(position, {1, .75f, 0}, 150);
        }
        auto variable = [&](const char* key)
        {
            const auto found = game.server.variables().find(key);
            if (found == game.server.variables().end())
                return 0.f;
            const auto& text = found->second.text;
            float value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size() && std::isfinite(value) ? value : 0.f;
        };
        frame.fog = {std::trunc(variable("FogEnable")) != 0,
                     variable("FogNearZ"),
                     variable("FogFarZ"),
                     {variable("FogR"), variable("FogG"), variable("FogB")}};
        std::set<std::uint32_t> live;
        for (auto id : game.objects.container_snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->life != Life::Alive || !r->container())
                continue;
            const auto& shape = r->container();
            auto& cached = shapes_[id.slot];
            if (cached.id != id || cached.source.lock() != shape)
            {
                cached = {id, shape, std::make_shared<const ContainerShape>(*shape)};
                ++shape_copies;
            }
            live.insert(id.slot);
            EffectFogVolume volume;
            volume.owner = id;
            volume.shape = cached.copy;
            if (const auto* brush = dynamic_cast<const VolumeBrush*>(r->game.get()))
            {
                volume.visible = (r->user_flags & FLAG_VISIBLE) != 0;
                volume.fog = {brush->fog, brush->fog_near, brush->fog_far, brush->fog_color};
            }
            frame.volumes.push_back(std::move(volume));
        }
        std::erase_if(shapes_, [&](const auto& pair) { return !live.contains(pair.first); });
        return client.receive(std::move(frame), error);
    }
} // namespace OpenBlood2
