// Original ObjectDLL/GabriellaAI.cpp, GabriellaREV.cpp, OpheliaAI.cpp and OpheliaREV.cpp.
#include "ChosenAI.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
bool ChosenAI::ComputeState(int value)
{
    const int stimulus = value == -1 ? ComputeStimuli() : value;
    if (stimulus < 0 || !physics_vector(m_vTargetPos) || !m_MoveObj.sampled)
        return StateFailure("Chosen decision stimuli/pose unavailable");
    const float height = std::abs(m_vTargetPos.y - m_MoveObj.GetPos().y);
    if (!stimulus)
    {
        switch (m_nState)
        {
            case StateIdle:
            case StateSearchVisual:
            case StateSearchSmell:
                return SetNewState(StateIdle);
            case StateFindAmmo:
            case StateFindHealth:
                return SetNewState(m_nLastState);
            case StateEscapeRun:
                if (!SetNewState(StateIdle))
                    return false;
                [[fallthrough]]; // Original missing break: retain both state transitions.
            default:
                return SetNewState(StateSearchVisual);
        }
    }
    if (m_fStimuli[5] < .15f)
        return SetNewState(StateFindHealth);
    if (m_fStimuli[5] < .5f || m_fStimuli[0] <= .5f || height > m_vDims.y)
        return SetNewState(StateAttackFar);
    return SetNewState(StateAttackClose);
}
bool ChosenAI::Fire(bool)
{
    auto* server = GetServerDE();
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (!weapon)
        return StateFailure("Chosen fire weapon unavailable");
    DVector target_velocity{}, velocity{};
    const bool stationary = GetTarget() && server->GetVelocity(GetTarget(), &target_velocity) == DE_OK &&
                            server->GetVelocity(m_hObject, &velocity) == DE_OK &&
                            std::hypot(target_velocity.x, target_velocity.y, target_velocity.z) <= 5 &&
                            std::hypot(velocity.x, velocity.y, velocity.z) <= 5;
    const auto type = weapon->data.m_nType;
    const bool alternate = stationary && type != WEAP_ASSAULTRIFLE && type != WEAP_NAPALMCANNON &&
                           type != WEAP_SNIPERRIFLE &&
                           m_InventoryMgr.GetAmmoCount(weapon->data.m_nAmmoType) >= weapon->data.m_nAltAmmoUse;
    alternate_choices += alternate;
    return AI_Mgr::Fire(alternate);
}
bool ChosenAI::MC_BestWeapon()
{
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Chosen Metacmd overflow");
    auto* server = GetServerDE();
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (Ophelia() && Revenant() && !weapon)
        return SetNewState(StateIdle);
    const auto animate = [&](CWeapon* w)
    {
        if (!w || !AnimSound() || w->data.m_nFireType >= std::size(AnimSound()->m_nAnim_SWITCH_WEAPON))
            return StateFailure("Chosen switch animation column unavailable");
        return SetAnimation(AnimSound()->m_nAnim_SWITCH_WEAPON[w->data.m_nFireType]);
    };
    if (!m_bAnimating)
    {
        if (!weapon)
            return true; // Other original variants make no progress with an empty inventory.
        if (m_InventoryMgr.GetAmmoCount(weapon->data.m_nAmmoType) <= 0)
        {
            if (!animate(weapon))
                return false;
            server->SetModelLooping(m_hObject, DFALSE);
            m_bAnimating = true;
        }
        else
        {
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    else if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        if (!weapon)
            return StateFailure("Chosen switching weapon removed");
        if (weapon->data.m_nType == (Ophelia() ? WEAP_TESLACANNON : WEAP_MINIGUN))
        {
            const auto result = m_InventoryMgr.ChangeWeapon(Ophelia() ? WEAP_SNIPERRIFLE : WEAP_NAPALMCANNON);
            if (result != InventoryResult::Ok && result != InventoryResult::Current)
                return StateFailure("Chosen alternate weapon unavailable");
            weapon = m_InventoryMgr.CurrentWeapon();
            if (!weapon || !weapon->ShowHandModel(DFALSE) || !animate(weapon))
                return StateFailure("Chosen switched weapon presentation unavailable");
            ++weapon_switches; // Original remains animating through the second switch animation.
        }
        else if (m_InventoryMgr.GetAmmoCount(weapon->data.m_nAmmoType) <= 0)
            return SetNewState(StateFindAmmo);
        else
        {
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
bool ChosenAI::Attack(bool far)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.sampled || !physics_vector(m_vTargetPos) ||
        !physics_vector(m_vDims) || m_vDims.y <= 0 || Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Chosen attack cache/phase unavailable");
    ++crouch_attack_updates;
    const auto delta = vector_subtract(m_vTargetPos, m_MoveObj.GetPos());
    const float height = crouch_attack_height = std::abs(delta.y);
    if (!GetTarget() && (Revenant() || m_nCurMetacmd != 7))
        return height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    if (GetTarget() && !server->IsObjectAlive(GetTarget()))
        return StateFailure("Chosen attack target removed");
    const float distance = crouch_attack_distance = m_fStimuli[0] = std::hypot(delta.x, delta.y, delta.z);
    const bool high = height > m_vDims.y, above = delta.y > 0;
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (Ophelia() && !weapon)
        return SetNewState(StateIdle);
    const bool sniper = Ophelia() && weapon->data.m_nType == WEAP_SNIPERRIFLE;
    const bool lateral = (m_nDodgeFlags & 1) || (m_nDodgeFlags & 2), hurt = m_fStimuli[5] < .5f,
               crouched = m_nCurMetacmd == 11;
    const float near = m_fSeeingDist * (far ? .75f : .33f);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            return MC_BestWeapon();
        case 3:
            if (distance <= near || m_nCurMetacmd == 7 || crouched || (far && high))
            {
                const bool crouch = sniper || (high && !above) || crouched || hurt || (!far && lateral);
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
                if (!ledge && !high && !crouched && !sniper)
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
            return true;
    }
}
bool ChosenAI::AI_STATE_Escape_RunAway()
{
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Chosen Metacmd overflow");
    ++escape_updates;
    if (Metacmd > 1)
    {
        const int stimulus = ComputeStimuli();
        if (stimulus < 0)
            return StateFailure(perception_error.c_str());
        if (stimulus > 0)
            return ComputeState(stimulus);
    }
    if (!m_MoveObj.sampled || !physics_vector(m_vDims))
        return StateFailure("Chosen escape movement unavailable");
    const auto position = m_MoveObj.GetPos();
    const float dim = std::hypot(m_vDims.x, m_vDims.z) + .1f;
    switch (Metacmd)
    {
        case 1:
        {
            const auto hint = FindObjectInRadius(AIFindList::Exits, m_fSeeingDist, FindVisible | FindAvoidTarget);
            if (!find.valid)
                return StateFailure(find.error.c_str());
            if (!SetTrackedObject(hint))
                return StateFailure(target_error.c_str());
            if (!hint)
                return SetNewState(StateAttackFar);
            if (!RefreshTrackedPosition() || !MC_FacePos(m_vTrackObjPos))
                return StateFailure(target_error.c_str());
            if (!m_MoveObj.CalculatePath(m_vTrackObjPos))
                return m_MoveObj.outcome == CMovement::Outcome::Blocked ? SetNewState(StateIdle)
                                                                        : StateFailure(m_MoveObj.error.c_str());
            return true;
        }
        case 2:
        {
            DVector point;
            return m_MoveObj.GetNextPathPoint(point) ? MC_FacePos(point) : SetNewState(StateEscapeRun);
        }
        case 3:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return StateFailure("Chosen escape phase-3 path missing");
            point.y = position.y;
            const auto delta = vector_subtract(point, position);
            if (std::hypot(delta.x, delta.y, delta.z) <= dim / 2)
            {
                if (!m_MoveObj.MoveToNextPathPoint())
                    return MC_FacePos(m_vTargetPos);
                Metacmd = 2;
                return true;
            }
            return MC_Run();
        }
        case 4:
            return ComputeState();
        default:
            return true;
    }
}
bool ChosenAI::AI_STATE_Escape_Hide()
{
    ++escape_updates;
    if (Metacmd != 1)
        return true;
    const auto hint = FindObjectInRadius(AIFindList::Exits, m_fSeeingDist);
    if (!find.valid)
        return StateFailure(find.error.c_str());
    if (!SetTrackedObject(hint))
        return StateFailure(target_error.c_str());
    if (!hint)
        return SetNewState(StateAttackClose);
    if (GetServerDE()->GetObjectPos(hint, &m_vDestPos) != DE_OK)
        return StateFailure("Chosen hide hint pose unavailable");
    return SetNewState(StateRunTo);
}
bool ChosenAI::Dodge(bool right)
{
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Chosen Metacmd overflow");
    auto* s = GetServerDE();
    const int command = right ? 22 : 21;
    if (!AnimSound() || !m_MoveObj.sampled)
        return StateFailure("Chosen sidestep cache unavailable");
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        m_fTimeStart = s->GetTime();
        if (!SetAnimation(right ? AnimSound()->m_nAnim_DODGE_RIGHT : AnimSound()->m_nAnim_DODGE_LEFT))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        if (!Move(vector_scale(m_MoveObj.GetRightVector(), right ? 1.f : -1.f), m_fRollSpeed))
            return false;
        m_bAnimating = true;
        m_nCurMetacmd = command;
        ++sidesteps;
    }
    else if (s->GetTime() - m_fTimeStart >= 1)
    {
        m_bAnimating = false;
        ++Metacmd;
    } // Source does not stop velocity or wait for clip completion.
    return true;
}
bool ChosenAI::AI_STATE_Dodge()
{
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Chosen Metacmd overflow");
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
            if (m_nDodgeFlags & 0x10)
            {
                if (m_nDodgeFlags & 4)
                    return MC_Roll_Right();
                if (m_nDodgeFlags & 8)
                    return MC_Roll_Left();
                return m_nDodgeFlags & 1 ? MC_Roll_Forward() : MC_Roll_Backward();
            }
            if (m_nDodgeFlags & 4)
                return Dodge(true);
            if (m_nDodgeFlags & 8)
                return Dodge(false);
            ++Metacmd;
            return true;
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
