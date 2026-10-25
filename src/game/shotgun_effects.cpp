// Selected original WeaponFX, ShellCasingFX, SurfaceFragmentFX, MarkSFX, SplashFX and RippleFX.
#include "game/shotgun_effects.h"
#include "engine/collision.h"
#include "engine/texture.h"
#include "game/legacy/server_de.h"
#include "game/original/SparksFX.h"
#include "game/weapon_sprites.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace OpenBlood2
{
    namespace
    {
        DVector unit(DVector v)
        {
            return vector_scale(v, 1 / std::sqrt(vector_dot(v, v)));
        }
        bool color(DVector c)
        {
            return physics_vector(c) && c.x >= 0 && c.x <= 255 && c.y >= 0 && c.y <= 255 && c.z >= 0 && c.z <= 255;
        }
        bool texture(const Resources& resources, const std::string& path, std::string& error)
        {
            Texture out;
            return read_texture_resource(resources, path, out, error);
        }
        bool sprite(SpriteStore& store, const Resources& resources, const std::string& path,
                    std::shared_ptr<const Sprite>& out, std::string& error)
        {
            if (!store.load(path, out, error))
                return false;
            for (const auto& frame : out->frames)
                if (!texture(resources, frame, error))
                    return false;
            return true;
        }
    } // namespace
    float ShotgunEffects::random(float lo, float hi)
    {
        return lo + (hi - lo) * float(random_() - random_.min()) / float(random_.max() - random_.min());
    }
    unsigned ShotgunEffects::integer(unsigned lo, unsigned hi)
    {
        return lo + random_() % (hi - lo + 1);
    }
    void ShotgunEffects::emit(ShotgunSplash& s)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            Particle p;
            p.position = {random(-1, 1), random(-1, 1), random(-1, 1)};
            p.velocity = vector_add(vector_scale(s.normal, 175), {random(-10, 10), random(-10, 10), random(-10, 10)});
            p.color = vector_scale(integer(0, 1) ? s.color1 : s.color2, 1.f / 255);
            p.lifetime = .75f;
            s.particles.push_back(p);
        }
    }
    std::uint64_t ShotgunEffects::publish(HOBJECT owner, const ShotgunEffectSettings& s, ModelStore& models,
                                          SpriteStore& store, const Resources& resources, double now, ServerDE& server,
                                          std::string& error)
    {
        error.clear();
        ShotgunEffectReport report;
        report.id = next_event_++;
        report.settings = s;
        auto failure = [&](const std::string& message)
        {
            if (!message.empty())
            {
                if (!error.empty())
                    error += "; ";
                error += message;
            }
        };
        if (!physics_vector(s.source) || !physics_vector(s.destination) || !physics_vector(s.forward) ||
            !physics_vector(s.normal) || vector_dot(s.forward, s.forward) < 1e-12f ||
            vector_dot(s.normal, s.normal) < 1e-12f || !color(s.color1) || !color(s.color2) || !std::isfinite(now) ||
            now < 0)
            error = "Invalid shotgun effect payload";
        else
        {
            const auto normal = unit(s.normal), forward = unit(s.forward);
            const auto delta = vector_subtract(s.destination, s.source);
            const auto direction = vector_dot(delta, delta) > 1e-12f ? unit(delta) : forward;
            DRotation rotation;
            align_rotation(normal, {0, 1, 0}, rotation);
            const auto up = rotate_vector(rotation, {0, 1, 0}), right = rotate_vector(rotation, {1, 0, 0});
            ContainerCode source_code = CC_NOTHING, dest_code = CC_NOTHING;
            const bool containers = server.GetPointContainerCode(s.source, source_code, true) &&
                                    server.GetPointContainerCode(s.destination, dest_code, true);
            if (!containers)
                failure("Shotgun effect container query unavailable");
            auto bubble = [&](DVector at)
            {
                SparkSettings b;
                b.position = at;
                b.direction = vector_scale(normal, 15);
                b.color1 = {125, 125, 255};
                b.color2 = {180, 180, 255};
                b.count = 1;
                b.duration = 1.25f;
                b.emission_radius = .05f;
                b.radius = 600;
                b.gravity = 50;
                b.texture = "SpriteTextures/Particles/ParticleBubble.dtx";
                std::string e;
                if (texture(resources, b.texture, e) && server.PublishSparks(owner, b, e))
                    ++report.bubbles;
                failure(e);
            };
            if (s.eject)
            {
                if (containers && IsLiquid(source_code) && s.surface != SURFTYPE_LIQUID)
                    bubble(s.source);
                else if (containers)
                {
                    WeaponSpriteSettings ws;
                    auto& smoke = ws.smoke.emplace();
                    smoke.position = s.source;
                    smoke.normal = normal;
                    smoke.gravity = {random(-.5f, .5f), random(-.25f, .75f), random(-.5f, .5f)};
                    smoke.resource = "Sprites/smokepuff" + std::to_string(integer(1, 3)) + ".spr";
                    smoke.duration = 1;
                    smoke.alpha = .25f;
                    smoke.scale1 = {.04f, .04f, 0};
                    smoke.scale2 = {.25f, .25f, 0};
                    smoke.fade = smoke.move = true;
                    smoke.velocity = vector_scale(unit(vector_subtract(vector_scale(direction, 2), forward)), 3);
                    smoke.deceleration = vector_scale(forward, -.07f);
                    std::string e;
                    report.smoke = server.PublishWeaponSprites(owner, ws, e);
                    failure(e);
                }
                DRotation eject_rotation;
                align_rotation(direction, {0, 1, 0}, eject_rotation);
                for (unsigned i = 0; i < 2; ++i)
                {
                    ShotgunCasing c;
                    c.id = next_id_++;
                    c.position = s.source;
                    c.last_time = now;
                    c.expires = now + 20;
                    c.rotation = eject_rotation;
                    c.liquid = IsLiquid(source_code);
                    c.velocity =
                        vector_add(vector_add(vector_scale(rotate_vector(eject_rotation, {0, 1, 0}), random(60, 90)),
                                              vector_scale(rotate_vector(eject_rotation, {s.left ? -1.f : 1.f, 0, 0}),
                                                           random(50, 70))),
                                   vector_scale(direction, random(10, 25)));
                    c.pitch_velocity = random(-4 * std::numbers::pi_v<float>, 4 * std::numbers::pi_v<float>);
                    c.yaw_velocity = random(-4 * std::numbers::pi_v<float>, 4 * std::numbers::pi_v<float>);
                    std::string e;
                    if (containers && models.load("Models/Ammo/sshell.abc", c.model, e) &&
                        texture(resources, "Skins/Ammo/sshell.dtx", e))
                    {
                        c.animation.animation = 0;
                        if (casings.size() == 100)
                            casings.erase(casings.begin());
                        casings.push_back(std::move(c));
                        ++report.casings;
                    }
                    failure(e);
                }
            }
            auto add_sprite = [&](ShotgunSprite fx)
            {
                if (fx.kind == ShotgunSpriteKind::Mark)
                {
                    ShotgunSprite* closest = nullptr;
                    float distance = 100;
                    unsigned nearby = 0;
                    for (auto& old : sprites)
                    {
                        if (old.kind != ShotgunSpriteKind::Mark)
                            continue;
                        const auto delta = vector_subtract(old.position, fx.position);
                        const auto d = vector_dot(delta, delta);
                        if (d >= 100)
                            continue;
                        if (d < distance)
                        {
                            closest = &old;
                            distance = d;
                        }
                        if (++nearby > 10)
                        {
                            closest->position = fx.position;
                            return true;
                        }
                    }
                }
                std::string e;
                if (sprite(store, resources, fx.resource, fx.sprite, e))
                {
                    fx.id = next_id_++;
                    fx.last_time = now;
                    const auto limit = fx.kind == ShotgunSpriteKind::Mark ? 300 : 80;
                    if (std::count_if(sprites.begin(), sprites.end(),
                                      [&](const auto& old) { return old.kind == fx.kind; }) >= limit)
                        sprites.erase(std::find_if(sprites.begin(), sprites.end(),
                                                   [&](const auto& old) { return old.kind == fx.kind; }));
                    sprites.push_back(std::move(fx));
                    return true;
                }
                failure(e);
                return false;
            };
            if (s.surface != SURFTYPE_UNKNOWN && s.surface != SURFTYPE_FLESH && s.surface != SURFTYPE_SKY &&
                s.surface != SURFTYPE_GLASS)
            {
                ShotgunSprite fx;
                fx.position = s.destination;
                fx.rotation = rotation;
                fx.color = {.1f, .1f, .1f};
                std::string suffix;
                switch (s.surface)
                {
                    case SURFTYPE_METAL:
                        break;
                    case SURFTYPE_PLASTIC:
                    case SURFTYPE_CLOTH:
                        suffix = "2";
                        break;
                    case SURFTYPE_WOOD:
                        suffix = "5";
                        break;
                    default:
                        suffix = std::to_string(integer(3, 4));
                        break;
                }
                fx.resource = "Sprites/blthole" + suffix + ".spr";
                if (add_sprite(std::move(fx)))
                    ++report.marks;
            }
            if (s.surface == SURFTYPE_WOOD || s.surface == SURFTYPE_STONE)
            {
                if (containers && IsLiquid(dest_code))
                    bubble(s.destination);
                DVector velocity = vector_scale(normal, normal.y > .5f ? 3.f : 1.f);
                const auto count = integer(1, 3);
                for (unsigned i = 0; i < count; ++i)
                {
                    // Source mutates the shared velocity between successive fragments.
                    velocity = vector_add(velocity, vector_scale(right, random(-2, 2)));
                    velocity = vector_add(velocity, vector_scale(up, random(-2, 3)));
                    ShotgunSprite fx;
                    fx.kind = ShotgunSpriteKind::Fragment;
                    fx.position = vector_add(s.destination, vector_scale(normal, 3));
                    fx.velocity = velocity;
                    fx.duration = normal.y > .5f ? 2.f : 3.f;
                    const bool wood = s.surface == SURFTYPE_WOOD;
                    fx.gravity.y = wood ? -.2f : -.3f;
                    if (IsLiquid(dest_code))
                        fx.gravity.y *= .5f;
                    const float scale = wood ? .05f : .03f;
                    fx.scale = {scale, scale, 0};
                    fx.pitch_velocity = random(-2 * std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>);
                    fx.yaw_velocity = random(-2 * std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>);
                    fx.resource = std::string("Sprites/") + (wood ? "woodsplint" : "stonechip") +
                                  std::to_string(integer(1, 3)) + ".spr";
                    if (add_sprite(std::move(fx)))
                        ++report.fragments;
                }
            }
            if (s.surface == SURFTYPE_LIQUID)
            {
                std::string e;
                if (texture(resources, "SpriteTextures/drop32_1.dtx", e))
                {
                    ShotgunSplash splash;
                    splash.id = next_id_++;
                    splash.position = s.destination;
                    splash.normal = normal;
                    splash.color1 = s.color1;
                    splash.color2 = s.color2;
                    splash.start = splash.last_time = now;
                    emit(splash);
                    if (splashes.size() == 80)
                        splashes.erase(splashes.begin());
                    splashes.push_back(std::move(splash));
                    ++report.splashes;
                }
                failure(e);
                ShotgunSprite ripple;
                ripple.kind = ShotgunSpriteKind::Ripple;
                ripple.position = s.destination;
                ripple.rotation = rotation;
                ripple.scale = {.1f, .1f, 0};
                ripple.alpha = .25f;
                ripple.duration = 1;
                ripple.resource = "Sprites/ripple.spr";
                add_sprite(std::move(ripple));
            }
        }
        report.error = error;
        if (!error.empty())
            ++failed;
        ++started;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    void ShotgunEffects::advance(double now, ServerDE& server)
    {
        auto valid_time = [&](double last) { return std::isfinite(now) && now >= last && now - last <= 1; };
        for (auto& c : casings)
        {
            if (!c.error.empty())
                continue;
            if (!valid_time(c.last_time))
            {
                c.error = update_error = "Invalid shotgun casing clock";
                continue;
            }
            const auto dt = now - c.last_time;
            if (dt < .001)
                continue;
            c.last_time = now;
            c.visible = true;
            if (c.resting)
            {
                c.rotation = rotation_from_euler({0, c.yaw, 0});
                continue;
            }
            c.pitch += c.pitch_velocity * float(dt);
            c.yaw += c.yaw_velocity * float(dt);
            c.rotation = rotation_from_euler({c.pitch, c.yaw, 0});
            auto next = vector_add(c.position, vector_scale(c.velocity, float(dt)));
            c.velocity.y -= float(dt) * (c.liquid ? 125.f : 250.f); // InitMovingObject THEN MO_HALFGRAVITY.
            SegmentHit hit;
            auto result = vector_dot(vector_subtract(next, c.position), vector_subtract(next, c.position)) < 1e-12f
                              ? SegmentResult::Miss
                              : server.IntersectWorldSegment(c.position, next, hit);
            if (result == SegmentResult::Hit)
            {
                const float dot = -2 * vector_dot(c.velocity, hit.normal);
                c.velocity =
                    vector_scale(vector_add(c.velocity, vector_scale(hit.normal, dot)), c.liquid ? .20f : .35f);
                if (dot > 0)
                    next = vector_add(hit.point, vector_scale(hit.normal, .3f));
                if (hit.normal.y > .6f && vector_dot(c.velocity, c.velocity) < 250)
                    c.resting = true;
                const char* material = "Stone";
                switch (hit.texture_flags)
                {
                    case SURFTYPE_GLASS:
                        material = "Glass";
                        break;
                    case SURFTYPE_METAL:
                        material = "Metal";
                        break;
                    case SURFTYPE_PLASTIC:
                        material = "Plastic";
                        break;
                    case SURFTYPE_TERRAIN:
                        material = "Terrain";
                        break;
                    case SURFTYPE_LIQUID:
                        material = "Water";
                        break;
                    case SURFTYPE_WOOD:
                        material = "Wood";
                        break;
                    default:
                        break;
                }
                SoundRequest sound;
                sound.resource = std::string("Sounds/Weapons/ShellDrops/") + material + "/Shell" +
                                 std::to_string(integer(1, 2)) + ".wav";
                sound.outer_radius = 150;
                sound.inner_radius = 15;
                server.PlayClientSound(sound, next, c.sound_error);
                c.pitch_velocity = random(-4 * std::numbers::pi_v<float>, 4 * std::numbers::pi_v<float>);
                c.yaw_velocity = random(-4 * std::numbers::pi_v<float>, 4 * std::numbers::pi_v<float>);
                if (--c.bounces <= 0)
                    c.resting = true;
                ++c.impacts;
            }
            else if (result != SegmentResult::Miss)
            {
                c.error = update_error = "Shotgun casing collision unavailable";
                continue;
            }
            c.position = next;
        }
        std::erase_if(casings, [&](const auto& c) { return now > c.expires || !c.error.empty(); });
        for (auto& fx : sprites)
        {
            if (!valid_time(fx.last_time))
            {
                update_error = "Invalid shotgun sprite clock";
                continue;
            }
            const auto dt = now - fx.last_time;
            if (dt < .001)
                continue;
            fx.last_time = now;
            advance_sprite(*fx.sprite, fx.track, dt);
            if (fx.start < 0)
                fx.start = now;
            if (fx.kind == ShotgunSpriteKind::Fragment)
            {
                fx.velocity = vector_add(fx.velocity, fx.gravity);
                fx.position = vector_add(fx.position, fx.velocity);
                fx.pitch += fx.pitch_velocity;
                fx.yaw += fx.yaw_velocity; // Source per-update rotation/motion.
                fx.rotation = rotation_from_euler({fx.pitch, fx.yaw, 0});
            }
        }
        std::erase_if(sprites,
                      [&](const auto& s) { return s.duration > 0 && s.start >= 0 && now >= s.start + s.duration; });
        for (auto& s : splashes)
        {
            if (!valid_time(s.last_time))
            {
                s.error = update_error = "Invalid shotgun splash clock";
                continue;
            }
            auto dt = now - s.last_time;
            if (dt < .001)
                continue;
            s.last_time = now;
            if (!advance_particles(s.particles, -500, dt, s.error))
                update_error = s.error;
            if (now < s.start + .3)
                emit(s);
        }
        // Source can retain an empty parentless splash indefinitely; retire once no particle remains.
        std::erase_if(splashes,
                      [&](const auto& s) { return !s.error.empty() || (now >= s.start + .3 && s.particles.empty()); });
    }
    void ShotgunEffects::clear()
    {
        casings.clear();
        sprites.clear();
        splashes.clear();
        reports.clear();
        update_error.clear();
        random_.seed(1);
        started = failed = 0;
    }
} // namespace OpenBlood2
