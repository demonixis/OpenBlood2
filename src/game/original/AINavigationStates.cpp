// Original AI_Mgr.cpp:5874-5951,6445-6755. See AI_RECOVERY_CONTRACTS.md.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
bool AI_Mgr::NavigationFailure(std::string_view why)
{
    ++navigation_failures;
    navigation_error = why;
    return false;
}
bool AI_Mgr::NavigationDecision(int stim)
{
    const auto failures = navigation_failures;
    const bool ok = ComputeState(stim);
    if (!GetServerDE()->IsObjectAlive(m_hObject))
        return NavigationFailure("Navigation decision removed owner");
    if (navigation_failures != failures)
        return false;
    return ok || NavigationFailure(state_error);
}
bool AI_Mgr::NavigationState(int state)
{
    ++navigation_updates;
    if (navigation_busy)
        return NavigationFailure("Reentrant navigation state rejected");
    navigation_busy = true;
    navigation_state = state;
    navigation_error.clear();
    const auto failures = navigation_failures;
    const bool ok = NavigationStep(state);
    navigation_busy = false;
    return ok && navigation_failures == failures;
}
bool AI_Mgr::NavigationStep(int state)
{
    if (!LocomotionReady())
        return NavigationFailure(locomotion_error);
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return NavigationFailure("Navigation Metacmd overflow");
    if (state == StateJumpOver || state == StateCrawlUnder)
    {
        if (Metacmd == 2)
            return SetNewState(m_nLastState) || NavigationFailure(state_error);
        if (Metacmd != 1)
            return true;
        const bool ok = state == StateJumpOver             ? MC_Jump()
                        : m_nLastState == StateAttackClose ? MC_Fire_Crawl()
                                                           : MC_Crawl();
        return ok || NavigationFailure(locomotion_error);
    }
    if (state == StateFindAmmo)
        return AmmoStep();
    if (state == StateSearchSmell)
        return SmellStep();
    if (state == StateGuard)
        return GuardStep();
    const bool search = state == StateSearchVisual;
    const bool around = state == StateWalkAround || state == StateRunAround;
    const bool run = search || state == StateRunAround || state == StateRunTo;
    if (!physics_vector(search ? m_vTargetPos : m_vDestPos))
        return NavigationFailure("Invalid navigation destination");
    if (search || around)
    {
        const int stim = ComputeStimuli();
        if (stim < 0)
            return NavigationFailure(perception_error);
        if (search && !GetTarget())
            return NavigationDecision();
        if (stim > 0 && stim != 3)
        {
            SegmentHit hit;
            auto from = m_MoveObj.GetPos();
            if (search)
            {
                if (!RefreshTargetPosition())
                    return NavigationFailure(target_error);
            }
            else
                from.y = m_vDestPos.y;
            if (m_MoveObj.ClearToPoint(from, search ? m_vTargetPos : m_vDestPos, m_vDims, hit))
                return NavigationDecision(stim);
            if (m_MoveObj.outcome != CMovement::Outcome::Obstructed)
                return NavigationFailure(m_MoveObj.error);
        }
    }
    navigation_threshold = (std::hypot(m_vDims.x, m_vDims.z) + .1f) / 2;
    if ((Metacmd == 2 || Metacmd == 3) && navigation_plan_failed)
        return NavigationFailure("Failed navigation plan must be retried from phase 1");
    switch (Metacmd)
    {
        case 1:
        {
            navigation_plan_failed = true;
            const auto destination = search ? m_vTargetPos : m_vDestPos;
            if ((search || around) && !MC_FacePos(destination))
                return NavigationFailure(target_error);
            if (!m_MoveObj.CalculatePath(destination))
            {
                if (m_MoveObj.outcome != CMovement::Outcome::Blocked)
                    return NavigationFailure(m_MoveObj.error);
                // Ordinary source planning refusal changes state; partial paths are not followed.
                return SetNewState(StateIdle) || NavigationFailure(state_error);
            }
            navigation_plan_failed = false;
            if (!search && !around)
                ++Metacmd;
            return true;
        }
        case 2:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return SetNewState(search || around ? StateSearchSmell : m_nLastState) ||
                       NavigationFailure(state_error);
            return MC_FacePos(point) || NavigationFailure(target_error);
        }
        case 3:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return search || around ? NavigationDecision()
                                        : NavigationFailure("Missing point in source ToPos phase 3");
            bool edge = false;
            if (search)
            {
                if (!IsLedge(m_MoveObj.GetForwardVector(), edge))
                    return NavigationFailure(locomotion_error);
                if (edge)
                    return NavigationDecision();
            }
            point.y = m_MoveObj.GetPos().y;
            const auto delta = vector_subtract(point, m_MoveObj.GetPos());
            navigation_distance = std::hypot(delta.x, delta.y, delta.z);
            if (search)
            {
                // Source repeats IsLedge in the same call. The second check is
                // normally throttled false; preserve its ordering and result.
                if (!IsLedge(m_MoveObj.GetForwardVector(), edge))
                    return NavigationFailure(locomotion_error);
                if (edge)
                {
                    ++Metacmd;
                    return true;
                }
            }
            if (navigation_distance <= navigation_threshold)
            {
                const bool another = m_MoveObj.MoveToNextPathPoint();
                ++navigation_arrivals;
                if (!search && another && !IsLedge(m_MoveObj.GetForwardVector(), edge))
                    return NavigationFailure(locomotion_error);
                if (!another || edge)
                {
                    if (search || around)
                        return NavigationDecision();
                    ++Metacmd;
                }
                else
                    Metacmd = 2;
                return true;
            }
            return (run ? MC_Run() : MC_Walk()) || NavigationFailure(locomotion_error);
        }
        case 4:
            return search || around ? NavigationDecision()
                                    : SetNewState(m_nLastState) || NavigationFailure(state_error);
        default:
            return true;
    }
}
bool AI_Mgr::AI_STATE_JumpOverObj()
{
    return NavigationState(StateJumpOver);
}
bool AI_Mgr::AI_STATE_CrawlUnderObj()
{
    return NavigationState(StateCrawlUnder);
}
bool AI_Mgr::AI_STATE_WalkAroundObj()
{
    return NavigationState(StateWalkAround);
}
bool AI_Mgr::AI_STATE_RunAroundObj()
{
    return NavigationState(StateRunAround);
}
bool AI_Mgr::AI_STATE_WalkToPos()
{
    return NavigationState(StateWalkTo);
}
bool AI_Mgr::AI_STATE_RunToPos()
{
    return NavigationState(StateRunTo);
}
bool AI_Mgr::AI_STATE_SearchVisualTarget()
{
    return NavigationState(StateSearchVisual);
}
