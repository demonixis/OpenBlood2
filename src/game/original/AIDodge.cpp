// Selected original AI_Mgr::CalculateDodge and MC_Roll_Forward/Backward/Left/Right.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;

int AI_Mgr::CalculateDodge(DVector position)
{
    dodge_ray = {};
    if (!LocomotionReady() || !physics_vector(position) || !std::isfinite(m_fRollSpeed) || m_fRollSpeed < 0 ||
        m_fRollSpeed > 1e7f)
    {
        LocomotionFailure("Invalid dodge position, roll speed or movement cache");
        return -1;
    }
    auto direction = vector_subtract(position, m_MoveObj.GetPos());
    const auto length = std::hypot(direction.x, direction.y, direction.z);
    if (length < 1e-6f)
    {
        LocomotionFailure("Coincident dodge impact has no direction");
        return -1;
    }
    direction = vector_scale(direction, 1 / length);
    const auto right = m_MoveObj.GetRightVector(), forward = m_MoveObj.GetForwardVector();
    const float r = vector_dot(direction, right), l = -r, f = vector_dot(direction, forward), b = -f;
    int flags = 0;
    DVector away{};
    if (r > 0)
    {
        flags = 8;
        away = vector_scale(right, -1);
    }
    else if (l > 0)
    {
        flags = 4;
        away = right;
    }
    if (f > 0 && f > r && f > l)
    {
        flags = 2;
        away = vector_scale(forward, -1);
    }
    else if (b > 0 && b > r && b > l)
    {
        flags = 1;
        away = forward;
    }
    if (!flags)
    {
        LocomotionFailure("Vertical-only dodge impact leaves the original ray undefined");
        return -1;
    }
    dodge_ray.from = m_MoveObj.GetPos();
    dodge_ray.to = vector_add(dodge_ray.from, vector_scale(away, m_fRollSpeed));
    ++dodge_queries;
    // Original INTERSECT_OBJECTS includes nonsolid objects. The replacement
    // entry-face ray skips enclosing objects, like the existing vector-fire ray.
    dodge_ray.result =
        GetServerDE()->IntersectSegment(dodge_ray.from, dodge_ray.to, nullptr, dodge_ray.hit, false,
                                        SegmentObjects::All, SegmentFilter::None, SegmentStart::SkipInterior);
    if (dodge_ray.result == SegmentResult::Invalid || dodge_ray.result == SegmentResult::Unavailable)
    {
        LocomotionFailure("Dodge obstruction query unavailable");
        return -1;
    }
    return flags | (dodge_ray.result == SegmentResult::Hit ? 0x20 : 0x10);
}

bool AI_Mgr::MC_Roll_Left()
{
    return RollMovement(15);
}
bool AI_Mgr::MC_Roll_Right()
{
    return RollMovement(16);
}
bool AI_Mgr::MC_Roll_Forward(bool loop)
{
    return RollMovement(13, loop);
}
bool AI_Mgr::MC_Roll_Backward()
{
    return RollMovement(14);
}
// Original MC_ROLL_FORWARD 13 / BACKWARD 14 / LEFT 15 / RIGHT 16 share one body;
// only the direction, clip and the forward roll's looping argument differ.
bool AI_Mgr::RollMovement(int command, bool loop)
{
    ++locomotion_calls;
    if (locomotion_busy)
        return LocomotionFailure("Reentrant roll command rejected");
    locomotion_busy = true;
    locomotion_error.clear();
    locomotion_command = command;
    const auto step = [&]()
    {
        if (!LocomotionReady() || !m_pAnim_Sound || !std::isfinite(m_fRollSpeed) || m_fRollSpeed < 0 ||
            m_fRollSpeed > 1e7f || Metacmd > std::numeric_limits<int>::max() - 2)
            return LocomotionFailure("Roll requires a live cache, animation table and bounded speed/phase");
        auto* server = GetServerDE();
        const auto axis = command <= 14 ? m_MoveObj.GetForwardVector() : m_MoveObj.GetRightVector();
        const auto direction = vector_scale(axis, command == 14 || command == 15 ? -1.f : 1.f);
        if (!m_bAnimating || m_nCurMetacmd != locomotion_command)
        {
            const DDWORD clip = command == 13   ? m_pAnim_Sound->m_nAnim_ROLL_FORWARD
                                : command == 14 ? m_pAnim_Sound->m_nAnim_ROLL_BACK
                                : command == 15 ? m_pAnim_Sound->m_nAnim_ROLL_LEFT
                                                : m_pAnim_Sound->m_nAnim_ROLL_RIGHT;
            if (!SetAnimation(clip))
                return LocomotionFailure(state_error);
            server->SetModelLooping(m_hObject, loop ? DTRUE : DFALSE);
            if (!Move(direction, m_fRollSpeed))
                return LocomotionFailure(script_motion_error);
            m_bAnimating = true;
            m_nCurMetacmd = locomotion_command;
            return true;
        }
        bool blocked;
        if (!CheckObstructed(direction, m_fRollSpeed, blocked))
            return false;
        if (blocked)
        {
            m_bAnimating = false;
            ++Metacmd; // Obstruction does not skip support/playback processing.
        }
        MovementState support;
        if (!server->GetMovementState(m_hObject, support))
            return LocomotionFailure("Roll support query unavailable");
        if (support.standing && (server->GetObjectFlags(m_hObject) & FLAG_GRAVITY))
            if (!Move(direction, m_fRollSpeed))
                return LocomotionFailure(script_motion_error);
        std::string name;
        if (!server->GetModelAnimationName(m_hObject, server->GetModelAnimation(m_hObject), name))
            return LocomotionFailure("Roll playback unavailable");
        if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            if (!StopVelocity())
                return LocomotionFailure(state_error);
            m_bAnimating = false;
            ++Metacmd;
            ++animation_finishes;
        }
        return true;
    };
    const bool ok = step();
    locomotion_busy = false;
    return ok;
}
