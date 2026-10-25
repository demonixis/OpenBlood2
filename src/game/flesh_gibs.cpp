// Original ClientShellDLL/gibfx.cpp and client_physics.cpp, flesh and selected destruction materials.
#include "game/flesh_gibs.h"
#include "engine/collision.h"
#include "engine/texture.h"
#include "game/legacy/server_de.h"
#include "game/original/BloodSplatFX.h"
#include "game/original/ParticleExplosionFX.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace OpenBlood2
{
    namespace
    {
        float draw(std::minstd_rand& rng, float lo, float hi)
        {
            return lo + (hi - lo) * float(rng() - std::minstd_rand::min()) /
                            float(std::minstd_rand::max() - std::minstd_rand::min());
        }
        int integer(std::minstd_rand& rng, int lo, int hi)
        {
            return lo + int(rng() % unsigned(hi - lo + 1));
        }
    } // namespace
    std::uint64_t FleshGibs::publish(HOBJECT owner, DVector pos, DVector dir, DVector dims, float scale, unsigned count,
                                     ServerDE& server, ModelStore& models, const Resources& resources, double now,
                                     std::string& error)
    {
        return publish_impl(owner, pos, dir, dims, scale, count, server, models, resources, now, error);
    }
    std::uint64_t FleshGibs::publish_debris(const DebrisEffect& effect, int detail, ServerDE& server,
                                            ModelStore& models, const Resources& resources, double now,
                                            std::string& error)
    {
        return publish_impl(nullptr, effect.position, effect.velocity, effect.dimensions, effect.scale,
                            detail == 0 ? effect.amount / 2 : effect.amount, server, models, resources, now, error,
                            &effect);
    }
    std::uint64_t FleshGibs::publish_impl(HOBJECT owner, DVector pos, DVector dir, DVector dims, float scale,
                                          unsigned count, ServerDE& server, ModelStore& models,
                                          const Resources& resources, double now, std::string& error,
                                          const DebrisEffect* debris)
    {
        error.clear();
        const unsigned surface = debris ? (debris->flags & 0xff) * 10 : 100;
        const bool custom = debris && (debris->flags & 0x4000);
        FleshGibReport report;
        if (debris)
            report.source = debris->source;
        else if (const auto* name = server.GetObjectName(owner))
            report.source = name;
        report.position = pos;
        report.velocity = dir;
        report.dimensions = dims;
        report.scale = scale;
        report.requested = debris ? debris->amount : count;
        if ((!debris && !server.IsObjectAlive(owner)) || !physics_vector(pos) || !physics_vector(dir) ||
            !physics_vector(dims) || dims.x < 0 || dims.y < 0 || dims.z < 0 || !std::isfinite(scale) || scale <= 0 ||
            scale > 100 || (!debris && (count > 64 || active.size() + count * 2 > 256)) || count > 255 ||
            !std::isfinite(now) || now < 0 || report.source.size() > 100)
            error = "Invalid client-gib request or capacity exhausted";
        else if (debris && surface != 20 && surface != 30 && surface != 50)
            error = "Destruction debris material not implemented: " + std::to_string(surface);
        else if (custom && (debris->model.empty() || debris->texture.empty() || debris->model.size() > 100 ||
                            debris->texture.size() > 100 || debris->sound.size() > 100))
            error = "Custom destruction debris requires bounded model and skin paths";
        else
        {
            // Selected high detail: the source's (DBYTE)1.5 is exactly one.
            if (!debris && integer(random_, 0, 99) < 20)
                count *= 2;
            report.id = next_event_++;
            report.adjusted = count;
            for (unsigned i = 0; i < count; ++i)
            {
                FleshGib g;
                g.blood = !debris;
                g.surface = surface;
                g.alpha = surface == 50 ? .4f : 1.f;
                g.position = pos;
                g.last_time = now;
                const auto half = vector_scale(dims, .8f);
                if (half.x)
                    g.position.x += draw(random_, -half.x, half.x);
                if (half.z)
                    g.position.z += draw(random_, -half.z, half.z);
                if (half.y)
                    g.position.y += draw(random_, surface == 50 ? -half.y : -half.y / 2, half.y);
                const float speed = std::sqrt(vector_dot(dir, dir));
                auto forward =
                    speed > 0 ? vector_scale(dir, 1 / speed) : DVector{0, 1, 0}; // finite zero-speed adaptation
                DRotation basis;
                if (!align_rotation(forward, basis))
                {
                    error = "Invalid flesh-gib alignment";
                    break;
                }
                auto right = rotate_vector(basis, {1, 0, 0}), up = rotate_vector(basis, {0, 1, 0});
                g.angular_velocity = {draw(random_, -2 * std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>),
                                      draw(random_, -2 * std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>), 0};
                if (custom)
                {
                    // Original custom branch skips both scale randomization and the authored multiplier.
                    g.model_path = debris->model;
                    g.skin_path = debris->texture;
                    g.sound_path = debris->sound;
                    g.bounces = 1;
                }
                else
                {
                    // B2_LITE NRES limits model and sound choices to 1..3 for every material.
                    const int model = integer(random_, 1, 3);
                    g.bounces = debris ? 1 : (integer(random_, 0, 4) == 4 ? 1 : 0);
                    const int sound = integer(random_, 1, 3);
                    const std::string material = surface == 50   ? "glass"
                                                 : surface == 30 ? "wood"
                                                 : surface == 20 ? "metal"
                                                                 : "flesh";
                    g.model_path = "models/gibs/" + material + "/gib" + std::to_string(model) + ".abc";
                    g.skin_path = "skins/gibs/" + material + "/gib" + std::to_string(model) + ".dtx";
                    g.sound_path = "sounds/gibs/" + material + "/gib_impact" + std::to_string(sound) + ".wav";
                    if (!debris || surface == 50)
                        g.scale = {draw(random_, debris ? .5f : .8f, debris ? 1.25f : 1.35f) * scale,
                                   draw(random_, debris ? .5f : .8f, debris ? 1.25f : 1.35f) * scale,
                                   draw(random_, debris ? .5f : .8f, debris ? 1.25f : 1.35f) * scale};
                    else
                        g.scale = {scale, scale, scale};
                }
                if (!models.load(g.model_path, g.model, error) || !resources.find(g.skin_path))
                {
                    if (error.empty())
                        error = "Client-gib skin not indexed: " + g.skin_path;
                    break;
                }
                if (debris)
                {
                    Texture image;
                    if (!read_texture_resource(resources, g.skin_path, image, error))
                    {
                        error = g.skin_path + ": " + error;
                        break;
                    }
                }
                if (std::abs(forward.y) >= .95f)
                {
                    forward.y = forward.y > 0 ? 1.f : -1.f;
                    right = {1, 0, 0};
                    up = {0, 0, 1};
                }
                else if (std::abs(forward.x) >= .95f)
                {
                    forward.x = forward.x > 0 ? 1.f : -1.f;
                    right = {0, 1, 0};
                    up = {0, 0, 1};
                }
                else if (std::abs(forward.z) >= .95f)
                {
                    forward.z = forward.z > 0 ? 1.f : -1.f;
                    right = {1, 0, 0};
                    up = {0, 1, 0};
                }
                const auto lo = vector_scale(vector_add(right, up), -speed);
                const auto hi = vector_add(vector_scale(forward, (surface == 50 ? 2 : 4) * speed),
                                           vector_scale(vector_add(right, up), speed));
                g.velocity = {draw(random_, lo.x, hi.x), draw(random_, lo.y, hi.y), draw(random_, lo.z, hi.z)};
                ContainerCode code = CC_NOTHING;
                if (!server.GetPointContainerCode(g.position, code))
                {
                    error = "Flesh-gib contents query unavailable";
                    break;
                }
                g.liquid = IsLiquid(code);
                if (g.blood)
                {
                    BloodSpurtSettings spurt;
                    spurt.position = g.position;
                    spurt.min_velocity = spurt.max_velocity = vector_scale(g.velocity, .6f);
                    spurt.texture = integer(random_, 1, 3);
                    g.spurt_event = server.PublishClientBloodSpurt(spurt, "flesh fragment launch", true, g.spurt_error);
                }
                g.id = next_id_++;
                if (debris)
                {
                    // Original oldest-slot replacement and 150-update terminating fade.
                    if (std::count_if(active.begin(), active.end(), [](const auto& f) { return !f.fading; }) >= 80)
                    {
                        auto oldest =
                            std::find_if(active.begin(), active.end(), [](const auto& f) { return !f.fading; });
                        oldest->fading = true;
                    }
                    if (std::count_if(active.begin(), active.end(), [](const auto& f) { return f.fading; }) > 50)
                        active.erase(
                            std::find_if(active.begin(), active.end(), [](const auto& f) { return f.fading; }));
                }
                active.push_back(std::move(g));
                ++report.created;
            }
        }
        report.error = error;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        const auto id = report.id;
        reports.push_back(std::move(report));
        return error.empty() ? id : 0;
    }
    bool FleshGibs::advance(double now, ServerDE& server)
    {
        bool ok = true;
        for (auto& g : active)
        {
            if (!g.error.empty())
            {
                ok = false;
                continue;
            }
            const auto dt = now - g.last_time;
            if (!std::isfinite(dt) || dt < 0 || dt > 1)
            {
                g.error = "Invalid flesh-gib clock";
                ok = false;
                continue;
            }
            if (!g.blood && dt < .001)
                continue;
            g.last_time = now;
            if (g.fading)
            {
                g.alpha = 1.f - float(g.fade_updates++) / 150.f;
                continue;
            }
            std::vector<std::uint32_t> keys;
            if (!advance_animation(*g.model, g.animation, dt, keys, g.error))
            {
                ok = false;
                continue;
            }
            if (g.resting || dt == 0)
                continue;
            if (vector_dot(g.velocity, g.velocity) < .01f)
                g.velocity = {};
            auto next = vector_add(g.position, vector_scale(g.velocity, float(dt)));
            // InitMovingObject memset clears the HALFGRAVITY/STICKY bits set by CreateObject.
            g.velocity.y -= float(dt) * (g.liquid ? 250.f : 500.f);
            SegmentHit hit;
            const auto delta = vector_subtract(next, g.position);
            const auto result = vector_dot(delta, delta) < 1e-12f ? SegmentResult::Miss
                                                                  : server.IntersectWorldSegment(g.position, next, hit);
            if (result != SegmentResult::Hit && result != SegmentResult::Miss)
            {
                g.error = "Flesh-gib world collision unavailable";
                ok = false;
                continue;
            }
            if (result == SegmentResult::Hit)
            {
                const float dot = -2 * vector_dot(g.velocity, hit.normal);
                g.velocity =
                    vector_scale(vector_add(g.velocity, vector_scale(hit.normal, dot)), g.liquid ? .20f : .35f);
                if (dot > 0)
                    next = vector_add(hit.point, vector_scale(hit.normal, .3f));
                if (hit.normal.y > .6f && vector_dot(g.velocity, g.velocity) < 250)
                    g.resting = true;
                if (hit.normal.y > 0)
                {
                    if (g.bounces <= 0)
                        g.resting = true;
                    --g.bounces;
                }
                else
                    g.resting = false;
                ++g.impacts;
                // Client sounds have no server owner: the adapter accepts world-owned playback below.
                SoundRequest request;
                request.resource = g.sound_path;
                request.outer_radius = 750;
                request.inner_radius = 75;
                std::string sound_error;
                if (!g.sound_path.empty())
                    server.PlayClientSound(request, next, sound_error);
                g.sound_error = sound_error;
                if (g.blood)
                {
                    BloodSplatSettings splat;
                    splat.resource = "sprites/blood" + std::to_string(integer(random_, 1, 3)) + ".spr";
                    splat.position = next;
                    splat.scale = .14f + draw(random_, -.1f, .1f);
                    splat.grow_scale = hit.normal.y >= .5f ? .01f : 0;
                    if (align_rotation(hit.normal, hit.normal, splat.rotation))
                        g.splat_event = server.PublishClientBloodSplat(splat, "flesh fragment impact", g.splat_error);
                    else
                        g.splat_error = "Invalid flesh-fragment impact normal";
                }
                g.angular_velocity = {draw(random_, -2 * std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>),
                                      draw(random_, -2 * std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>), 0};
            }
            else
            {
                g.angles = vector_add(g.angles, vector_scale(g.angular_velocity, float(dt)));
                g.rotation = rotation_from_euler(g.angles);
            }
            if (!physics_vector(next) || !physics_vector(g.velocity))
            {
                g.error = "Flesh-gib motion exceeds bounds";
                ok = false;
                continue;
            }
            g.position = next;
        }
        std::erase_if(active, [](const auto& g) { return g.fading && g.fade_updates > 150; });
        if (!ok)
            update_error = "Flesh-gib update failed; inspect individual fragment";
        return ok;
    }
    void FleshGibs::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        random_.seed(1);
    }
} // namespace OpenBlood2
