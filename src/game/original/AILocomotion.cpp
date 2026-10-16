// Original AI_Mgr obstruction/ledge queries and MC_Walk/Run/Fire_Walk/Fire_Run.
// Checked typed boundaries and missing shade recovery: AI_LOCOMOTION_CONTRACTS.md.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
namespace
{
    struct Busy
    {
        bool& flag;
        explicit Busy(bool& f) : flag(f)
        {
            flag = true;
        }
        ~Busy()
        {
            flag = false;
        }
    };
} // namespace
bool AI_Mgr::LocomotionFailure(std::string_view why)
{
    ++locomotion_failures;
    locomotion_error = why;
    return false;
}
bool AI_Mgr::LocomotionReady()
{
    if (!GetServerDE() || !GetServerDE()->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vDims) || m_vDims.x <= 0 || m_vDims.y <= 0 || m_vDims.z <= 0)
        return LocomotionFailure("Locomotion requires a live sampled CMovement and positive finite dimensions");
    return true;
}
bool AI_Mgr::CheckObstructed(DVector, float speed, bool& out)
{
    out = false;
    if (!LocomotionReady() || !std::isfinite(speed) || speed < 0 || speed > 1e7f)
        return LocomotionFailure("Invalid obstruction speed or movement cache");
    DVector velocity;
    if (GetServerDE()->GetVelocity(m_hObject, &velocity) != DE_OK || !physics_vector(velocity))
        return LocomotionFailure("Obstruction velocity unavailable");
    obstruction_velocity = std::hypot(velocity.x, velocity.y, velocity.z);
    obstruction_vertical = std::abs(m_MoveObj.GetLastPos().y - m_MoveObj.GetPos().y);
    ++obstruction_checks;
    // Source ignores the direction and its computed total displacement.
    out = obstructed = obstruction_velocity < speed * .1f && obstruction_vertical <= .01f;
    return true;
}
int AI_Mgr::CalculateObstruction()
{
    block_flags = 0;
    obstacle_rays = {};
    if (!LocomotionReady())
        return -1;
    ++obstacle_queries;
    const auto pos = m_MoveObj.GetPos();
    const float feeler = (std::hypot(m_vDims.x, m_vDims.z) + 1) * 2;
    const float side = (m_vDims.x + m_vDims.z) / 2;
    for (unsigned i = 0; i < obstacle_rays.size(); ++i)
    {
        auto& ray = obstacle_rays[i];
        const float offset = i < 3 ? side : i < 6 ? 0 : -side;
        ray.from = vector_add(pos, vector_scale(m_MoveObj.GetRightVector(), offset));
        ray.from.y += i % 3 == 0 ? m_vDims.y - 1 : i % 3 == 2 ? 1 - m_vDims.y : 0;
        ray.to = vector_add(ray.from, vector_scale(m_MoveObj.GetForwardVector(), feeler));
        ray.result = GetServerDE()->IntersectSegment(ray.from, ray.to, nullptr, ray.hit, false,
                                                     SegmentObjects::SolidOnly, SegmentFilter::None);
        if (ray.result == SegmentResult::Invalid || ray.result == SegmentResult::Unavailable)
        {
            LocomotionFailure("Obstruction segment service failed; flags are partial");
            return -1;
        }
        // Source tests self AFTER the first hit, without recasting past it.
        if (ray.result == SegmentResult::Hit && ray.hit.object != m_hObject)
            block_flags |= (1u << (i % 3)) | (0x10u << (i / 3));
    }
    return static_cast<int>(block_flags);
}
bool AI_Mgr::NavigateObstacle()
{
    const int flags = CalculateObstruction();
    if (flags < 0)
        return false;
    if (!flags)
        return SetNewState(m_nLastState) || LocomotionFailure(state_error);
    if (m_nState == StateSearchVisual || m_nState == StateSearchSmell)
        return SetNewState(m_nState) || LocomotionFailure(state_error);
    // Preserve the source else-if structure: an enabled upper/lower branch can
    // consume the decision even when other rows prevent crawl/jump selection.
    if ((flags & 1) && (m_dwFlags & 1))
    {
        if (!(flags & 6))
            return SetNewState(StateCrawlUnder) || LocomotionFailure(state_error);
        return true;
    }
    if ((flags & 4) && (m_dwFlags & 2))
    {
        if (!(flags & 3))
            return SetNewState(StateJumpOver) || LocomotionFailure(state_error);
        return true;
    }
    // Source FindPosAroundObj needs the actual GetPointShade service. Do not
    // substitute a guessed position or successful RunAround state transition.
    return LocomotionFailure("FindPosAroundObj/GetPointShade obstacle recovery is not integrated");
}
bool AI_Mgr::IsLedge(DVector direction, bool& out)
{
    out = false;
    ledge_sampled = false;
    if (!LocomotionReady() || !physics_vector(direction) || !std::isfinite(last_ledge_check) ||
        !std::isfinite(GetServerDE()->GetTime()))
        return LocomotionFailure("Invalid ledge request/cache/clock");
    // Throttled calls return false, not the previous query result.
    if (GetServerDE()->GetTime() - last_ledge_check < .05f)
        return true;
    const float distance = std::hypot(m_vDims.x, m_vDims.z);
    ledge_ray = {};
    ledge_ray.from = vector_add(m_MoveObj.GetPos(), vector_scale(direction, distance * 1.25f));
    ledge_ray.to = ledge_ray.from;
    ledge_ray.to.y -= m_vDims.y * 2;
    ++ledge_queries;
    ledge_ray.result = GetServerDE()->IntersectSegment(ledge_ray.from, ledge_ray.to, nullptr, ledge_ray.hit, false,
                                                       SegmentObjects::SolidOnly, SegmentFilter::None);
    if (ledge_ray.result == SegmentResult::Invalid || ledge_ray.result == SegmentResult::Unavailable)
        return LocomotionFailure("Ledge segment service failed");
    // A failed typed query must not turn the next call into a successful
    // throttled false. Only a completed hit/miss consumes the source interval.
    last_ledge_check = GetServerDE()->GetTime();
    out = ledge = ledge_ray.result == SegmentResult::Miss;
    ledge_sampled = true;
    return true;
}
bool AI_Mgr::Locomotion(bool run, bool fire)
{
    ++locomotion_calls;
    if (locomotion_busy)
        return LocomotionFailure("Reentrant autonomous locomotion rejected");
    Busy busy(locomotion_busy);
    const auto failures = locomotion_failures;
    locomotion_error.clear();
    if (!LocomotionReady())
        return false;
    auto* server = GetServerDE();
    const float speed = run ? m_fRunSpeed : m_fWalkSpeed;
    const float time = server->GetTime();
    if (!std::isfinite(speed) || speed < 0 || speed > 1e7f || !std::isfinite(time))
        return LocomotionFailure("Invalid autonomous speed/clock");
    const int command = fire ? (run ? 9 : 8) : (run ? 2 : 1);
    locomotion_command = command;
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        auto* weapon = m_InventoryMgr.CurrentWeapon();
        const unsigned type = weapon ? weapon->data.m_nFireType : fire ? TYPE_PISTOL : TYPE_MELEE;
        if (!m_pAnim_Sound || type >= 9)
            return LocomotionFailure("Locomotion animation table unavailable");
        DDWORD clip = fire ? (run ? m_pAnim_Sound->m_nAnim_FIRE_RUN[type] : m_pAnim_Sound->m_nAnim_FIRE_WALK[type])
                           : (run ? m_pAnim_Sound->m_nAnim_RUN[type] : m_pAnim_Sound->m_nAnim_WALK[type]);
        if (!fire && !run && weapon && (m_nInjuredLeg == 10 || m_nInjuredLeg == 14))
        {
            const bool pistol = type == TYPE_PISTOL || type == TYPE_MELEE;
            clip = m_nInjuredLeg == 10 ? (pistol ? m_pAnim_Sound->m_nAnim_WALK_INJURED_LLEG_PISTOL
                                                 : m_pAnim_Sound->m_nAnim_WALK_INJURED_LLEG_RIFLE)
                                       : (pistol ? m_pAnim_Sound->m_nAnim_WALK_INJURED_RLEG_PISTOL
                                                 : m_pAnim_Sound->m_nAnim_WALK_INJURED_RLEG_RIFLE);
        }
        m_fTimeStart = fire ? 0 : time;
        if (fire)
        {
            if (weapon)
                m_fAttackLoadTime = weapon->data.m_fReloadTime;
            m_fLoadTimeStart = 0;
        }
        if (!SetAnimation(clip))
        {
            if (animation_blocked)
                return SetNewState(StateCrawlUnder) || LocomotionFailure(state_error);
            return LocomotionFailure(state_error);
        }
        server->SetModelLooping(m_hObject, DTRUE);
        if (!Move(m_MoveObj.GetForwardVector(), !fire && !run && m_nInjuredLeg ? speed / 3 : speed))
            return LocomotionFailure(script_motion_error);
        m_bAnimating = true;
        m_nCurMetacmd = command;
        if (fire)
            server->SetNextUpdate(m_hObject, .1f);
        return true;
    }
    bool blocked;
    if (!CheckObstructed(m_MoveObj.GetForwardVector(), speed, blocked))
        return false;
    if (blocked && !NavigateObstacle())
        return false;
    MovementState support;
    if (!server->GetMovementState(m_hObject, support))
        return LocomotionFailure("Locomotion support query unavailable");
    if (support.standing && (server->GetObjectFlags(m_hObject) & FLAG_GRAVITY))
        if (!Move(m_MoveObj.GetForwardVector(), speed))
            return LocomotionFailure(script_motion_error);
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return LocomotionFailure("Locomotion Metacmd overflow");
    if (fire)
    {
        if (!std::isfinite(m_fLoadTimeStart) || !std::isfinite(m_fAttackLoadTime) || m_fAttackLoadTime < 0)
            return LocomotionFailure("Moving fire cadence invalid");
        if (time > m_fLoadTimeStart + m_fAttackLoadTime)
        {
            if (Fire())
                m_fLoadTimeStart = server->GetTime();
            else if (fire_result == FireResult::Blocked || !m_InventoryMgr.CurrentWeapon())
            {
                m_bAnimating = false;
                ++Metacmd;
            }
            else
                return LocomotionFailure("Moving fire required shot service failed");
        }
    }
    if (!server->IsObjectAlive(m_hObject))
        return LocomotionFailure("Locomotion owner removed during shot callback");
    if (locomotion_failures != failures)
        return false;
    std::string name;
    if (!server->GetModelAnimationName(m_hObject, server->GetModelAnimation(m_hObject), name))
        return LocomotionFailure("Locomotion playback unavailable");
    if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        if (!StopVelocity())
            return LocomotionFailure(state_error);
        m_bAnimating = false;
        ++Metacmd;
        ++animation_finishes;
    }
    return true;
}
bool AI_Mgr::MC_Walk()
{
    return Locomotion(false, false);
}
bool AI_Mgr::MC_Run()
{
    return Locomotion(true, false);
}
bool AI_Mgr::MC_Fire_Walk()
{
    return Locomotion(false, true);
}
bool AI_Mgr::MC_Fire_Run()
{
    return Locomotion(true, true);
}
