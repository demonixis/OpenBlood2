// Selected original ClientShellDLL/FlashlightFX.cpp client calculations.
#include "client/flashlight.h"
#include "client/effect_queries.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
bool CFlashlightFX::Update(const ClientWorld& world, const ClientCollisionWorld* scene, ClientObjectId child,
                           ClientObjectId character)
{
    ++updates;
    source = child;
    owner = character;
    visible = false;
    error.clear();
    if (!source.object)
        return true;
    const auto* light_source = world.get(source);
    if (!light_source || !world.get(owner))
        return true; // Original removed SFX has no visible client light.
    if (!(light_source->user_flags & 1))
        return true;
    const auto position = light_source->pose.position;
    const auto rotation = light_source->pose.rotation;
    DVector to;
    hit = {};
    result = bound_collision_ray(position, rotate_vector(rotation, {0, 0, 1}), to);
    if (result != SegmentResult::Invalid)
    {
        if (!scene || scene->frame().world != world.frame().world || !scene->frame().sequence)
            result = SegmentResult::Unavailable;
        else if (result == SegmentResult::Hit)
            result = scene->intersect(position, to, owner, hit);
    }
    if (result == SegmentResult::Miss)
        return true;
    if (result != SegmentResult::Hit)
    {
        error = "Flashlight ray unavailable";
        return false;
    }
    shade_position = vector_subtract(position, hit.point);
    distance = std::max(10.f, std::sqrt(vector_dot(shade_position, shade_position)));
    // GetPointShade is unavailable; preserve the source's explicit failure fallback.
    // No successful shade service or baked-lighting parity is claimed.
    shade_available = false;
    minimum_intensity = 132.f / 255;
    const float intensity = std::max(minimum_intensity, (1000.f - distance / 2) / 1000);
    light = {hit.point, {intensity, intensity, intensity}, distance * .4f};
    visible = true;
    return true;
}
PointLight CFlashlightFX::Current(const ClientWorld& world) const
{
    const auto* light_source = world.get(source);
    return visible && light_source && world.get(owner) && (light_source->user_flags & 1) ? light : PointLight{};
}
