// Original ClientShellDLL/gibfx.cpp and client_physics.cpp, flesh-only selection.
#include "game/flesh_gibs.h"
#include "engine/collision.h"
#include "game/legacy/server_de.h"
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
        error.clear();
        FleshGibReport report;
        if (const auto* name = server.GetObjectName(owner))
            report.source = name;
        report.position = pos;
        report.velocity = dir;
        report.dimensions = dims;
        report.scale = scale;
        report.requested = count;
        if (!server.IsObjectAlive(owner) || !physics_vector(pos) || !physics_vector(dir) || !physics_vector(dims) ||
            dims.x < 0 || dims.y < 0 || dims.z < 0 || !std::isfinite(scale) || scale <= 0 || scale > 100 ||
            count > 64 || active.size() + count * 2 > 256 || !std::isfinite(now) || now < 0)
            error = "Invalid flesh-gib request or capacity exhausted";
        else
        {
            // Selected high detail: the source's (DBYTE)1.5 is exactly one.
            if (integer(random_, 0, 99) < 20)
                count *= 2;
            report.id = next_event_++;
            report.adjusted = count;
            for (unsigned i = 0; i < count; ++i)
            {
                FleshGib g;
                g.position = pos;
                g.last_time = now;
                const auto half = vector_scale(dims, .8f);
                if (half.x)
                    g.position.x += draw(random_, -half.x, half.x);
                if (half.z)
                    g.position.z += draw(random_, -half.z, half.z);
                if (half.y)
                    g.position.y += draw(random_, -half.y / 2, half.y);
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
                // B2_LITE NRES limits model and sound choices to 1..3.
                const int model = integer(random_, 1, 3);
                g.bounces = integer(random_, 0, 4) == 4 ? 1 : 0;
                const int sound = integer(random_, 1, 3);
                g.model_path = "models/gibs/flesh/gib" + std::to_string(model) + ".abc";
                g.skin_path = "skins/gibs/flesh/gib" + std::to_string(model) + ".dtx";
                g.sound_path = "sounds/gibs/flesh/gib_impact" + std::to_string(sound) + ".wav";
                g.scale = {draw(random_, .8f, 1.35f) * scale, draw(random_, .8f, 1.35f) * scale,
                           draw(random_, .8f, 1.35f) * scale};
                if (!models.load(g.model_path, g.model, error) || !resources.find(g.skin_path))
                {
                    if (error.empty())
                        error = "Flesh-gib skin not indexed";
                    break;
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
                const auto hi =
                    vector_add(vector_scale(forward, 4 * speed), vector_scale(vector_add(right, up), speed));
                g.velocity = {draw(random_, lo.x, hi.x), draw(random_, lo.y, hi.y), draw(random_, lo.z, hi.z)};
                ContainerCode code = CC_NOTHING;
                if (!server.GetPointContainerCode(g.position, code))
                {
                    error = "Flesh-gib contents query unavailable";
                    break;
                }
                g.liquid = IsLiquid(code);
                g.id = next_id_++;
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
            g.last_time = now;
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
            const auto result =
                vector_dot(delta, delta) < 1e-12f ? SegmentResult::Miss : server.CastWorldRay(g.position, next, hit);
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
                server.PlayClientSound(request, next, sound_error);
                g.sound_error = sound_error;
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
