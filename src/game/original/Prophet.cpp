// Original ObjectDLL/prophet.cpp (Monolith, 1997). Construction, properties,
// model and table initialization live in AIRuntime.cpp; attacks/dodge in
// CabalAttack.cpp. See fanatic_prophet_contracts.md.
#include "Prophet.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;

DDWORD Prophet::EngineMessageFn(DDWORD message, void* data, DFLOAT value)
{
    const auto result = AI_Mgr::EngineMessageFn(message, data, value);
    // Source InitialUpdate: base initialization, then the melee weapon.
    if (message == MID_INITIALUPDATE && npc_type == NPCType::Prophet && RuntimeReady())
    {
        const auto obtained = m_InventoryMgr.ObtainWeapon(WEAP_MELEE);
        if (obtained != InventoryResult::Ok && obtained != InventoryResult::AlreadyHave)
        {
            RuntimeFailure("Prophet melee weapon acquisition failed");
            return 0;
        }
        return 0; // The source returns 0 here.
    }
    return result;
}

bool Prophet::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    DVector position;
    auto* server = GetServerDE();
    if (stim < 0 || stim > 4 || !server || !server->IsObjectAlive(m_hObject) ||
        server->GetObjectPos(m_hObject, &position) != DE_OK || !std::isfinite(m_fStimuli[0]) ||
        !std::isfinite(m_fStimuli[5]) || !std::isfinite(m_vDims.y) || m_vDims.y <= 0)
        return StateFailure("Prophet state stimulus or body unavailable");
    const double height = std::abs(double(m_vTargetPos.y) - position.y);
    if (!std::isfinite(height))
        return StateFailure("Prophet target height invalid");
    if (!stim)
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
                [[fallthrough]]; // Source continues with the changed last state.
            default:
                if (m_nLastState == StateGuard)
                    return SetNewState(StateGuard);
                // Source quirk: only a badly hurt Prophet searches (Fanatic requires >= 0.5).
                if (height <= m_vDims.y && m_fStimuli[5] < .30f)
                    return SetNewState(StateSearchVisual);
                return SetNewState(StateIdle);
        }
    }
    if (m_fStimuli[5] < .15f)
        return SetNewState(StateFindHealth);
    if (m_fStimuli[5] < .5f)
        return SetNewState(StateAttackFar);
    if (m_nState == StateEscapeRun)
        return SetNewState(StateFindAmmo);
    if (m_fStimuli[0] <= .5f || height > m_vDims.y)
        return SetNewState(StateAttackFar);
    return SetNewState(StateAttackClose);
}

bool Prophet::AI_STATE_Escape_RunAway()
{
    ++escape_updates;
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vTargetPos) || !physics_vector(m_vDims) || Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Prophet escape requires the movement cache and bounded state");
    const auto delta = vector_subtract(m_vTargetPos, m_MoveObj.GetPos());
    const float height = std::abs(delta.y);
    // Sanity check, except while a melee swing is still playing.
    if (!GetTarget() && m_nCurMetacmd != 7)
        return height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    switch (Metacmd)
    {
        case 1:
        {
            if (!MC_FaceTarget())
                return false;
            const auto changed = m_InventoryMgr.ChangeWeapon(WEAP_MELEE); // Result ignored by the source.
            if (changed == InventoryResult::Invalid)
                return StateFailure("Prophet melee selection failed");
            return true;
        }
        case 2:
        {
            m_fStimuli[0] = std::hypot(delta.x, delta.y, delta.z);
            if (m_fStimuli[0] <= 75)
                return MC_Fire_Stand();
            if (!MC_FaceTarget())
                return false;
            --Metacmd;
            bool ledge;
            if (!IsLedge(m_MoveObj.GetForwardVector(), ledge))
                return StateFailure(locomotion_error.c_str());
            return !ledge && height <= m_vDims.y ? MC_Run() : MC_Idle();
        }
        case 3:
            return ComputeState();
        default:
            return true;
    }
}

bool Prophet::MC_Fire_Stand()
{
    auto* server = GetServerDE();
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (!weapon)
        return SetNewState(StateIdle);
    if (weapon->data.m_nType != WEAP_MELEE)
        return AI_Mgr::MC_Fire_Stand();
    // Melee swing: a random FIRE_STAND clip 4..6; damage comes from its model keys.
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        if (!AnimSound() || !server->RandomInt(4, 6, melee_roll))
            return StateFailure("Prophet melee animation table or RNG unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[melee_roll]))
            return false;
        server->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
        ++melee_swings;
        server->SetNextUpdate(m_hObject, .1f);
        return true;
    }
    if (Metacmd == std::numeric_limits<int>::max())
        return StateFailure("AI Metacmd overflow");
    if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
        ++animation_finishes;
    }
    return true;
}

bool Prophet::Fire(bool)
{
    auto* server = GetServerDE();
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (!weapon)
    {
        SetNewState(StateIdle);
        return false;
    }
    // Alternate fire when both bodies are (nearly) stationary, except for weapons
    // whose alternate mode is not a stationary attack. A missing target velocity
    // keeps primary fire (the source reads an uninitialized vector).
    DVector target_velocity, velocity;
    const bool stationary = GetTarget() && server->GetVelocity(GetTarget(), &target_velocity) == DE_OK &&
                            server->GetVelocity(m_hObject, &velocity) == DE_OK &&
                            std::hypot(target_velocity.x, target_velocity.y, target_velocity.z) <= 5 &&
                            std::hypot(velocity.x, velocity.y, velocity.z) <= 5;
    const auto type = weapon->data.m_nType;
    bool alternate = false;
    if (stationary && type != WEAP_ASSAULTRIFLE && type != 7 /* NAPALM */ && type != 5 /* SNIPER */ &&
        m_InventoryMgr.GetAmmoCount(weapon->data.m_nAmmoType) >= weapon->data.m_nAltAmmoUse)
        alternate = true;
    alternate_choices += alternate;
    return AI_Mgr::Fire(alternate);
}
