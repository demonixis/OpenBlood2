#include "ExplosionRingFX.h"
#include "engine/collision.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
namespace
{
    void around(DRotation& r, DVector axis, float angle)
    {
        const float length = std::sqrt(vector_dot(axis, axis));
        const float sine = std::sin(angle / 2) / length;
        DRotation turn;
        turn.m_Vec = {axis.x * sine, axis.y * sine, axis.z * sine};
        turn.m_Spin = std::cos(angle / 2);
        r = compose_rotation(turn, r);
    }
} // namespace
bool CExplosionRingFX::Init(const ExplosionRingSettings& s, std::string& failure)
{
    failure.clear();
    *this = {};
    auto bounded = [](float v, float low, float high) { return std::isfinite(v) && v >= low && v <= high; };
    if (!physics_vector(s.position) || !physics_vector(s.normal) || vector_dot(s.normal, s.normal) < 1e-12f ||
        !bounded(s.color.x, 0, 1) || !bounded(s.color.y, 0, 1) || !bounded(s.color.z, 0, 1) ||
        !bounded(s.radius, 0, 640000) || !bounded(s.position_radius, 0, 10000) || !bounded(s.velocity, -10000, 10000) ||
        !bounded(s.gravity, -10000, 10000) || !bounded(s.duration, .001f, 600) || !bounded(s.delay, 0, 600) ||
        !bounded(s.alpha, 0, 1) || !bounded(s.rotation, -10000, 10000) || !s.count || s.count > 256 || s.fade > 5 ||
        s.rotate > 5 || s.align > 1 ||
        (s.texture && (s.texture->size() > 1000 || s.texture->find('\0') != std::string::npos)))
    {
        failure = "Invalid explosion ring descriptor";
        return false;
    }
    settings = s;
    initialized_ = true;
    return true;
}
bool CExplosionRingFX::AddParticles()
{
    DRotation basis;
    if (!align_rotation(settings.align ? settings.normal : DVector{0, 1, 0}, basis))
        return false;
    const float step = 2 * std::numbers::pi_v<float> / settings.count;
    particles.clear();
    particles.reserve(settings.count);
    for (unsigned i = 0; i < settings.count; ++i)
    {
        auto right = rotate_vector(basis, {1, 0, 0});
        const auto forward = rotate_vector(basis, {0, 0, 1});
        around(basis, forward, step); // Cache right before rotating, as original AddParticles does.
        const float length = std::sqrt(vector_dot(right, right));
        right = {right.x / length, right.y / length, right.z / length};
        particles.push_back({{right.x * settings.position_radius, right.y * settings.position_radius,
                              right.z * settings.position_radius},
                             {right.x * settings.velocity, right.y * settings.velocity, right.z * settings.velocity},
                             settings.duration});
    }
    return true;
}
bool CExplosionRingFX::CreateObject(const Resources& resources, double now, std::string& failure)
{
    failure.clear();
    if (!initialized_ || created_ || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid explosion ring creation state/time";
        return false;
    }
    texture = settings.texture.value_or("SpriteTextures\\smoke64_2.dtx");
    if (!resources.find(texture))
    {
        failure = "Explosion particle texture is not indexed: " + texture;
        return false;
    }
    delay = settings.delay;
    alpha = delay ? 0 : settings.alpha;
    if (!delay && !AddParticles())
    {
        failure = "Explosion ring orientation failed";
        return false;
    }
    start_time = last_time = now;
    created_ = true;
    return true;
}
bool CExplosionRingFX::Update(double now)
{
    error.clear();
    if (!created_ || !std::isfinite(now) || now < last_time)
    {
        error = "Invalid explosion ring clock/state";
        return false;
    }
    if (!advance_particles(particles, settings.gravity, now - last_time, error))
        return false;
    last_time = now;
    if (now < next_update)
        return true;
    ++updates;
    next_update = now + .001;
    const float time = static_cast<float>(now - start_time), ratio = time / settings.duration;
    if (!std::isfinite(ratio))
    {
        error = "Explosion ring time overflow";
        return false;
    }
    if (delay)
    {
        if (time >= delay)
        {
            if (!settings.fade)
                alpha = settings.alpha;
            start_time = now;
            delay = 0;
            if (!AddParticles())
            {
                error = "Explosion ring orientation failed";
                return false;
            }
        }
        return true;
    }
    float turn = 0;
    switch (settings.rotate)
    {
        case 1:
            turn = settings.rotation;
            break;
        case 2:
            turn = settings.rotation * (1 - ratio);
            break;
        case 3:
            turn = 1 - (1 - settings.rotation) * ratio;
            break;
        case 4:
            turn = settings.rotation * ratio;
            break;
        case 5:
            turn = settings.rotation + (1 - settings.rotation) * ratio;
            break;
    }
    if (settings.rotate)
        around(rotation, settings.align ? settings.normal : DVector{0, 1, 0}, turn * std::numbers::pi_v<float>);
    switch (settings.fade)
    {
        case 1:
            alpha = settings.alpha * (1 - ratio);
            break;
        case 2:
            alpha = 1 - (1 - settings.alpha) * ratio;
            break;
        case 3:
            alpha = settings.alpha * ratio;
            break;
        case 4:
            alpha = settings.alpha + (1 - settings.alpha) * ratio;
            break;
        case 5:
            alpha = time < settings.duration / 2 ? settings.alpha * ratio * 2 : settings.alpha * 2 * (1 - ratio);
            break;
    }
    return time < settings.duration;
}
