// Original ObjectDLL/ShikariAI.cpp (Monolith). See shikari_contracts.md.
#include "ShikariAI.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
namespace
{
    constexpr int stim_smell = 3; // STIM_SMELL
} // namespace

bool ShikariAI::Ready(const char* what)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vTargetPos) || !physics_vector(m_vDims) ||
        Metacmd > std::numeric_limits<int>::max() - 2 || Metacmd < std::numeric_limits<int>::min() + 2)
        return StateFailure(what);
    return true;
}
float ShikariAI::Height() const
{
    return std::abs(m_vTargetPos.y - m_MoveObj.GetPos().y);
}
bool ShikariAI::Above() const
{
    return m_vTargetPos.y - m_MoveObj.GetPos().y > 0;
}

bool ShikariAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("Shikari stimulus unavailable");
    if (!physics_vector(m_vTargetPos) || !m_MoveObj.initialized)
        return StateFailure("Shikari state cache unavailable");
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
    // Source quirk: the smell test reads the argument, not the computed stimulus.
    if (value == stim_smell)
        return SetNewState(StateSearchSmell);
    if (m_fStimuli[0] <= .66f || Height() > m_vDims.y) // SIGHT.
        return SetNewState(StateAttackFar);
    return SetNewState(StateAttackClose);
}

bool ShikariAI::AI_STATE_AttackClose()
{
    if (!Ready("Shikari attack requires live owner, cache and bounded state"))
        return false;
    ++attack_updates;
    attack_height = Height();
    if (!GetTarget())
        return attack_height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    if (!GetServerDE()->IsObjectAlive(GetTarget()))
        return StateFailure("Shikari target pending removal");
    attack_distance = m_fStimuli[0] =
        std::sqrt(vector_dot(vector_subtract(m_MoveObj.GetPos(), m_vTargetPos),
                             vector_subtract(m_MoveObj.GetPos(), m_vTargetPos)));
    const bool above = Above();
    // The source faces every update and cancels the command advance.
    if (!MC_FaceTarget())
        return false;
    --Metacmd;
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

bool ShikariAI::AI_STATE_AttackFar()
{
    if (!Ready("Shikari attack requires live owner, cache and bounded state"))
        return false;
    ++attack_updates;
    attack_height = Height();
    // A pounce in flight continues without a target.
    if (!GetTarget() && m_nCurMetacmd != 3 /* MC_JUMP */)
        return attack_height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    if (GetTarget() && !GetServerDE()->IsObjectAlive(GetTarget()))
        return StateFailure("Shikari target pending removal");
    attack_distance = m_fStimuli[0] =
        std::sqrt(vector_dot(vector_subtract(m_MoveObj.GetPos(), m_vTargetPos),
                             vector_subtract(m_MoveObj.GetPos(), m_vTargetPos)));
    const bool above = Above(), high = above && attack_height > m_vDims.y;
    if (!MC_FaceTarget())
        return false;
    --Metacmd;
    switch (Metacmd)
    {
        case 1:
            if (attack_distance <= m_fSeeingDist * .75f || m_nCurMetacmd == 7 || high)
                return MC_Fire_Stand();
            if (attack_distance < m_fSeeingDist * .33f)
                return ComputeState();
            return MC_Walk();
        case 2:
        {
            // Source precedence: (near && !jumping) || high.
            if ((attack_distance < m_fSeeingDist * .33f && m_nCurMetacmd != 3) || high)
                return ComputeState();
            bool ledge = false;
            if (attack_distance >= m_fSeeingDist * .4f && m_nCurMetacmd != 3 &&
                !IsLedge(m_MoveObj.GetForwardVector(), ledge))
                return StateFailure(locomotion_error.c_str());
            if (attack_distance < m_fSeeingDist * .4f || m_nCurMetacmd == 3 || ledge)
                return Pounce();
            return MC_Roll_Forward(true);
        }
        case 3:
            return ComputeState();
        default:
            return true;
    }
}

// Source MC_Jump override: m_nAnim_JUMP[4], three times the run speed forward, held
// every update until landing.
bool ShikariAI::Pounce()
{
    auto* server = GetServerDE();
    if (!AnimSound() || !std::isfinite(m_fRunSpeed) || !std::isfinite(m_fJumpSpeed))
        return StateFailure("Shikari jump table or speeds unavailable");
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
        ++pounces;
        return true;
    }
    if (server->GetVelocity(m_hObject, &velocity) != DE_OK)
        return StateFailure("Shikari jump velocity unavailable");
    velocity.x = forward.x * m_fRunSpeed * 3;
    velocity.z = forward.z * m_fRunSpeed * 3;
    if (!Move(velocity, 1e-11f))
        return StateFailure(script_motion_error.c_str());
    MovementState support;
    if (!server->GetMovementState(m_hObject, support))
        return StateFailure("Shikari jump support query unavailable");
    if (support.standing)
    {
        if (!StopVelocity())
            return false;
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}

// Source override: spit (FIRE_STAND[0]) for AttackFar, guard or a target above the body
// height, else claw with a random FIRE_STAND[4..6]; the claw is restored afterwards.
bool ShikariAI::MC_Fire_Stand()
{
    state_error.clear();
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !AnimSound())
        return StateFailure("Shikari fire command requires a live owner and table");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        const bool spit =
            m_nState == StateAttackFar || m_nState == StateGuard || (Above() && Height() > m_vDims.y);
        // The source ignores the ChangeWeapon result.
        m_InventoryMgr.ChangeWeapon(spit ? WEAP_SHIKARI_SPIT : WEAP_SHIKARI_CLAW);
        int clip = 0;
        if (!spit && !server->RandomInt(4, 6, clip))
            return StateFailure("Shikari attack clip RNG failed");
        last_fire_clip = clip;
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[clip]))
            return animation_blocked ? SetNewState(StateCrawlUnder) : false;
        server->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
        ++(spit ? spit_clips : claw_clips);
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

// Source MC_Dodge_Left/Right: both play m_nAnim_DODGE_RIGHT (source quirk) with a
// jump-speed hop sideways at run speed, ending on landing with a zero move.
bool ShikariAI::Dodge(bool right)
{
    auto* server = GetServerDE();
    if (!AnimSound())
        return StateFailure("Shikari dodge table unavailable");
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
        return StateFailure("Shikari dodge support query unavailable");
    if (support.standing)
    {
        if (!Move({}, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}

bool ShikariAI::AI_STATE_Dodge()
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

bool ShikariAI::AI_STATE_GuardLocation()
{
    const int stim = ComputeStimuli();
    if (stim < 0)
        return StateFailure("Shikari stimulus unavailable");
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

bool ShikariAI::AI_STATE_Special(int index)
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
