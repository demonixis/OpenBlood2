#include "ParticleStreamFX.h"
#include "engine/collision.h"
#include "engine/texture.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
namespace
{
    bool pose_valid(NodePose pose)
    {
        return physics_vector(pose.position) && physics_vector(pose.rotation.m_Vec) &&
               std::isfinite(pose.rotation.m_Spin) &&
               std::abs(vector_dot(pose.rotation.m_Vec, pose.rotation.m_Vec) +
                        pose.rotation.m_Spin * pose.rotation.m_Spin - 1) < .001f;
    }
    float draw(std::minstd_rand& r, float lo, float hi)
    {
        return lo + (hi - lo) * float(r() - std::minstd_rand::min()) /
                        float(std::minstd_rand::max() - std::minstd_rand::min());
    }
} // namespace
bool OpenBlood2::valid_particle_stream(const ParticleStreamSettings& s)
{
    auto bound = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
    auto color = [&](DVector v) { return bound(v.x, 0, 255) && bound(v.y, 0, 255) && bound(v.z, 0, 255); };
    auto path = [](const std::string& p) { return p.size() <= 1000 && p.find('\0') == p.npos; };
    return bound(s.radius, 0, 640000) && bound(s.position_radius, 0, 10000) && bound(s.min_velocity, 0, 10000) &&
           bound(s.max_velocity, s.min_velocity, 10000) && bound(s.spread, 0, 10000) && s.count <= 256 &&
           color(s.min_color) && color(s.max_color) && bound(s.alpha, 0, 1) && bound(s.min_life, 0, 600) &&
           bound(s.max_life, s.min_life, 600) && bound(s.ramp_time, 0, 600) && bound(s.delay, 0, 600) &&
           bound(s.gravity, -10000, 10000) && s.ramp_flags <= 15 && path(s.texture) &&
           bound(s.sound_radius, 0, 100000) && std::all_of(s.sounds.begin(), s.sounds.end(), path);
}
bool CParticleStreamFX::Init(const ParticleStreamSettings& s, std::string& failure)
{
    failure.clear();
    *this = {};
    if (!valid_particle_stream(s))
    {
        failure = "Invalid particle stream descriptor";
        return false;
    }
    settings = s;
    state = s.on ? 2 : 0;
    initialized_ = true;
    return true;
}
bool CParticleStreamFX::AddParticles(float now, std::minstd_rand& random, std::size_t budget, std::string& failure)
{
    if (!state)
        return true; // Source does not advance last-add time while off.
    unsigned count = settings.count;
    float offset = settings.position_radius, min_vel = settings.min_velocity, max_vel = settings.max_velocity,
          min_life = settings.min_life, max_life = settings.max_life, spread = settings.spread;
    if (state == 1 || state == 3)
    {
        float ratio;
        if (state == 1)
        {
            ratio = (now - trigger_time) / settings.ramp_time;
            if (ratio >= 1)
            {
                ratio = 1;
                state = 2;
            }
        }
        else
        {
            ratio = 1 - (now - trigger_time) / settings.ramp_time;
            if (ratio <= 0)
            {
                state = 0;
                return true;
            }
        }
        if (settings.ramp_flags & 1)
            count = unsigned(count * ratio);
        if (settings.ramp_flags & 2)
            offset *= ratio;
        if (settings.ramp_flags & 4)
        {
            min_vel *= ratio;
            max_vel *= ratio;
            spread *= ratio;
        }
        if (settings.ramp_flags & 8)
        {
            min_life *= ratio;
            max_life *= ratio;
        }
    }
    budget = std::min<std::size_t>(2048, budget);
    if (particles.size() > budget || count > budget - particles.size())
    {
        failure = "Particle stream particle capacity exhausted";
        return false;
    }
    for (unsigned i = 0; i < count; ++i)
    {
        Particle p;
        p.lifetime = draw(random, min_life, max_life);
        p.position = {draw(random, -offset, offset), draw(random, -offset, offset), draw(random, -offset, offset)};
        DVector jitter{draw(random, -spread, spread), draw(random, -spread, spread), draw(random, -spread, spread)};
        p.velocity = vector_add(vector_scale(direction, max_vel), jitter);
        const auto length = std::sqrt(vector_dot(p.velocity, p.velocity));
        const auto speed = draw(random, min_vel, max_vel);
        // A zero vector stays zero, avoiding the original normalize(0) undefined result.
        if (length > 0)
            p.velocity = vector_scale(p.velocity, speed / length);
        const auto red = draw(random, settings.min_color.x, settings.max_color.x);
        auto range = vector_subtract(settings.max_color, settings.min_color);
        range = {std::max(range.x, 0.f), std::max(range.y, 0.f), std::max(range.z, 0.f)};
        p.color = range.x <= 0 ? settings.min_color : DVector{red, range.y * red / range.x, range.z * red / range.x};
        p.color = {std::clamp(p.color.x / 255, 0.f, 1.f), std::clamp(p.color.y / 255, 0.f, 1.f),
                   std::clamp(p.color.z / 255, 0.f, 1.f)};
        particles.push_back(p);
    }
    last_add = now;
    ++bursts;
    emitted += count;
    return true;
}
bool CParticleStreamFX::CreateObject(const Resources& resources, NodePose pose, double now, std::minstd_rand& random,
                                     std::string& failure, std::size_t budget)
{
    failure.clear();
    if (!initialized_ || created_ || !pose_valid(pose) || !std::isfinite(now) || now < 0 || now > 1e7)
    {
        failure = "Invalid particle stream creation";
        return false;
    }
    // An authored empty HSTRING is not the null-HSTRING default texture branch.
    Texture image;
    if (!read_texture_resource(resources, settings.texture, image, failure))
        return false;
    texture = settings.texture;
    position = pose.position;
    direction = rotate_vector(pose.rotation, {0, 0, 1});
    last_time = now;
    if (state && !AddParticles(float(now), random, budget, failure))
        return false;
    created_ = true;
    return true;
}
bool CParticleStreamFX::Update(double now, const NodePose* pose, bool visible, std::minstd_rand& random,
                               std::size_t budget)
{
    error.clear();
    if (!created_ || !std::isfinite(now) || now < last_time || now > 1e7 || (pose && !pose_valid(*pose)))
    {
        error = "Invalid particle stream update";
        return false;
    }
    if (!pose)
        return false;
    if (!advance_particles(particles, settings.gravity, now - last_time, error))
        return false;
    last_time = now;
    if (now < next_update)
        return true;
    next_update = now + .001;
    ++updates;
    position = pose->position;
    direction = rotate_vector(pose->rotation, {0, 0, 1});
    const float time = float(now);
    if (visible && (state == 0 || state == 3))
    {
        trigger_time = time;
        state = settings.ramp_time ? 1 : 2;
    }
    else if (!visible && (state == 1 || state == 2))
    {
        trigger_time = time;
        state = settings.ramp_time ? 3 : 0;
    }
    // The original SFXMgr never reads the sound tail; no sound enters this client effect.
    return time <= last_add + settings.delay || AddParticles(time, random, budget, error);
}
