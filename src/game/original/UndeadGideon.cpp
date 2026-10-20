// Selected original UndeadGideon.cpp: lifecycle, decisions, attacks, jump/dodge and head fire.
#include "UndeadGideon.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
bool UndeadGideon::PublishHealth(float fraction)
{
    ++health_requests;
    health_error.clear();
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) || !std::isfinite(fraction))
    {
        health_result = DE_INVALIDPARAMS;
        health_error = "UndeadGideon health owner/ratio unavailable";
        return StateFailure(health_error.c_str());
    }
    boss_health_fraction = fraction;
    health_result = s->SendBossHealth({fraction});
    if (health_result == DE_OK)
        ++health_deliveries;
    else if (health_result == DE_NOTFOUND)
        ++health_no_client;
    else
    {
        health_error = "UndeadGideon boss-health delivery failed";
        return StateFailure(health_error.c_str());
    }
    return true;
}
DDWORD UndeadGideon::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_UPDATE && RuntimeReady() && m_bCreateHealth)
    {
        // Unlike Gideon, InitialUpdate does not capture pre-base health.
        if (!PublishHealth(m_fFullHealth == 0 ? 1 : m_damage.m_fHitPoints / m_fFullHealth))
        {
            RuntimeFailure(health_error);
            return 0;
        }
        if (m_fFullHealth == 0)
        {
            m_fFullHealth = m_damage.m_fHitPoints;
            m_bCreateHealth = false;
        }
    }
    return AI_Mgr::EngineMessageFn(message, data, reason);
}
bool UndeadGideon::OnAcceptedDamage()
{
    // Same consumed-reader adaptation as Gideon: apply one typed damage value,
    // publish health, then the existing base AI reaction; never duplicate damage.
    if (!std::isfinite(m_fFullHealth) || m_fFullHealth == 0)
    {
        health_error = "UndeadGideon damage health denominator is zero";
        return StateFailure(health_error.c_str());
    }
    return PublishHealth(m_damage.m_fHitPoints / m_fFullHealth);
}
bool UndeadGideon::Ready()
{
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vTargetPos) || !physics_vector(m_vDims) || Metacmd <= std::numeric_limits<int>::min() + 2 ||
        Metacmd >= std::numeric_limits<int>::max() - 2)
        return StateFailure("UndeadGideon owner, target cache or command unavailable");
    attack_height = std::abs(m_vTargetPos.y - m_MoveObj.GetPos().y);
    return true;
}
bool UndeadGideon::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0 || stim > 4 || !Ready())
        return StateFailure("UndeadGideon stimulus unavailable");
    if (!stim)
        return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual || m_nState == StateSearchSmell
                               ? StateIdle
                               : StateSearchSmell);
    // Original tests the argument, not the result of ComputeStimuli.
    if (value == 3)
        return SetNewState(StateSearchSmell);
    return SetNewState(m_fStimuli[0] <= .85f || attack_height > m_vDims.y ? StateAttackFar : StateAttackClose);
}
bool UndeadGideon::SelectAttack(DBYTE weapon, unsigned column)
{
    selected_weapon = weapon;
    fire_column = column;
    const auto result = m_InventoryMgr.ChangeWeapon(weapon);
    if (result != InventoryResult::Ok && result != InventoryResult::Current)
        return StateFailure("UndeadGideon selected weapon unavailable");
    // Source ignores bRet; a valid current clip retained by the engine remains
    // valid, while unavailable services must still be reported.
    if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[column]))
        return false;
    GetServerDE()->SetModelLooping(m_hObject, DFALSE);
    m_bAnimating = true;
    m_nCurMetacmd = 7;
    return true;
}
bool UndeadGideon::MC_Fire_Stand()
{
    state_error.clear();
    if (!Ready() || !AnimSound())
        return StateFailure("UndeadGideon fire animation unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        auto chance = [&](int percent, bool& yes)
        {
            int draw;
            if (!GetServerDE()->RandomInt(0, 99, draw))
                return false;
            yes = draw < percent;
            return true;
        };
        bool chosen;
        if (m_nState == StateAttackFar)
        {
            if (m_vTargetPos.y > m_MoveObj.GetPos().y && attack_height > m_vDims.y)
                return SelectAttack(WEAP_GIDEON_VOMIT, TYPE_MELEE);
            if (!chance(70, chosen))
                return StateFailure("UndeadGideon weapon RNG failed");
            if (!chosen)
                return SelectAttack(WEAP_GIDEON_VOMIT, TYPE_RIFLE);
            if (!chance(50, chosen))
                return StateFailure("UndeadGideon magic RNG failed");
            return SelectAttack(chosen ? WEAP_SKULL : WEAP_DRUDGE_LIGHTNING, TYPE_MAGIC);
        }
        auto* player = dynamic_cast<CPlayerObj*>(GetServerDE()->HandleToObject(GetTarget()));
        if (player && player->m_bImprisoned)
        {
            if (!chance(40, chosen))
                return StateFailure("UndeadGideon imprisoned-target RNG failed");
            return SelectAttack(chosen ? WEAP_GIDEON_VOMIT : WEAP_BEHEMOTH_SHOCKWAVE, chosen ? TYPE_RIFLE : TYPE_MAGIC);
        }
        return SelectAttack(WEAP_GIDEON_SPEAR, TYPE_MELEE);
    }
    if (GetServerDE()->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        const auto result = m_InventoryMgr.ChangeWeapon(WEAP_GIDEON_SPEAR);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("UndeadGideon spear restoration failed");
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool UndeadGideon::AI_STATE_AttackClose()
{
    if (!Ready())
        return false;
    ++attack_updates;
    if (!GetTarget() && m_nCurMetacmd != 7)
        return attack_height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    attack_distance = m_fStimuli[0] =
        std::hypot(m_MoveObj.GetPos().x - m_vTargetPos.x, m_MoveObj.GetPos().y - m_vTargetPos.y,
                   m_MoveObj.GetPos().z - m_vTargetPos.z);
    if (!MC_FaceTarget())
        return false;
    --Metacmd;
    const bool high = m_vTargetPos.y > m_MoveObj.GetPos().y && attack_height > m_vDims.y;
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            if (attack_distance <= 405 || m_nCurMetacmd == 7 || high)
                return MC_Fire_Stand();
            return attack_distance >= m_fSeeingDist * .33f ? ComputeState() : MC_Walk();
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
bool UndeadGideon::AI_STATE_AttackFar()
{
    if (!Ready())
        return false;
    ++attack_updates;
    if (!GetTarget() && m_nCurMetacmd != 7 && m_nCurMetacmd != 3)
        return attack_height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    attack_distance = m_fStimuli[0] =
        std::hypot(m_MoveObj.GetPos().x - m_vTargetPos.x, m_MoveObj.GetPos().y - m_vTargetPos.y,
                   m_MoveObj.GetPos().z - m_vTargetPos.z);
    if (!MC_FaceTarget())
        return false;
    --Metacmd;
    const bool high = m_vTargetPos.y > m_MoveObj.GetPos().y && attack_height > m_vDims.y;
    switch (Metacmd)
    {
        case 1:
            if (attack_distance <= m_fSeeingDist * .75f || m_nCurMetacmd == 7 || high)
                return MC_Fire_Stand();
            return attack_distance < m_fSeeingDist * .20f ? ComputeState() : MC_Walk();
        case 2:
        {
            if ((attack_distance < m_fSeeingDist * .20f && m_nCurMetacmd != 3) || high)
                return ComputeState();
            bool ledge = false;
            if (attack_distance >= m_fSeeingDist * .30f && m_nCurMetacmd != 3 &&
                !IsLedge(m_MoveObj.GetForwardVector(), ledge))
                return StateFailure(locomotion_error.c_str());
            return attack_distance < m_fSeeingDist * .30f || m_nCurMetacmd == 3 || ledge ? MC_Jump() : MC_Walk();
        }
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
bool UndeadGideon::MC_Jump()
{
    if (!Ready() || !AnimSound())
        return StateFailure("UndeadGideon jump unavailable");
    auto* s = GetServerDE();
    const auto forward = m_MoveObj.GetForwardVector();
    DVector velocity;
    if (!m_bAnimating || m_nCurMetacmd != 3)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_JUMP[TYPE_MELEE]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        velocity = {forward.x * m_fRunSpeed * 3, m_MoveObj.GetUpVector().y * m_fJumpSpeed, forward.z * m_fRunSpeed * 3};
        if (!Move(velocity, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        m_bAnimating = true;
        m_nCurMetacmd = 3;
        ++jumps;
        return true;
    }
    if (s->GetVelocity(m_hObject, &velocity) != DE_OK)
        return StateFailure("UndeadGideon jump velocity unavailable");
    velocity.x = forward.x * m_fRunSpeed * 3;
    velocity.z = forward.z * m_fRunSpeed * 3;
    if (!Move(velocity, 1e-11f))
        return StateFailure(script_motion_error.c_str());
    MovementState support;
    if (!s->GetMovementState(m_hObject, support))
        return StateFailure("UndeadGideon jump support unavailable");
    if (support.standing)
    {
        if (!Move({}, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        // Source tests its OWN class (always CBaseCharacter), not the support.
        // Preserve the 999 sentinel and keep m_bAnimating/Metacmd unchanged.
        m_nCurMetacmd = 999;
    }
    return true;
}
bool UndeadGideon::MC_Dodge_Left()
{
    return Dodge(false);
}
bool UndeadGideon::MC_Dodge_Right()
{
    return Dodge(true);
}
bool UndeadGideon::Dodge(bool right)
{
    if (!Ready() || !AnimSound())
        return StateFailure("UndeadGideon dodge unavailable");
    auto* s = GetServerDE();
    const int command = right ? 22 : 21;
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_JUMP[TYPE_MELEE]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        auto side = vector_scale(m_MoveObj.GetRightVector(), right ? 1.f : -1.f);
        DVector velocity{side.x * m_fRunSpeed, m_MoveObj.GetUpVector().y * m_fJumpSpeed, side.z * m_fRunSpeed};
        if (!Move(velocity, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        m_bAnimating = true;
        m_nCurMetacmd = command;
        ++dodges;
        return true;
    }
    MovementState support;
    if (!s->GetMovementState(m_hObject, support))
        return StateFailure("UndeadGideon dodge support unavailable");
    if (support.standing && !support.ground_object && support.velocity.y <= 0)
    {
        if (!Move({}, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool UndeadGideon::AI_STATE_Dodge()
{
    if (!Ready())
        return false;
    switch (Metacmd)
    {
        case 1:
            m_nDodgeFlags = CalculateDodge(m_vTrackObjPos);
            if (m_nDodgeFlags < 0)
                return false;
            ++Metacmd;
            return true;
        case 2:
            if (m_nDodgeFlags & 4)
                return MC_Dodge_Right();
            if (m_nDodgeFlags & 8)
                return MC_Dodge_Left();
            ++Metacmd;
            return true;
        case 3:
            return SetNewState(StateIdle);
        default:
            return true;
    }
}
bool UndeadGideon::Fire(bool alternate)
{
    ++fire_calls;
    fire_error.clear();
    fire_result = FireResult::Invalid;
    clear_shot = {};
    fire_origin = fire_direction = {};
    fire_gun_node = false;
    auto fail = [&](FireResult result, const char* why)
    {
        fire_result = result;
        fire_error = why;
        ++fire_failures;
        return false;
    };
    auto* s = GetServerDE();
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (!s || !s->IsObjectAlive(m_hObject) || !m_MoveObj.sampled || !weapon)
        return fail(FireResult::Unavailable, "UndeadGideon fire owner/weapon/cache unavailable");
    DRotation rotation;
    if (!s->GetModelNodeTransform(m_hObject, "head", &fire_origin, &rotation))
        return fail(FireResult::Unavailable, "UndeadGideon head node unavailable");
    fire_gun_node = true;
    fire_direction = vector_subtract(m_vTargetPos, fire_origin);
    float length = std::hypot(fire_direction.x, fire_direction.y, fire_direction.z);
    if (!physics_vector(fire_direction) || length < 1e-6f)
        return fail(FireResult::Invalid, "UndeadGideon target aim degenerate");
    fire_direction = vector_scale(fire_direction, 1 / length);
    fire_direction.x = m_MoveObj.GetForwardVector().x;
    fire_direction.z = m_MoveObj.GetForwardVector().z;
    // AlignRotation normalizes the source's mixed forward-XZ / target-Y vector.
    length = std::hypot(fire_direction.x, fire_direction.y, fire_direction.z);
    if (!std::isfinite(length) || length < 1e-6f)
        return fail(FireResult::Invalid, "UndeadGideon mixed aim degenerate");
    fire_direction = vector_scale(fire_direction, 1 / length);
    int seed;
    if (!s->RandomInt(0, 255, seed))
        return fail(FireResult::Unavailable, "UndeadGideon shot RNG failed");
    WeaponFire request;
    request.position = fire_origin;
    request.direction = fire_direction;
    request.weapon = weapon->data.m_nType;
    request.seed = static_cast<DBYTE>(seed);
    request.alternate = alternate;
    fire_result = m_InventoryMgr.FireCurrentWeapon(request);
    if (fire_result == FireResult::Fired)
        ++fire_shots;
    else if (fire_result == FireResult::Dry)
        ++fire_dry;
    else
        return fail(fire_result, "UndeadGideon weapon service failed");
    return true;
}
