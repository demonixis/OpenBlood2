// Original ObjectDLL/DrudgeLord.cpp (Monolith), selected decisions and attacks.
#include "DrudgeLord.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
bool DrudgeLord::ComputeState(int value)
{
    const int stimulus = value == -1 ? ComputeStimuli() : value;
    if (stimulus < 0)
        return StateFailure("DrudgeLord stimulus unavailable");
    if (!stimulus)
        return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual || m_nState == StateSearchSmell
                               ? StateIdle
                               : StateSearchVisual);
    return SetNewState(m_fStimuli[5] < .25f || m_fStimuli[0] <= .70f ? StateAttackFar : StateAttackClose);
}
bool DrudgeLord::MC_Fire_Stand()
{
    auto* s = GetServerDE();
    if (!AnimSound() || !m_MoveObj.initialized || !s->IsObjectAlive(m_hObject))
        return StateFailure("DrudgeLord fire table/cache unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        const bool far = m_nState == StateAttackFar || std::abs(m_vTargetPos.y - m_MoveObj.GetPos().y) > m_vDims.y;
        m_InventoryMgr.ChangeWeapon(far ? WEAP_DRUDGE_FIREBALL : WEAP_SOUL_HOOK);
        int clip = 0;
        if (!far && !s->RandomInt(4, 6, clip))
            return StateFailure("DrudgeLord hook animation RNG failed");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[clip]))
            return animation_blocked ? SetNewState(StateCrawlUnder) : false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
        ++(far ? fireball_clips : hook_clips);
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_InventoryMgr.ChangeWeapon(WEAP_SOUL_HOOK);
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool DrudgeLord::Attack(bool far)
{
    if (!m_MoveObj.initialized || !m_MoveObj.sampled || !physics_vector(m_vTargetPos))
        return StateFailure("DrudgeLord movement cache unavailable");
    ++attack_updates;
    const auto delta = vector_subtract(m_vTargetPos, m_MoveObj.GetPos());
    const auto height = std::abs(delta.y);
    if (!GetTarget())
        return height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    if (!GetServerDE()->IsObjectAlive(GetTarget()))
        return StateFailure("DrudgeLord target pending removal");
    const float distance = m_fStimuli[0] = std::sqrt(vector_dot(delta, delta));
    auto face = [&]
    {
        if (!MC_FaceTarget())
            return false;
        --Metacmd;
        return true;
    };
    auto walk_or_fire = [&]
    {
        bool ledge;
        if (!IsLedge(m_MoveObj.GetForwardVector(), ledge))
            return StateFailure(locomotion_error.c_str());
        return !ledge && height <= m_vDims.y && m_nCurMetacmd != 7 ? MC_Walk() : MC_Fire_Stand();
    };
    if (!far)
    {
        switch (Metacmd)
        {
            case 1:
                return MC_FaceTarget();
            case 2:
                if (distance <= 100 || m_nCurMetacmd == 7)
                    return MC_Fire_Stand();
                if (!face())
                    return false;
                return distance >= m_fSeeingDist * .30f ? ComputeState() : walk_or_fire();
            case 3:
                return ComputeState();
            default:
                return true;
        }
    }
    switch (Metacmd)
    {
        case 1:
            if (!face())
                return false;
            if (distance <= m_fSeeingDist * .75f || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            return distance < m_fSeeingDist * .30f ? ComputeState() : walk_or_fire();
        case 2:
            return MC_Taunt_Bold();
        case 3:
            if (!face())
                return false;
            return distance < m_fSeeingDist * .30f ? ComputeState() : walk_or_fire();
        case 4:
            return ComputeState();
        default:
            return true;
    }
}
