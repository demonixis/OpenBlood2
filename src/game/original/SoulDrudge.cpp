// Original ObjectDLL/SoulDrudge.cpp (Monolith). See souldrudge_contracts.md.
#include "SoulDrudge.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;

bool SoulDrudge::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("SoulDrudge stimulus unavailable");
    if (!stim)
    {
        switch (m_nState)
        {
            case StateIdle:
            case StateSearchVisual:
            case StateSearchSmell:
                return SetNewState(StateIdle);
            default:
                return SetNewState(StateSearchVisual);
        }
    }
    return SetNewState(m_fStimuli[0] > .5f ? StateAttackClose : StateAttackFar); // SIGHT.
}

// AttackClose runs, AttackFar walks; otherwise the two source bodies are identical.
bool SoulDrudge::Attack(bool far)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vTargetPos) || !physics_vector(m_vDims) || Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("SoulDrudge attack requires live owner, cache and bounded state");
    ++attack_updates;
    const auto delta = vector_subtract(m_vTargetPos, m_MoveObj.GetPos());
    attack_height = std::abs(delta.y);
    // SCHLEGZ sanity check.
    if (!GetTarget())
        return attack_height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    if (!server->IsObjectAlive(GetTarget()))
        return StateFailure("SoulDrudge target pending removal");
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
        {
            attack_distance = m_fStimuli[0] = std::hypot(delta.x, delta.y, delta.z);
            if (attack_distance <= 75 || m_nCurMetacmd == 7 /* MC_FIRE_STAND */)
                return MC_Fire_Stand();
            if (!MC_FaceTarget())
                return false;
            --Metacmd;
            bool ledge;
            if (!IsLedge(m_MoveObj.GetForwardVector(), ledge))
                return StateFailure(locomotion_error.c_str());
            if (!ledge && attack_height <= m_vDims.y)
                return far ? MC_Walk() : MC_Run();
            return MC_Idle();
        }
        case 3:
            return ComputeState();
        default:
            return true;
    }
}

// Source override: a random clip from m_nAnim_FIRE_STAND[4..6], non-looping, and the
// next command once it finishes. Damage comes from its fire_key model keys; there is
// no reload-timed Fire() here.
bool SoulDrudge::MC_Fire_Stand()
{
    state_error.clear();
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !AnimSound())
        return StateFailure("SoulDrudge fire command requires a live owner and table");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        int clip = 4;
        if (!server->RandomInt(4, 6, clip))
            return StateFailure("SoulDrudge attack clip RNG failed");
        last_fire_clip = clip;
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[clip]))
        {
            // SCHLEGZ: if we can't stand up, crawl out from underneath. Only an actual
            // blocked resize selects it; missing clips/services stay failures.
            return animation_blocked ? SetNewState(StateCrawlUnder) : false;
        }
        server->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
        ++fire_stand_clips;
        return true;
    }
    if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
