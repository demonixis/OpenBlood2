#include "ExplosionLightFX.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
bool CExplosionLightFX::Init(const ExplosionLightSettings& input, std::string& failure)
{
    failure.clear();
    *this = {};
    auto color_valid = [](DVector c)
    { return physics_vector(c) && c.x >= 0 && c.x <= 1 && c.y >= 0 && c.y <= 1 && c.z >= 0 && c.z <= 1; };
    auto radius_valid = [](float r) { return std::isfinite(r) && r >= 0 && r <= 10000; };
    if (!physics_vector(input.position) || !color_valid(input.color1) || !color_valid(input.color2) ||
        !radius_valid(input.radius1) || !radius_valid(input.radius2) || !std::isfinite(input.duration) ||
        input.duration <= 0 || input.duration > 600 || !std::isfinite(input.delay) || input.delay < 0 ||
        input.delay > 600)
    {
        failure = "Invalid explosion light descriptor";
        return false;
    }
    settings = input;
    change_color_ =
        input.color1.x != input.color2.x || input.color1.y != input.color2.y || input.color1.z != input.color2.z;
    scale_ = input.radius1 != input.radius2;
    initialized_ = true;
    return true;
}
bool CExplosionLightFX::CreateObject(double now, std::string& failure)
{
    failure.clear();
    if (!initialized_ || created_ || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid explosion light creation state/time";
        return false;
    }
    color = settings.color1;
    delay = settings.delay;
    radius = delay ? 0 : settings.radius1;
    start_time = last_time = now;
    created_ = true;
    return true;
}
bool CExplosionLightFX::Update(double now)
{
    error.clear();
    if (!created_ || !std::isfinite(now) || now < last_time)
    {
        error = "Invalid explosion light clock/state";
        return false;
    }
    last_time = now;
    if (now < next_update)
        return true;
    ++updates;
    const float time = static_cast<float>(now - start_time), ratio = time / settings.duration;
    if (!std::isfinite(ratio))
    {
        error = "Explosion light elapsed time overflow";
        return false;
    }
    next_update = now + .001; // Original CSFXMgr scheduling, including the delay branch.
    if (delay)
    {
        if (time >= delay)
        {
            radius = settings.radius1;
            start_time = now; // Discard delay overshoot, then return without interpolation.
            delay = 0;
        }
        return true;
    }
    if (change_color_)
        color = {settings.color1.x + (settings.color2.x - settings.color1.x) * ratio,
                 settings.color1.y + (settings.color2.y - settings.color1.y) * ratio,
                 settings.color1.z + (settings.color2.z - settings.color1.z) * ratio};
    if (scale_)
        radius = settings.radius1 + (settings.radius2 - settings.radius1) * ratio;
    return time < settings.duration; // Final source interpolation precedes removal.
}
