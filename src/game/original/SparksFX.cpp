// Selected original ClientShellDLL/SparksFX.cpp Init/CreateObject/AddSparks/Update.
#include "SparksFX.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    bool color(DVector c)
    {
        return physics_vector(c) && c.x >= 0 && c.x <= 255 && c.y >= 0 && c.y <= 255 && c.z >= 0 && c.z <= 255;
    }
    float mix(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
} // namespace
bool CSparksFX::CreateObject(const SparkSettings& s, const Resources& resources, double now, std::minstd_rand& rng,
                             std::string& failure)
{
    failure.clear();
    if (!physics_vector(s.position) || !physics_vector(s.direction) || vector_dot(s.direction, s.direction) < 1e-12f ||
        !color(s.color1) || !color(s.color2) || s.count > 255 || !std::isfinite(s.duration) || s.duration <= 0 ||
        s.duration > 10 || !std::isfinite(s.emission_radius) || s.emission_radius < 0 || s.emission_radius > 100 ||
        !std::isfinite(s.radius) || s.radius <= 0 || s.radius > 10000 || !std::isfinite(s.gravity) ||
        std::abs(s.gravity) > 10000 || !std::isfinite(now) || now < 0 || s.texture.size() > 1000 ||
        s.texture.find('\0') != std::string::npos)
    {
        failure = "Invalid sparks recipe";
        return false;
    }
    settings = s;
    if (settings.texture.empty())
        settings.texture = "spritetextures/particles/particle1.dtx";
    if (!resources.find(settings.texture))
    {
        failure = "Spark texture not indexed: " + settings.texture;
        return false;
    }
    const float speed = std::sqrt(vector_dot(s.direction, s.direction));
    direction = vector_scale(s.direction, 1 / speed);
    // Source uses the normalized component +/- the ORIGINAL magnitude, then clips
    // non-negligible components to their outgoing hemisphere. There is no collision.
    const auto axis = [speed](float d, float& lo, float& hi)
    {
        lo = d - speed;
        hi = d + speed;
        if (!(-.001f < d && d < .001f))
        {
            if (d < 0)
                hi = 0;
            else
                lo = 0;
        }
    };
    axis(direction.x, min_velocity.x, max_velocity.x);
    axis(direction.y, min_velocity.y, max_velocity.y);
    axis(direction.z, min_velocity.z, max_velocity.z);
    particles.clear();
    particles.reserve(s.count);
    for (unsigned i = 0; i < s.count; ++i)
    {
        float t[6];
        for (auto& v : t)
            v = float(rng() - rng.min()) / float(rng.max() - rng.min());
        // Bounded AddParticles adaptation, corroborated by later LithTech:
        // six draws, shared interpolation fractions for offsets/velocity/colors.
        Particle v;
        v.position = {mix(-s.emission_radius, s.emission_radius, t[0]),
                      mix(-s.emission_radius, s.emission_radius, t[1]),
                      mix(-s.emission_radius, s.emission_radius, t[2])};
        v.velocity = {mix(min_velocity.x, max_velocity.x, t[3]), mix(min_velocity.y, max_velocity.y, t[4]),
                      mix(min_velocity.z, max_velocity.z, t[5])};
        v.color = {mix(s.color1.x, s.color2.x, t[0]) / 255.f, mix(s.color1.y, s.color2.y, t[5]) / 255.f,
                   mix(s.color1.z, s.color2.z, t[3]) / 255.f};
        v.lifetime = s.duration;
        particles.push_back(v);
    }
    start_time = last_time = next_update = now;
    return true;
}
bool CSparksFX::Update(double now)
{
    if (!error.empty())
        return false;
    if (!std::isfinite(now) || now < last_time || now - last_time > 1)
    {
        error = "Invalid sparks clock";
        return false;
    }
    if (!advance_particles(particles, settings.gravity, now - last_time, error))
        return false;
    last_time = now;
    next_update = now + .001;
    ++updates;
    if (want_remove && now > start_time + settings.duration)
        return false;
    alpha = float((settings.duration - (now - start_time)) / settings.duration);
    return true; // Source retains an empty detached system until list eviction.
}
