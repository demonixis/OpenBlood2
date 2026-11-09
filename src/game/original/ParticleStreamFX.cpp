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
    // Later LithTech stores SetObjectColor alpha in an unsigned byte. Define the
    // wrapping explicitly instead of invoking an out-of-range float-to-byte cast.
    double byte_alpha = std::fmod(std::trunc(double(s.alpha) * 255), 256.);
    if (byte_alpha < 0)
        byte_alpha += 256;
    alpha = float(byte_alpha / 255.);
    state = s.on ? 2 : 0;
    initialized_ = true;
    return true;
}
bool CParticleStreamFX::Init(const RainSettings& rain, std::string& failure)
{
    if (!valid_rain(rain))
    {
        failure = "Invalid Rain descriptor";
        return false;
    }
    ParticleStreamSettings rendering;
    rendering.system = true;
    rendering.on = true;
    rendering.radius = 200 * rain.scale;
    rendering.gravity = rain.gravity ? -500.f : 0.f;
    rendering.texture = "SpriteTextures\\blooddrop2.dtx";
    if (!Init(rendering, failure))
        return false;
    rain_ = rain;
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
    if (settings.system)
    {
        system_rotation = pose.rotation;
        system_visible = settings.on;
    }
    if (!settings.system && state && !AddParticles(float(now), random, budget, failure))
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
    const auto dt = now - last_time;
    last_time = now;
    if (rain_)
        return RainUpdate(now, *pose, random, budget);
    if (settings.system)
        return SystemUpdate(now, dt, *pose, visible, random, budget);
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

bool CParticleStreamFX::RainUpdate(double now, const NodePose& pose, std::minstd_rand& random, std::size_t budget)
{
    if (first_system_update_)
    {
        first_system_update_ = false;
        rain_start_ = now;
        next_update = now + .01;
        return true;
    }
    if (now < next_update)
        return true;
    const auto& rain = *rain_;
    const double elapsed = now - rain_start_;
    if (rain.time_limit && elapsed > rain.time_limit)
        return false;
    position = pose.position;
    system_rotation = pose.rotation;
    // The source overwrites the authored direction with the owner's forward vector each tick.
    direction = rotate_vector(pose.rotation, {0, 0, 1});
    const bool trails = !rain.gravity || (!rain.spread && direction.x == 0 && direction.z == 0);
    const double rate = double(rain.density) * rain.dims.x * rain.dims.z / 250;
    const auto samples = static_cast<std::uint64_t>(std::floor(rate * elapsed));
    const auto demand = (samples - rain_samples_) * (trails ? 1u : 6u);
    rain_samples_ = samples;
    budget = std::min<std::size_t>(2048, budget);
    const auto room = particles.size() < budget ? budget - particles.size() : 0;
    const unsigned width = trails ? 6 : 1;
    const auto groups = std::min<std::uint64_t>(demand, room / width);
    dropped += (demand - groups) * width;
    particles.reserve(particles.size() + groups * width);
    for (std::uint64_t i = 0; i < groups; ++i)
    {
        Particle particle;
        particle.lifetime = rain.lifetime;
        particle.position = {draw(random, -rain.dims.x, rain.dims.x), draw(random, -rain.dims.y, rain.dims.y),
                             draw(random, -rain.dims.z, rain.dims.z)};
        particle.velocity = vector_scale(direction, draw(random, .8f, 1.2f));
        if (rain.spread)
        {
            particle.velocity.x += draw(random, -rain.spread, rain.spread);
            particle.velocity.z += draw(random, -rain.spread, rain.spread);
        }
        const DVector color{draw(random, rain.color1.x, rain.color2.x) / 255,
                            draw(random, rain.color1.y, rain.color2.y) / 255,
                            draw(random, rain.color1.z, rain.color2.z) / 255};
        constexpr float brightness[]{1, .8f, .6f, .4f, .2f, .2f};
        for (unsigned j = 0; j < width; ++j)
        {
            particle.color = vector_scale(color, brightness[j]);
            particles.push_back(particle);
            particle.position = vector_subtract(particle.position, vector_scale(direction, float(j + 3)));
        }
    }
    emitted += groups * width;
    ++updates;
    ++bursts;
    next_update = now + (rain.pulse > .001f ? rain.pulse * draw(random, .01f, 1) : .01f);
    return true;
}

// Selected ClientShellDLL/ParticleSystemFX: floor(rate * elapsed), no fractional carry or catch-up loop.
bool CParticleStreamFX::SystemUpdate(double now, double dt, const NodePose& pose, bool visible,
                                     std::minstd_rand& random, std::size_t budget)
{
    position = pose.position;
    const bool hidden_now = system_visible != visible && !visible;
    system_visible = visible;
    if (hidden_now)
    {
        system_last_ = now;
        next_update = now + system_wait_;
        return true;
    }
    if (first_system_update_)
    {
        system_last_ = now;
        next_update = now + .01;
        first_system_update_ = false;
    }
    if (now < next_update)
        return true;
    const double number = std::floor(settings.particles_per_second * (now - system_last_));
    budget = std::min<std::size_t>(2048, budget);
    if (particles.size() > budget || number > double(budget - particles.size()))
    {
        error = "ParticleSystem particle capacity exhausted";
        return false;
    }
    const auto count = static_cast<unsigned>(number);
    const float r = settings.position_radius, v = settings.spread;
    for (unsigned i = 0; i < count; ++i)
    {
        Particle p;
        float t[6];
        for (auto& value : t)
            value = draw(random, 0, 1);
        p.position = {-r + 2 * r * t[0], -r + 2 * r * t[1], -r + 2 * r * t[2]};
        p.velocity = {-v + 2 * v * t[3], settings.min_velocity + (settings.max_velocity - settings.min_velocity) * t[4],
                      -v + 2 * v * t[5]};
        // Later engine AddParticles reuses position/velocity samples for RGB and lifetime.
        const auto c = settings.min_color, d = vector_subtract(settings.max_color, c);
        p.color = {(c.x + d.x * t[0]) / 255, (c.y + d.y * t[5]) / 255, (c.z + d.z * t[3]) / 255};
        p.lifetime = settings.min_life;
        particles.push_back(p);
    }
    system_wait_ = settings.burst_wait > .001f ? settings.burst_wait * draw(random, .01f, 1) : .01;
    next_update = now + system_wait_;
    if (settings.rotation_velocity != 0)
        system_rotation =
            compose_rotation(system_rotation, rotation_from_euler({0, float(dt) * settings.rotation_velocity, 0}));
    system_last_ = now;
    ++updates;
    ++bursts;
    emitted += count;
    return true;
}
