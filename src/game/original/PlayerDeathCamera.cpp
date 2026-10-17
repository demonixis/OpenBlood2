// Original PlayerCamera.cpp death orbit, optimal position and transition subset.
#include "PlayerDeathCamera.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>

bool CPlayerDeathCamera::Update(ServerDE& server, HOBJECT player, DVector eye)
{
    using namespace OpenBlood2;
    error.clear();
    DVector body;
    DRotation target;
    const auto elapsed = server.GetTime() - start;
    if (!active || !server.IsObjectAlive(player) || !std::isfinite(elapsed) || elapsed < 0 || !physics_vector(eye) ||
        server.GetObjectPos(player, &body) != DE_OK || server.GetObjectRotation(player, &target) != DE_OK)
    {
        error = "Death camera target/clock unavailable";
        return false;
    }
    angle = std::fmod(elapsed * .8f, 2 * std::numbers::pi_v<float>);
    // Source EulerRotateY then world X/Z components fed back through target
    // right/forward. Preserve this double basis use, including nonzero yaw.
    const auto orbit = rotate_vector(multiply_rotation(target, rotation_from_euler({0, angle, 0})), {0, 0, -108});
    anchor = vector_add(body, {0, 30, 0});
    requested = vector_add(anchor, rotate_vector(target, {orbit.x, 35, orbit.z}));
    result = server.IntersectSegment(anchor, requested, player, hit);
    if (result == SegmentResult::Invalid || result == SegmentResult::Unavailable)
    {
        error = "Death camera obstruction query failed";
        return false;
    }
    auto optimum = requested;
    if (result == SegmentResult::Hit && hit.fraction < 1)
        optimum = vector_add(hit.point, hit.normal); // Source one-unit plane separation.
    fraction = std::min(elapsed, 1.f);               // One second; source slide multiplier is 1.
    position = vector_add(eye, vector_scale(vector_subtract(optimum, eye), fraction));
    const auto aim = vector_subtract(eye, position);
    // At the first frame the source normalizes a zero vector. Retain the checked
    // target orientation instead of passing a NaN to the portable renderer.
    rotation = target;
    if (vector_dot(aim, aim) > 1e-12f && server.AlignRotation(rotation, aim) != DE_OK)
    {
        error = "Death camera look direction unavailable";
        return false;
    }
    return true;
}
