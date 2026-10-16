// Original CultistAI.cpp:331-454; checked existing services, no production AI_Update.
#include "CultistAI.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
bool CultistAI::AI_STATE_AttackClose()
{
    return Attack(false);
}
bool CultistAI::AI_STATE_AttackFar()
{
    return Attack(true);
}
bool CultistAI::Attack(bool far)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !OpenBlood2::physics_vector(m_vTargetPos) || !OpenBlood2::physics_vector(m_vDims) || m_vDims.y <= 0 ||
        !std::isfinite(m_fSeeingDist) || m_fSeeingDist < 0 || m_fSeeingDist > 1e7f ||
        Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Cultist attack requires live owner, cache and bounded state");
    ++attack_updates;
    const auto delta = OpenBlood2::vector_subtract(m_vTargetPos, m_MoveObj.GetPos());
    attack_height = std::abs(delta.y);
    attack_threshold = m_fSeeingDist * (far ? .75f : .25f);
    if (!GetTarget())
        return attack_height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    if (!server->IsObjectAlive(GetTarget()))
        return StateFailure("Cultist target pending removal");
    attack_distance = m_fStimuli[0] = std::hypot(delta.x, delta.y, delta.z);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            return MC_BestWeapon();
        case 3:
            if (attack_distance <= attack_threshold || m_nCurMetacmd == 7 || attack_height > m_vDims.y)
                return MC_Fire_Stand();
            ++Metacmd;
            return true;
        case 4:
            if (attack_distance > attack_threshold && (far || attack_distance < m_fSeeingDist * .5f))
            {
                if (!MC_FaceTarget())
                    return false;
                --Metacmd;
                bool ledge_found;
                if (!IsLedge(m_MoveObj.GetForwardVector(), ledge_found))
                    return StateFailure(locomotion_error.c_str());
                if (!ledge_found && attack_height <= m_vDims.y && m_nCurMetacmd != 7)
                {
                    if (m_nInjuredLeg)
                        return MC_Walk();
                    return far ? MC_Fire_Run() : MC_Run();
                }
                return MC_Fire_Stand();
            }
            ++Metacmd;
            return true;
        case 5:
            return ComputeState();
        default:
            return true; // Original switch has no other command action.
    }
}
