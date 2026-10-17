#include "BloodSplatFX.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
bool CBloodSplatFX::Init(const BloodSplatSettings& input, std::string& failure)
{
    failure.clear();
    *this = {};
    const auto q = input.rotation;
    const auto length = vector_dot(q.m_Vec, q.m_Vec) + q.m_Spin * q.m_Spin;
    if (!physics_vector(input.position) || !std::isfinite(length) || std::abs(length - 1) > .001f ||
        !std::isfinite(input.scale) || input.scale < 0 || input.scale > 1000 || !std::isfinite(input.grow_scale) ||
        input.grow_scale < 0 || input.grow_scale > .1f ||
        (input.resource && (input.resource->size() > 1000 || input.resource->find('\0') != std::string::npos)))
    {
        failure = "Invalid blood splat transform/growth/resource";
        return false;
    }
    settings = input;
    position = input.position;
    rotation = input.rotation;
    scale = {input.scale, input.scale, input.scale};
    grow_time = input.grow_scale ? 4.f : 0.f;
    initialized_ = true;
    return true;
}
bool CBloodSplatFX::CreateObject(SpriteStore& store, double now, std::minstd_rand& random, std::string& failure)
{
    failure.clear();
    if (!initialized_ || sprite || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid blood splat creation state/time";
        return false;
    }
    resource = settings.resource
                   ? *settings.resource
                   : "sprites/blood" + std::to_string(std::uniform_int_distribution<int>(1, 3)(random)) + ".spr";
    if (!store.load(resource, sprite, failure))
        return false;
    last_time = now;
    return true;
}
bool CBloodSplatFX::Term()
{
    shrinking = true;
    grow_time = 0;
    return false; // Source keeps the effect in the terminating list.
}
bool CBloodSplatFX::Update(double now, std::minstd_rand& random)
{
    error.clear();
    if (!sprite || !std::isfinite(now) || now < last_time || now - last_time > 60 ||
        !advance_sprite(*sprite, track, now - last_time))
    {
        error = "Invalid blood splat clock/state";
        return false;
    }
    const float elapsed = static_cast<float>(now - last_time);
    last_time = now;
    if (now < next_update)
        return true;
    ++updates;
    const float delta = std::min(elapsed, .25f);
    auto multiply = [&](float lo, float hi)
    {
        std::uniform_real_distribution<float> draw(lo, hi);
        scale.x *= draw(random);
        scale.y *= draw(random);
        scale.z *= draw(random);
    };
    if (settings.grow_scale && grow_time > 0 && !shrinking)
    {
        const float amount = settings.grow_scale * delta * 30.f;
        multiply(1, 1 + amount * delta); // Original delta squared, independent axis draws.
        grow_time -= delta;
    }
    else if (shrinking)
    {
        if (grow_time >= 5)
            return false;
        if (settings.grow_scale)
            multiply(1 - settings.grow_scale * delta * 30.f, 1);
        alpha = 1 - grow_time / 5; // Alpha uses the previous, uncapped termination clock.
        grow_time += elapsed;
    }
    if (!physics_vector(scale))
    {
        error = "Blood splat growth exceeds finite bounds";
        return false;
    }
    next_update = now + .001;
    return true;
}
