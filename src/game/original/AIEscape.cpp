// Original CultistAI.cpp:460-550, CivilianAI.cpp:374-489 and AI_Mgr.cpp:5355-5379,6275-6350.
// Checked adaptations and diagnostic scope: AI_ESCAPE_CONTRACTS.md.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;

bool AI_Mgr::MC_Taunt_Beg()
{
    return TauntMovement(6, 17);
}
bool AI_Mgr::MC_Taunt_Bold()
{
    return TauntMovement(4, 18);
}
// Original AI_Mgr.cpp:5355-5411: both taunts start with a rightward velocity
// write, then wait for playback without refreshing or stopping that velocity.
bool AI_Mgr::TauntMovement(unsigned slot, int command)
{
    ++locomotion_calls;
    if (locomotion_busy)
        return LocomotionFailure("Reentrant taunt command rejected");
    locomotion_busy = true;
    locomotion_error.clear();
    locomotion_command = command;
    // No callbacks occur in these typed clip/velocity operations. Keep original
    // SetAnimation -> nonloop -> rightward Move -> command state ordering.
    bool ok = LocomotionReady();
    if (ok && (!std::isfinite(m_fRollSpeed) || m_fRollSpeed < 0 || m_fRollSpeed > 1e7f ||
               Metacmd == std::numeric_limits<int>::max()))
        ok = LocomotionFailure("Invalid taunt roll speed/Metacmd");
    if (ok && (!m_bAnimating || m_nCurMetacmd != command))
    {
        if (!m_pAnim_Sound || !SetAnimation(m_pAnim_Sound->m_nAnim_TAUNT[slot]))
            ok = LocomotionFailure("Taunt animation table/clip/resize unavailable");
        else
        {
            GetServerDE()->SetModelLooping(m_hObject, DFALSE);
            ok = Move(m_MoveObj.GetRightVector(), m_fRollSpeed);
            if (ok)
            {
                m_bAnimating = true;
                m_nCurMetacmd = command;
            }
            else
                LocomotionFailure(script_motion_error);
        }
    }
    else if (ok)
    {
        std::string name;
        if (!GetServerDE()->GetModelAnimationName(m_hObject, GetServerDE()->GetModelAnimation(m_hObject), name))
            ok = LocomotionFailure("Taunt playback unavailable");
        else if (GetServerDE()->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            m_bAnimating = false;
            ++Metacmd;
            ++animation_finishes;
            // Source does not stop the lateral velocity here.
        }
    }
    locomotion_busy = false;
    return ok;
}
bool AI_Mgr::AI_STATE_Escape_RunAway()
{
    return StateFailure("Subclass escape action is not integrated");
}
bool AI_Mgr::AI_STATE_Escape_Hide()
{
    return StateFailure("Subclass hide action is not integrated");
}
bool AI_Mgr::EscapeRun(bool civilian)
{
    ++escape_updates;
    ++navigation_updates;
    if (navigation_busy)
        return NavigationFailure("Reentrant escape state rejected");
    navigation_busy = true;
    navigation_state = StateEscapeRun;
    navigation_error.clear();
    const auto failures = navigation_failures;
    const bool ok = EscapeRunStep(civilian);
    navigation_busy = false;
    return ok && navigation_failures == failures;
}
bool AI_Mgr::EscapeHide()
{
    ++escape_updates;
    ++navigation_updates;
    if (navigation_busy)
        return NavigationFailure("Reentrant hide state rejected");
    navigation_busy = true;
    navigation_state = StateEscapeHide;
    navigation_error.clear();
    const auto failures = navigation_failures;
    const bool ok = EscapeHideStep();
    navigation_busy = false;
    return ok && navigation_failures == failures;
}
bool AI_Mgr::EscapeRunStep(bool civilian)
{
    if (!LocomotionReady())
        return NavigationFailure(locomotion_error);
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return NavigationFailure("Escape Metacmd overflow");
    DVector dims = m_vDims;
    if (civilian)
    {
        if (GetServerDE()->GetObjectDims(m_hObject, &dims) != DE_OK || !physics_vector(dims) || dims.x <= 0 ||
            dims.y <= 0 || dims.z <= 0)
            return NavigationFailure("Civilian escape dimensions unavailable");
    }
    else if (Metacmd > 1)
    {
        const int stim = ComputeStimuli();
        if (stim < 0)
            return NavigationFailure(perception_error);
        if (stim > 0)
            return NavigationDecision(stim);
    }
    navigation_threshold = (std::hypot(dims.x, dims.z) + .1f) / 2;
    if ((Metacmd == 2 || Metacmd == 3) && navigation_plan_failed)
        return NavigationFailure("Failed escape plan must be retried from phase 1");
    switch (Metacmd)
    {
        case 1:
        {
            navigation_plan_failed = true;
            const auto hint =
                FindObjectInRadius(AIFindList::Exits, m_fSeeingDist, FindVisible | (civilian ? 0u : FindAvoidTarget));
            if (!find.valid)
                return NavigationFailure(find.error);
            if (!SetTrackedObject(hint))
                return NavigationFailure(target_error);
            if (!hint)
                return SetNewState(civilian ? StateEscapeHide : StateAttackFar) || NavigationFailure(state_error);
            if (!RefreshTrackedPosition())
                return NavigationFailure(target_error);
            if (civilian)
            {
                const auto d = vector_subtract(m_vTrackObjPos, m_MoveObj.GetPos());
                navigation_distance = std::hypot(d.x, d.y, d.z);
                if (navigation_distance <= navigation_threshold)
                    return SetNewState(StateEscapeHide) || NavigationFailure(state_error);
            }
            if (!MC_FacePos(m_vTrackObjPos))
                return NavigationFailure(target_error);
            if (civilian)
            {
                DRotation rotation;
                if (GetServerDE()->GetObjectRotation(hint, &rotation) != DE_OK)
                    return NavigationFailure("ExitHint rotation unavailable");
                const auto forward = rotate_vector({0, 0, 1}, rotation);
                const auto cached = m_MoveObj.GetForwardVector();
                escape_hint_dot = forward.x * cached.x + forward.y * cached.y + forward.z * cached.z;
                // The cache is not refreshed after facing: preserve the preceding sample.
                if (escape_hint_dot <= 0)
                    return SetNewState(StateEscapeHide) || NavigationFailure(state_error);
            }
            if (!m_MoveObj.CalculatePath(m_vTrackObjPos))
            {
                if (m_MoveObj.outcome != CMovement::Outcome::Blocked)
                    return NavigationFailure(m_MoveObj.error);
                return SetNewState(StateEscapeHide) || NavigationFailure(state_error);
            }
            navigation_plan_failed = false;
            return true;
        }
        case 2:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return SetNewState(civilian ? StateEscapeRun : StateEscapeHide) || NavigationFailure(state_error);
            return MC_FacePos(point) || NavigationFailure(target_error);
        }
        case 3:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return NavigationFailure("Missing source escape phase-3 point");
            point.y = m_MoveObj.GetPos().y;
            const auto d = vector_subtract(point, m_MoveObj.GetPos());
            navigation_distance = std::hypot(d.x, d.y, d.z);
            if (navigation_distance <= navigation_threshold)
            {
                ++navigation_arrivals;
                if (!m_MoveObj.MoveToNextPathPoint())
                    return MC_FacePos(m_vTargetPos) || NavigationFailure(target_error);
                Metacmd = 2;
                return true;
            }
            return MC_Run() || NavigationFailure(locomotion_error);
        }
        case 4:
            return NavigationDecision();
        default:
            return true;
    }
}
bool AI_Mgr::EscapeHideStep()
{
    if (!LocomotionReady())
        return NavigationFailure(locomotion_error);
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return NavigationFailure("Hide Metacmd overflow");
    if (!GetTarget())
        return NavigationDecision();
    if (!MC_FaceTarget())
        return NavigationFailure(target_error);
    --Metacmd;
    if (Metacmd == 1)
        return MC_Taunt_Beg() || NavigationFailure(locomotion_error);
    if (Metacmd == 2)
        return NavigationDecision();
    return true;
}
bool AI_Mgr::AI_STATE_GuardLocation()
{
    return NavigationState(StateGuard);
}
bool AI_Mgr::GuardStep()
{
    if (!physics_vector(m_vGuardPos))
        return NavigationFailure("Invalid guard position");
    const int stim = ComputeStimuli();
    if (stim < 0)
        return NavigationFailure(perception_error);
    if (stim == 3)
        return SetNewState(StateSearchSmell) || NavigationFailure(state_error);
    if (stim > 0)
        return NavigationDecision(stim);
    navigation_threshold = (std::hypot(m_vDims.x, m_vDims.z) + .1f) / 2;
    if ((Metacmd == 2 || Metacmd == 3) && navigation_plan_failed)
        return NavigationFailure("Failed guard plan must be retried from phase 1");
    switch (Metacmd)
    {
        case 1:
        {
            const auto d = vector_subtract(m_vGuardPos, m_MoveObj.GetPos());
            navigation_distance = std::hypot(d.x, d.y, d.z); // Guard initial distance is NOT flattened.
            if (navigation_distance <= navigation_threshold)
            {
                Metacmd = 4;
                return true;
            }
            navigation_plan_failed = true;
            if (!MC_FacePos(m_vGuardPos))
                return NavigationFailure(target_error);
            if (!m_MoveObj.CalculatePath(m_vGuardPos))
            {
                if (m_MoveObj.outcome != CMovement::Outcome::Blocked)
                    return NavigationFailure(m_MoveObj.error);
                return SetNewState(StateIdle) || NavigationFailure(state_error);
            }
            navigation_plan_failed = false;
            return true;
        }
        case 2:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return SetNewState(StateEscapeRun) || NavigationFailure(state_error);
            return MC_FacePos(point) || NavigationFailure(target_error);
        }
        case 3:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return NavigationDecision();
            bool edge;
            if (!IsLedge(m_MoveObj.GetForwardVector(), edge))
                return NavigationFailure(locomotion_error);
            if (edge)
                return NavigationDecision();
            point.y = m_MoveObj.GetPos().y;
            const auto d = vector_subtract(point, m_MoveObj.GetPos());
            navigation_distance = std::hypot(d.x, d.y, d.z);
            if (!IsLedge(m_MoveObj.GetForwardVector(), edge))
                return NavigationFailure(locomotion_error);
            if (edge)
            {
                ++Metacmd;
                return true;
            }
            if (navigation_distance <= navigation_threshold)
            {
                ++navigation_arrivals;
                if (!m_MoveObj.MoveToNextPathPoint())
                    return MC_FacePos(m_vTargetPos) || NavigationFailure(target_error);
                Metacmd = 2;
                return true;
            }
            return MC_Run() || NavigationFailure(locomotion_error);
        }
        case 4:
            return MC_Idle() || NavigationFailure(state_error);
        case 5:
            return NavigationDecision();
        default:
            return true;
    }
}
