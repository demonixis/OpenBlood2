// Original Fanatic.cpp:236-439, 548-577 and prophet.cpp:322-490, 579-608: the
// AttackClose/AttackFar/Dodge bodies shared by Fanatic and Prophet. CultistAI
// keeps its own versions. See fanatic_prophet_contracts.md.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;

bool AI_Mgr::CrouchAttack(bool far, bool close_height_fires)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vTargetPos) || !physics_vector(m_vDims) || m_vDims.y <= 0 || !std::isfinite(m_fSeeingDist) ||
        m_fSeeingDist < 0 || m_fSeeingDist > 1e7f || !std::isfinite(m_fStimuli[5]) ||
        Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Attack requires live owner, cache and bounded state");
    ++crouch_attack_updates;
    const auto delta = vector_subtract(m_vTargetPos, m_MoveObj.GetPos());
    crouch_attack_height = std::abs(delta.y);
    // SCHLEGZ sanity check: the target was lost since the state was chosen.
    if (!GetTarget())
        return crouch_attack_height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    if (!server->IsObjectAlive(GetTarget()))
        return StateFailure("Attack target pending removal");
    crouch_attack_distance = m_fStimuli[0] = std::hypot(delta.x, delta.y, delta.z);
    const float distance = crouch_attack_distance, height = crouch_attack_height;
    const bool above = delta.y > 0, high = height > m_vDims.y;
    const bool lateral = (m_nDodgeFlags & 1) || (m_nDodgeFlags & 2); // FORWARD | BACKWARD
    const bool hurt = m_fStimuli[5] < .5f, crouched = m_nCurMetacmd == 11;
    const float near = m_fSeeingDist * (far ? .75f : .33f);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            return MC_BestWeapon();
        case 3:
            // Prophet's close attack omits the height clause that Fanatic and both far attacks have.
            if (distance <= near || m_nCurMetacmd == 7 || crouched || ((far || close_height_fires) && high))
            {
                const bool crouch =
                    far ? (high && !above) || crouched || hurt : lateral || hurt || crouched || (high && !above);
                return crouch ? MC_Fire_Crouch() : MC_Fire_Stand();
            }
            ++Metacmd;
            return true;
        case 4:
            if (distance > near && (far || distance < m_fSeeingDist * .5f))
            {
                if (!MC_FaceTarget())
                    return false;
                --Metacmd;
                bool ledge;
                if (!IsLedge(m_MoveObj.GetForwardVector(), ledge))
                    return StateFailure(locomotion_error.c_str());
                if (!ledge && !high && !crouched)
                {
                    if (m_nInjuredLeg)
                        return MC_Walk();
                    return !far && (lateral || hurt) ? MC_Fire_Crawl() : MC_Fire_Run();
                }
                return MC_Fire_Crouch();
            }
            ++Metacmd;
            return true;
        case 5:
            return ComputeState();
        default:
            return true; // Original switch has no other command action.
    }
}

// Unlike CultistAI, both classes roll forward or backward when no side is chosen.
bool AI_Mgr::RollDodge()
{
    ++dodge_updates;
    switch (Metacmd)
    {
        case 1:
            m_nDodgeFlags = CalculateDodge(m_vTrackObjPos);
            if (m_nDodgeFlags < 0)
                return StateFailure(locomotion_error.c_str());
            ++Metacmd;
            return true;
        case 2:
            if (m_nDodgeFlags & 0x10) // ROLL: the dodge ray was clear.
            {
                if (m_nDodgeFlags & 4)
                    return MC_Roll_Right();
                if (m_nDodgeFlags & 8)
                    return MC_Roll_Left();
                if (m_nDodgeFlags & 1)
                    return MC_Roll_Forward();
                return MC_Roll_Backward();
            }
            ++Metacmd;
            return true;
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
