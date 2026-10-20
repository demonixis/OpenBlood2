// Selected original ClientShellDLL/WarpGateFX.cpp and BaseParticleSystemFX color rules.
#include "WarpGateFX.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
namespace
{
    bool range(float x, float lo, float hi)
    {
        return std::isfinite(x) && x >= lo && x <= hi;
    }
    bool pose_ok(NodePose p)
    {
        return physics_vector(p.position) && physics_vector(p.rotation.m_Vec) && std::isfinite(p.rotation.m_Spin) &&
               std::abs(vector_dot(p.rotation.m_Vec, p.rotation.m_Vec) + p.rotation.m_Spin * p.rotation.m_Spin - 1) <
                   .001f;
    }
    float ratio(float elapsed, float duration)
    {
        return duration ? elapsed / duration : 1.f;
    }
    float draw(std::minstd_rand& rng, float lo, float hi)
    {
        return lo + (hi - lo) * float(rng() - rng.min()) / float(rng.max() - rng.min());
    }
    void turn(DRotation& rotation, DVector axis, float angle)
    {
        DRotation delta;
        delta.m_Vec = vector_scale(axis, std::sin(angle / 2) / std::sqrt(vector_dot(axis, axis)));
        delta.m_Spin = std::cos(angle / 2);
        rotation = compose_rotation(delta, rotation);
    }
} // namespace
bool OpenBlood2::valid_warp_gate(const WarpGateSettings& s, std::string& error)
{
    error.clear();
    const auto path_ok = [](const std::string& p) { return p.size() <= 1000 && p.find('\0') == p.npos; };
    if (!range(s.ramp_up, 0, 1e6f) || !range(s.ramp_down, 0, 1e6f) || !range(s.sprite.min_scale, 0, 100) ||
        !range(s.sprite.max_scale, s.sprite.min_scale, 100) || !range(s.sprite.alpha, 0, 1) || s.sprite.ramp_up > 3 ||
        s.sprite.ramp_down > 3 || !path_ok(s.sprite.resource))
        error = "Invalid bounded warp sprite settings";
    for (const auto& p : s.particles)
    {
        bool color = true;
        const auto diff = vector_subtract(p.max_color, p.min_color);
        for (const auto c : {p.min_color, p.max_color})
            color = color && range(c.x, 0, 255) && range(c.y, 0, 255) && range(c.z, 0, 255);
        color = color && diff.x >= 0 && diff.y >= 0 && diff.z >= 0;
        // Original color helper omits the green/blue offsets. Bound the actual generated values.
        if (diff.x > 0)
            color = color && diff.y * p.max_color.x / diff.x <= 255 && diff.z * p.max_color.x / diff.x <= 255;
        if (!color || !range(p.radius, 0, 100000) || !range(p.position_radius, 0, 100000) ||
            !physics_vector(p.offset) || !physics_vector(p.rotations) || !range(p.min_velocity, -100000, 100000) ||
            !range(p.max_velocity, p.min_velocity, 100000) || !range(p.alpha, 0, 1) || !range(p.min_life, .001f, 60) ||
            !range(p.max_life, p.min_life, 60) || !range(p.delay, 0, 60) || !range(p.gravity, -100000, 100000) ||
            p.count > 512 || p.emit > 3 || p.ramp_up > 3 || p.ramp_down > 3 || !path_ok(p.resource))
            error = "Invalid bounded warp particle settings";
    }
    return error.empty();
}
bool CWarpGateSpriteFX::CreateObject(const WarpGateSettings& s, SpriteStore& store, NodePose owner, double now)
{
    *this = {};
    settings = s.sprite;
    up = s.ramp_up;
    down = s.ramp_down;
    pose = owner;
    if (!settings.align)
        pose.rotation = {};
    alpha = settings.alpha;
    last_time = now;
    return store.load(settings.resource, sprite, error);
}
bool CWarpGateSpriteFX::Update(double now, NodePose owner, bool on)
{
    if (!error.empty())
        return false;
    if (!sprite || !std::isfinite(now) || now < last_time || !pose_ok(owner) ||
        !advance_sprite(*sprite, track, now - last_time))
    {
        error = "Warp sprite pose/clock unavailable";
        return false;
    }
    last_time = now;
    if (now < next_update)
        return true;
    next_update = now + .001;
    pose.position = owner.position;
    if (settings.align)
        pose.rotation = owner.rotation;
    float elapsed = float(now - trigger_time);
    if (on)
    {
        if (first)
        {
            trigger_time = now;
            elapsed = 0;
            first = false;
            started = true;
        }
        if (elapsed <= up)
        {
            scale = settings.ramp_up & 1
                        ? settings.min_scale + (settings.max_scale - settings.min_scale) * ratio(elapsed, up)
                        : settings.max_scale;
            if (settings.ramp_up & 2)
                alpha = std::min(settings.alpha, settings.alpha * ratio(elapsed, up));
        }
        else
        {
            scale = settings.max_scale;
            alpha = settings.alpha;
        }
    }
    else if (started)
    {
        if (!first)
        {
            trigger_time = now;
            elapsed = 0;
            first = true;
        }
        if (elapsed <= down)
        {
            // Source divides the DOWN interpolation by RampUpTime.
            scale = settings.ramp_down & 1
                        ? settings.max_scale - (settings.max_scale - settings.min_scale) * ratio(elapsed, up)
                        : settings.max_scale;
            if (settings.ramp_down & 2)
                alpha = std::max(0.f, settings.alpha * (1 - ratio(elapsed, up)));
        }
        else
            started = false; // Source clears scale only on the following update.
    }
    else
        scale = 0;
    if (!std::isfinite(scale) || std::abs(scale) > 1e7f)
    {
        error = "Warp sprite scale exceeds finite presentation bounds";
        return false;
    }
    return true;
}
bool CWarpGateParticleFX::CreateObject(const WarpParticleSettings& s, float ramp_up, float ramp_down,
                                       SpriteStore& store, const Resources& resources, NodePose owner, double now)
{
    *this = {};
    settings = s;
    up = ramp_up;
    down = ramp_down;
    pose = owner;
    last_time = now;
    if (!settings.align)
        pose.rotation = {};
    texture = s.resource;
    if (!s.count)
        return true; // No particle can sample this texture; preserve the non-emitting system.
    std::string path;
    if (!resource_path(s.resource, path))
    {
        error = "Invalid warp particle resource";
        return false;
    }
    if (path.ends_with(".spr"))
    {
        if (!store.load(path, sprite, error))
            return false;
        texture = sprite->frames.front();
    }
    else if (!path.ends_with(".dtx") || !resources.find(path))
    {
        error = "Warp particle texture is absent or unsupported: " + s.resource;
        return false;
    }
    return true;
}
bool CWarpGateParticleFX::AddParticles(std::minstd_rand& rng, std::size_t budget)
{
    if (particles.size() > budget || settings.count > budget - particles.size())
    {
        error = "Warp particle capacity exceeded";
        return false;
    }
    auto basis = pose.rotation;
    if (!settings.align && !align_rotation({0, 1, 0}, basis))
    {
        error = "Warp particle vertical alignment unavailable";
        return false;
    }
    for (unsigned i = 0; i < settings.count; ++i)
    {
        auto right = rotate_vector(basis, {1, 0, 0});
        const auto forward = rotate_vector(basis, {0, 0, 1});
        turn(basis, forward, draw(rng, -std::numbers::pi_v<float>, std::numbers::pi_v<float>));
        if (settings.emit == 2 || settings.emit == 3)
            turn(basis, right, draw(rng, -std::numbers::pi_v<float>, std::numbers::pi_v<float>));
        right = vector_scale(right, 1 / std::sqrt(vector_dot(right, right)));
        const auto velocity = vector_scale(right, draw(rng, settings.min_velocity, settings.max_velocity));
        const auto start =
            vector_scale(right, settings.emit == 1 || settings.emit == 3 ? draw(rng, 0, settings.position_radius)
                                                                         : settings.position_radius);
        const float red = draw(rng, settings.min_color.x, settings.max_color.x);
        const auto diff = vector_subtract(settings.max_color, settings.min_color);
        auto color = diff.x > 0 ? DVector{red, diff.y * red / diff.x, diff.z * red / diff.x} : settings.min_color;
        color = vector_scale(color, 1 / 255.f);
        particles.push_back({start, velocity, draw(rng, settings.min_life, settings.max_life), color});
    }
    emitted += settings.count;
    ++bursts;
    return true;
}
bool CWarpGateParticleFX::Update(double now, NodePose owner, bool on, std::minstd_rand& rng, std::size_t budget)
{
    if (!error.empty())
        return false;
    if (!std::isfinite(now) || now < last_time || !pose_ok(owner) ||
        !advance_particles(particles, settings.gravity, now - last_time, error) ||
        (sprite && !advance_sprite(*sprite, track, now - last_time)))
    {
        if (error.empty())
            error = "Warp particle pose/clock unavailable";
        return false;
    }
    if (sprite && track.has_frame)
        texture = sprite->frames[track.frame]; // Retail retains texture on missing current frame.
    last_time = now;
    if (now < next_update)
        return true;
    next_update = now + .001;
    pose.position = vector_add(owner.position, settings.offset);
    if (!physics_vector(pose.position))
    {
        error = "Warp particle offset exceeds bounds";
        return false;
    }
    for (const auto [axis, angle] : std::array<std::pair<DVector, float>, 3>{
             {{{1, 0, 0}, settings.rotations.x}, {{0, 1, 0}, settings.rotations.y}, {{0, 0, 1}, settings.rotations.z}}})
        if (angle)
            turn(pose.rotation, rotate_vector(pose.rotation, axis), angle * std::numbers::pi_v<float>);
    float elapsed = float(now - trigger_time);
    bool emit = false;
    if (on)
    {
        if (first)
        {
            trigger_time = now;
            elapsed = 0;
            first = false;
            started = true;
        }
        if (elapsed <= up && (settings.ramp_up & 1))
            alpha = std::min(settings.alpha, settings.alpha * ratio(elapsed, up));
        // No source alpha snap after ramp up.
        emit = true;
    }
    else if (started)
    {
        if (!first)
        {
            trigger_time = now;
            elapsed = 0;
            first = true;
        }
        if (elapsed <= down)
        {
            if (settings.ramp_down & 1)
                alpha = std::max(0.f, settings.alpha * (1 - ratio(elapsed, up)));
        }
        else
            started = false;
        emit = true; // Original also emits on the update that ends ramp-down.
    }
    if (emit && elapsed >= last_add + settings.delay)
    {
        last_add = elapsed; // Intentionally never reset when the trigger clock restarts.
        return AddParticles(rng, budget);
    }
    return true;
}
