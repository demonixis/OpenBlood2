#include "SmokeImpactFX.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
bool CSmokeImpactFX::Init(const SmokeImpactSettings& input, std::string& failure)
{
    *this = {};
    failure.clear();
    auto scale_ok = [](DVector v) { return physics_vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0; };
    if (!physics_vector(input.position) || !physics_vector(input.normal) || !physics_vector(input.velocity) ||
        !physics_vector(input.deceleration) || !physics_vector(input.gravity) || !scale_ok(input.scale1) ||
        !scale_ok(input.scale2) || !scale_ok(input.color) || input.color.x > 1 || input.color.y > 1 ||
        input.color.z > 1 || !std::isfinite(input.offset) || !std::isfinite(input.duration) || input.duration <= 0 ||
        input.duration > 600 || !std::isfinite(input.delay) || input.delay < 0 || input.delay > 600 ||
        !std::isfinite(input.alpha) || input.alpha < 0 || input.alpha > 1 ||
        (input.resource && (input.resource->size() > 1000 || input.resource->find('\0') != std::string::npos)))
    {
        failure = "Invalid smoke descriptor (nonpositive/infinite durations unsupported)";
        return false;
    }
    settings = input;
    position = vector_add(input.position, vector_scale(input.normal, input.offset));
    if (!physics_vector(position))
    {
        failure = "Smoke normal offset exceeds position bounds";
        return false;
    }
    velocity = input.velocity;
    scale = input.scale1;
    delay = input.delay;
    // Original ignores bUpdateScale and compares only X/Y. bRotate is stored but unused.
    scale_changes = input.scale1.x != input.scale2.x || input.scale1.y != input.scale2.y;
    initialized_ = true;
    return true;
}
bool CSmokeImpactFX::CreateObject(SpriteStore& store, double now, std::string& failure)
{
    failure.clear();
    if (!initialized_ || sprite || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid smoke creation state/time";
        return false;
    }
    if (!store.load(settings.resource.value_or("Sprites\\smokepuff1.spr"), sprite, failure))
        return false;
    last_time = now;
    // Source creation only sets scale; actual color remains white/opaque until Update.
    return true;
}
bool CSmokeImpactFX::Update(double now)
{
    error.clear();
    if (!sprite || !std::isfinite(now) || now < last_time || !advance_sprite(*sprite, track, now - last_time))
    {
        error = "Invalid smoke clock/state";
        return false;
    }
    last_time = now;
    if (now < next_update)
        return true;
    next_update = now + .001; // Existing CSpecialFX callback cadence, independent of SPR frame clock.
    ++updates;
    if (start_delay == 0)
        start_delay = now;
    color = settings.color.x || settings.color.y || settings.color.z ? settings.color : DVector{1, 1, 1};
    if (delay > 0)
    {
        if (delay <= now - start_delay)
            delay = 0;
        alpha = 0;
        return true; // The activation tick still returns without starting the effect.
    }
    if (first_update)
    {
        first_update = false;
        start_time = now;
        scale = settings.scale1;
        alpha = settings.alpha;
    }
    const float ratio = static_cast<float>((now - start_time) / settings.duration);
    if (scale_changes)
    {
        scale = vector_add(settings.scale1, vector_scale(vector_subtract(settings.scale2, settings.scale1), ratio));
        scale.x = std::min(scale.x, settings.scale2.x);
        scale.y = std::min(scale.y, settings.scale2.y);
        scale.z = std::min(scale.z, settings.scale2.z);
    }
    if (settings.fade)
        alpha = std::max(0.f, settings.alpha - ratio * settings.alpha);
    if (settings.move)
    {
        velocity = vector_add(velocity, settings.deceleration);
        position = vector_add(vector_add(position, velocity), settings.gravity);
    }
    if (!physics_vector(position) || !physics_vector(velocity) || !physics_vector(scale) || !std::isfinite(ratio))
    {
        error = "Smoke update exceeds finite transform bounds";
        return false;
    }
    return now < start_time + settings.duration; // Final movement/fade happens before expiry.
}
