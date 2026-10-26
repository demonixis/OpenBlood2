// Original ObjectDLL/Gremlin.cpp (Monolith, _ADD_ON). See nightmares_edition_contracts.md.
#include "GremlinAI.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
namespace
{
    constexpr int stim_smell = 3; // STIM_SMELL
} // namespace

bool GremlinAI::Ready(const char* what)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vTargetPos) || !physics_vector(m_vDims) || Metacmd > std::numeric_limits<int>::max() - 2 ||
        Metacmd < std::numeric_limits<int>::min() + 2)
        return StateFailure(what);
    return true;
}
float GremlinAI::Height() const
{
    return std::abs(m_vTargetPos.y - m_MoveObj.GetPos().y);
}
bool GremlinAI::Above() const
{
    return m_vTargetPos.y - m_MoveObj.GetPos().y > 0;
}

// Unlike ShikariAI, no height test: SIGHT <= .75 attacks from afar.
bool GremlinAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("Gremlin stimulus unavailable");
    if (!stim)
    {
        switch (m_nState)
        {
            case StateIdle:
            case StateSearchVisual:
            case StateSearchSmell:
                return SetNewState(StateIdle);
            default:
                return SetNewState(StateSearchSmell);
        }
    }
    // Source quirk (shared with Shikari): the smell test reads the argument, not the stimulus.
    if (value == stim_smell)
        return SetNewState(StateSearchSmell);
    if (m_fStimuli[0] <= .75f) // SIGHT.
        return SetNewState(StateAttackFar);
    return SetNewState(StateAttackClose);
}

bool GremlinAI::AI_STATE_AttackClose()
{
    if (!GetTarget())
        return ComputeState(); // Source sanity check, before any cache read.
    if (!Ready("Gremlin attack requires live owner, cache and bounded state"))
        return false;
    if (!GetServerDE()->IsObjectAlive(GetTarget()))
        return StateFailure("Gremlin target pending removal");
    ++attack_updates;
    attack_height = Height();
    attack_distance = m_fStimuli[0] = std::sqrt(vector_dot(vector_subtract(m_MoveObj.GetPos(), m_vTargetPos),
                                                           vector_subtract(m_MoveObj.GetPos(), m_vTargetPos)));
    // The source faces every update and cancels the command advance.
    if (!MC_FaceTarget())
        return false;
    --Metacmd;
    const bool above = Above();
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            if (attack_distance <= 75 || m_nCurMetacmd == 7 || (above && attack_height > m_vDims.y))
                return MC_Fire_Stand();
            if (attack_distance > m_fSeeingDist * .33f)
                return ComputeState();
            return MC_Run();
        case 3:
            return ComputeState();
        default:
            return true;
    }
}

bool GremlinAI::AI_STATE_AttackFar()
{
    if (!GetTarget())
        return ComputeState(); // No exception for a jump in flight (unlike Shikari).
    if (!Ready("Gremlin attack requires live owner, cache and bounded state"))
        return false;
    if (!GetServerDE()->IsObjectAlive(GetTarget()))
        return StateFailure("Gremlin target pending removal");
    ++attack_updates;
    attack_height = Height();
    attack_distance = m_fStimuli[0] = std::sqrt(vector_dot(vector_subtract(m_MoveObj.GetPos(), m_vTargetPos),
                                                           vector_subtract(m_MoveObj.GetPos(), m_vTargetPos)));
    const bool above = Above();
    if (!MC_FaceTarget())
        return false;
    --Metacmd;
    switch (Metacmd)
    {
        case 1:
            if (m_nCurMetacmd != 18) // MC_TAUNT_BOLD
            {
                // IsRandomChance(40): GetRandom(0, 99) < 40.
                if (!GetServerDE()->RandomInt(0, 99, taunt_roll))
                    return StateFailure("Gremlin taunt RNG failed");
                if (taunt_roll < 40)
                {
                    if (!MC_Taunt_Bold())
                        return false;
                    ++taunts;
                    --Metacmd;
                }
                else
                    ++Metacmd;
            }
            else if (GetServerDE()->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
                ++Metacmd;
            return true;
        case 2:
            if (attack_distance <= m_fSeeingDist * .75f || m_nCurMetacmd == 7 || (above && attack_height > m_vDims.y))
                return MC_Fire_Stand();
            if (attack_distance < m_fSeeingDist * .33f)
                return ComputeState();
            return MC_Walk();
        case 3:
        {
            if (attack_distance < m_fSeeingDist * .33f && m_nCurMetacmd != 3) // MC_JUMP
                return ComputeState();
            // Source short circuit: IsLedge only when nearer and not jumping fail.
            bool ledge = false;
            if (attack_distance >= m_fSeeingDist * .4f && m_nCurMetacmd != 3 &&
                !IsLedge(m_MoveObj.GetForwardVector(), ledge))
                return StateFailure(locomotion_error.c_str());
            if (attack_distance < m_fSeeingDist * .4f || m_nCurMetacmd == 3 || ledge)
                return Jump();
            ++Metacmd;
            return true;
        }
        default:
            return ComputeState();
    }
}

// Source MC_Jump override: m_nAnim_JUMP[4], three times the run speed forward, held every
// update until landing (the source's unused height/above locals are not reproduced).
bool GremlinAI::Jump()
{
    auto* server = GetServerDE();
    if (!AnimSound() || !std::isfinite(m_fRunSpeed) || !std::isfinite(m_fJumpSpeed))
        return StateFailure("Gremlin jump table or speeds unavailable");
    const auto forward = m_MoveObj.GetForwardVector();
    DVector velocity{};
    if (!m_bAnimating || m_nCurMetacmd != 3)
    {
        // The source ignores this SetAnimation result.
        SetAnimation(AnimSound()->m_nAnim_JUMP[4]);
        server->SetModelLooping(m_hObject, DFALSE);
        velocity.y = m_MoveObj.GetUpVector().y * m_fJumpSpeed;
        velocity.x = forward.x * m_fRunSpeed * 3;
        velocity.z = forward.z * m_fRunSpeed * 3;
        if (!Move(velocity, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        m_nCurMetacmd = 3;
        m_bAnimating = true;
        ++jumps;
        return true;
    }
    if (server->GetVelocity(m_hObject, &velocity) != DE_OK)
        return StateFailure("Gremlin jump velocity unavailable");
    velocity.x = forward.x * m_fRunSpeed * 3;
    velocity.z = forward.z * m_fRunSpeed * 3;
    if (!Move(velocity, 1e-11f))
        return StateFailure(script_motion_error.c_str());
    MovementState support;
    if (!server->GetMovementState(m_hObject, support))
        return StateFailure("Gremlin jump support query unavailable");
    if (support.standing)
    {
        if (!StopVelocity())
            return false;
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}

// Source override: rocks for AttackFar, guard or a target above the body height, else the
// claw; both play FIRE_STAND[4] (no draw). The claw is restored when the clip ends.
bool GremlinAI::MC_Fire_Stand()
{
    state_error.clear();
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !AnimSound())
        return StateFailure("Gremlin fire command requires a live owner and table");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        const bool rock = m_nState == StateAttackFar || m_nState == StateGuard || (Above() && Height() > m_vDims.y);
        // The source ignores the ChangeWeapon result.
        m_InventoryMgr.ChangeWeapon(rock ? WEAP_GREMLIN_ROCK : WEAP_SHIKARI_CLAW);
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[4]))
            return animation_blocked ? SetNewState(StateCrawlUnder) : false;
        server->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
        ++(rock ? rock_clips : claw_clips);
        return true;
    }
    if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_InventoryMgr.ChangeWeapon(WEAP_SHIKARI_CLAW);
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}

// Source MC_Dodge_Left/Right: both play m_nAnim_DODGE_RIGHT with a jump-speed hop sideways at
// run speed, ending on landing with a zero move.
bool GremlinAI::Dodge(bool right)
{
    auto* server = GetServerDE();
    if (!AnimSound())
        return StateFailure("Gremlin dodge table unavailable");
    const int command = right ? 22 : 21; // MC_DODGE_RIGHT / MC_DODGE_LEFT.
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        SetAnimation(AnimSound()->m_nAnim_DODGE_RIGHT);
        server->SetModelLooping(m_hObject, DFALSE);
        const auto side = vector_scale(m_MoveObj.GetRightVector(), right ? 1.f : -1.f);
        DVector velocity{side.x * m_fRunSpeed, m_MoveObj.GetUpVector().y * m_fJumpSpeed, side.z * m_fRunSpeed};
        if (!Move(velocity, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        m_nCurMetacmd = command;
        m_bAnimating = true;
        ++dodges;
        return true;
    }
    MovementState support;
    if (!server->GetMovementState(m_hObject, support))
        return StateFailure("Gremlin dodge support query unavailable");
    if (support.standing)
    {
        if (!Move({}, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}

bool GremlinAI::AI_STATE_Dodge()
{
    switch (Metacmd)
    {
        case 1:
            m_nDodgeFlags = CalculateDodge(m_vTrackObjPos);
            ++Metacmd;
            return true;
        case 2:
            if (m_nDodgeFlags & 4) // RIGHT
                return Dodge(true);
            if (m_nDodgeFlags & 8) // LEFT
                return Dodge(false);
            ++Metacmd;
            return true;
        case 3:
            return SetNewState(m_nLastState);
        default:
            return true;
    }
}

bool GremlinAI::AI_STATE_GuardLocation()
{
    const int stim = ComputeStimuli();
    if (stim < 0)
        return StateFailure("Gremlin stimulus unavailable");
    if (!MC_FaceTarget())
        return false;
    --Metacmd;
    switch (Metacmd)
    {
        case 1:
            if (stim != stim_smell && stim > 0)
                ++Metacmd;
            return true;
        case 2:
            return MC_Taunt_Bold();
        case 3:
            if (m_fStimuli[0] <= m_fSeeingDist || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            ++Metacmd;
            return true;
        case 4:
            if (m_fStimuli[5] <= .5f) // HEALTH
                return ComputeState();
            Metacmd = 1;
            return true;
        default:
            return true;
    }
}

// Source AI_STATE_Special1 (eating): any damager interrupts it.
bool GremlinAI::AI_STATE_Special(int index)
{
    if (index != 1)
        return AI_Mgr::AI_STATE_Special(index);
    if (m_damage.GetLastDamager())
        return ComputeState();
    GetServerDE()->SetNextUpdate(m_hObject, .1f);
    switch (Metacmd)
    {
        case 1:
            return MC_Special(0);
        case 2:
            Metacmd = 1;
            return true;
        default:
            return true;
    }
}
