#include "RotatingDoor.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;

DDWORD RotatingDoor::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    constexpr float radians = std::numbers::pi_v<float> / 180;
    if (message == MID_PRECREATE)
    {
        if (!Door::EngineMessageFn(message, data, reason))
            return 0;
        g_pServerDE->GetPropVector("RotationAngles", &rotation_angles);
        g_pServerDE->GetPropVector("RotationPoint", &rotation_point);
        GenericProp prop;
        if (g_pServerDE->GetPropGeneric("Rotation", &prop) == DE_OK)
            angles = prop.m_Vec;
        const auto expected = rotation_from_euler(angles);
        const auto actual = static_cast<ObjectCreateStruct*>(data)->m_Rotation;
        if (std::abs(vector_dot(expected.m_Vec, actual.m_Vec) + expected.m_Spin * actual.m_Spin) < .99999f)
            return 0;
        DBOOL push = DTRUE;
        g_pServerDE->GetPropBool("PushPlayerBack", &push);
        push_player_back = push != 0;
        if (!physics_vector(rotation_angles) || !physics_vector(rotation_point) || !physics_vector(angles))
            return 0;
        speed *= radians;
        closing_speed *= radians;
        return 1;
    }
    if (message == MID_INITIALUPDATE)
    {
        closed_angles = angles;
        open_angles = vector_add(angles, vector_scale(rotation_angles, radians));
        g_pServerDE->GetObjectPos(m_hObject, &original_position);
        // Retain the source's asymmetric < +1 degree comparison (including zero).
        const auto direction = [&](float offset) { return offset > radians ? -1.0f : offset < radians ? 1.0f : 0.0f; };
        open_direction = {direction(closed_angles.x - open_angles.x), direction(closed_angles.y - open_angles.y),
                          direction(closed_angles.z - open_angles.z)};
    }
    return Door::EngineMessageFn(message, data, reason);
}
bool RotatingDoor::CalcAngle(float& angle, float initial, float target, int direction, float speed, unsigned wave,
                             float dt)
{
    // Defined zero-span guard for the source's 0/0; retain its duplicated increment
    // and late endpoint clamp. This can overshoot for one update.
    if (initial == target)
    {
        angle = target;
        return true;
    }
    const float percent = 1 - (target - angle) / (target - initial);
    const float amount = GetWaveValue(speed, percent, wave) * dt;
    bool done = direction == 0;
    for (int pass = 0; pass < 2; ++pass)
    {
        if (direction > 0)
        {
            if (angle < target)
                angle += amount;
            else
            {
                angle = target;
                done = true;
            }
        }
        else if (direction < 0)
        {
            if (angle > target)
                angle -= amount;
            else
            {
                angle = target;
                done = true;
            }
        }
    }
    return done;
}
void RotatingDoor::Move(bool opening)
{
    if (opening && !started_moving)
    {
        started_moving = true;
        StartSound(open_busy, true);
        portal_open = true;
    }
    const auto initial = opening ? closed_angles : open_angles;
    const auto target = opening ? open_angles : closed_angles;
    const auto direction = vector_scale(open_direction, opening ? 1.0f : -1.0f);
    auto next = angles;
    const auto velocity = opening ? speed : closing_speed;
    const auto dt = g_pServerDE->GetFrameTime();
    const bool x = CalcAngle(next.x, initial.x, target.x, static_cast<int>(direction.x), velocity, waveform, dt);
    const bool y = CalcAngle(next.y, initial.y, target.y, static_cast<int>(direction.y), velocity, waveform, dt);
    const bool z = CalcAngle(next.z, initial.z, target.z, static_cast<int>(direction.z), velocity, waveform, dt);
    last_move = g_pServerDE->RotateWorldModel(m_hObject, {angles, next, original_position, rotation_point});
    if (last_move != MoverResult::Moved)
        return;
    angles = next;
    if (x && y && z)
    {
        if (opening)
            SetOpen();
        else
            SetClosed();
    }
}
