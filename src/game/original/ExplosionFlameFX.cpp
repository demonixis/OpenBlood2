#include "ExplosionFlameFX.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
bool CExplosionFlameFX::Init(const ExplosionFlameSettings& s, std::string& failure)
{
    failure.clear();
    *this = {};
    auto bounded = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
    bool valid = physics_vector(s.position) && physics_vector(s.normal) && bounded(s.radius, 0, 640000) &&
                 bounded(s.position_radius, 0, 10000) && bounded(s.gravity, -10000, 10000) &&
                 bounded(s.velocity, 0, 10000) && bounded(s.delay, 0, 600) && bounded(s.duration, .001f, 600) &&
                 bounded(s.ramp_up, 0, 600) && bounded(s.ramp_down, 0, 600) && bounded(s.alpha, 0, 1) && s.count > 0 &&
                 s.count <= 256 && s.fade <= 5 && s.ramp_flags <= 7 &&
                 (!s.texture || (s.texture->size() <= 1000 && s.texture->find('\0') == std::string::npos));
    for (unsigned i = 0; i < 3; ++i)
        valid = valid && bounded(s.colors[i].x, 0, 1) && bounded(s.colors[i].y, 0, 1) && bounded(s.colors[i].z, 0, 1) &&
                bounded(s.life_offsets[i], 0, 600) && bounded(s.lifetimes[i], 0, 600) &&
                s.lifetimes[i] >= s.life_offsets[i] && s.lifetimes[i] + s.life_offsets[i] <= 600;
    if (!valid)
    {
        failure = "Invalid explosion flame descriptor";
        return false;
    }
    settings = s;
    initialized_ = true;
    return true;
}
bool CExplosionFlameFX::AddParticles(std::minstd_rand& random, std::size_t budget, std::string& failure)
{
    // Original three AddParticles calls are preflighted as one bounded burst.
    const auto count = std::size_t(settings.count) * 6;
    budget = std::min(budget, std::size_t(2048));
    if (particles.size() > budget || count > budget - particles.size())
    {
        failure = "Explosion flame particle capacity exhausted";
        return false;
    }
    auto draw = [&]
    { return float(random() - std::minstd_rand::min()) / float(std::minstd_rand::max() - std::minstd_rand::min()); };
    const float third = settings.position_radius / 3;
    const float radii[]{third, third * 2, settings.position_radius};
    for (unsigned zone = 0; zone < 3; ++zone)
        for (unsigned i = 0; i < settings.count * (zone + 1); ++i)
        {
            // Replacement bulk service follows later LithTech's six draws, including
            // its shared velocity.x/lifetime sample; no retail RNG parity is claimed.
            std::array<float, 6> t;
            for (auto& v : t)
                v = draw();
            const auto offset = [&](unsigned n) { return -radii[zone] + 2 * radii[zone] * t[n]; };
            const auto speed = [&](unsigned n) { return -settings.velocity + 2 * settings.velocity * t[n]; };
            particles.push_back(
                {{offset(0), offset(1), offset(2)},
                 {speed(3), speed(4), speed(5)},
                 settings.lifetimes[zone] - settings.life_offsets[zone] + 2 * settings.life_offsets[zone] * t[3],
                 settings.colors[zone]});
        }
    ++bursts;
    emitted += count;
    return true;
}
bool CExplosionFlameFX::CreateObject(const Resources& resources, double now, std::minstd_rand& random,
                                     std::string& failure, std::size_t budget)
{
    failure.clear();
    if (!initialized_ || created_ || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid explosion flame creation state/time";
        return false;
    }
    texture = settings.texture.value_or("SpriteTextures\\drop32_1.dtx");
    if (!resources.find(texture))
    {
        failure = "Explosion flame texture is not indexed: " + texture;
        return false;
    }
    if (!AddParticles(random, budget, failure))
        return false;
    alpha = settings.fade ? 1 : settings.alpha;
    add_time = settings.delay;
    start_time = last_time = now;
    created_ = true;
    return true;
}
bool CExplosionFlameFX::Update(double now, std::minstd_rand& random, std::size_t budget)
{
    error.clear();
    if (!created_ || !std::isfinite(now) || now < last_time)
    {
        error = "Invalid explosion flame clock/state";
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
        error = "Explosion flame time overflow";
        return false;
    }
    if (time >= add_time)
    {
        if (!AddParticles(random, budget, error))
            return false;
        add_time += settings.delay; // One burst, not a catch-up loop or now+delay.
    }
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
    // Original normal, ramp times and flags are stored but unused in these methods.
    return time < settings.duration;
}
