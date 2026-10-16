#include "ExplosionSpriteFX.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
using namespace OpenBlood2;
bool CExplosionSpriteFX::Init(const ExplosionSpriteSettings& input, std::string& failure)
{
    failure.clear();
    *this = {};
    auto valid_scale = [](DVector v) { return physics_vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0; };
    if (!physics_vector(input.position) || !physics_vector(input.normal) || !valid_scale(input.scale1) ||
        !valid_scale(input.scale2) || !std::isfinite(input.duration) || input.duration <= 0 || input.duration > 600 ||
        !std::isfinite(input.alpha) || input.alpha < 0 || input.alpha > 1 || input.fade > 5 || input.align > 1 ||
        (input.resource && (input.resource->size() > 1000 || input.resource->find('\0') != std::string::npos)))
    {
        failure = "Invalid explosion sprite descriptor or unsupported random alignment";
        return false;
    }
    settings = input;
    // Original only writes true; explicit initialization removes its indeterminate false case.
    scale_changes_ = settings.scale1.x != settings.scale2.x || settings.scale1.y != settings.scale2.y ||
                     settings.scale1.z != settings.scale2.z;
    initialized_ = true;
    return true;
}
bool CExplosionSpriteFX::CreateObject(SpriteStore& store, double now, std::string& failure)
{
    failure.clear();
    if (!initialized_ || sprite || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid explosion sprite creation state/time";
        return false;
    }
    if (settings.align && !align_rotation(settings.normal, rotation))
    {
        failure = "Cannot align explosion sprite to a zero/invalid normal";
        return false;
    }
    const auto path = settings.resource ? *settings.resource : "Sprites\\Explosn.spr";
    if (!store.load(path, sprite, failure))
        return false;
    flags = FLAG_VISIBLE | FLAG_SPRITECHROMAKEY | (settings.align ? FLAG_ROTATEABLESPRITE : 0);
    scale = settings.scale1;
    alpha = settings.alpha;
    track = {};
    start_time = last_time = now;
    return true;
}
bool CExplosionSpriteFX::Update(double now)
{
    error.clear();
    if (!sprite || !std::isfinite(now) || now < last_time)
    {
        error = "Invalid explosion sprite clock/state";
        return false;
    }
    const float time = static_cast<float>(now - start_time), ratio = time / settings.duration;
    if (!std::isfinite(ratio) || !advance_sprite(*sprite, track, now - last_time))
    {
        error = "Explosion sprite animation clock failed";
        return false;
    }
    last_time = now;
    if (now < next_update)
        return true;
    ++updates;
    if (scale_changes_)
    {
        float amount = ratio;
        if (settings.waveform == 1)
            amount = std::sqrt(ratio);
        else if (settings.waveform == 2)
            amount = ratio * ratio;
        scale = {settings.scale1.x + (settings.scale2.x - settings.scale1.x) * amount,
                 settings.scale1.y + (settings.scale2.y - settings.scale1.y) * amount,
                 settings.scale1.z + (settings.scale2.z - settings.scale1.z) * amount};
    }
    // Apply the source final update before returning false at time >= duration.
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
        default:
            break;
    }
    if (time < settings.duration)
    {
        next_update = now + .001;
        return true;
    }
    return false;
}
