// Original ObjectDLL/Naga.cpp, with checked handles and typed weapon/health messages.
#include "Naga.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
Naga::~Naga()
{
    if (!g_pServerDE)
        return;
    if (spike_attachment)
        g_pServerDE->RemoveAttachment(spike_attachment);
    if (g_pServerDE->IsObjectAlive(spike))
        g_pServerDE->RemoveObject(spike);
}
DDWORD Naga::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_UPDATE && RuntimeReady() && create_health)
    {
        const auto result = s->SendBossHealth({full_health ? m_damage.m_fHitPoints / full_health : 1});
        if (result != DE_OK && result != DE_NOTFOUND)
        {
            RuntimeFailure("Naga health delivery failed");
            return 0;
        }
        if (!full_health)
        {
            full_health = m_damage.m_fHitPoints;
            create_health = false;
        }
    }
    const auto result = AI_Mgr::EngineMessageFn(message, data, reason);
    if (message == MID_INITIALUPDATE && RuntimeReady())
    {
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        if (!s->GetModelNodeTransform(m_hObject, "l_gun", &state.m_Pos, &state.m_Rotation))
            return RuntimeFailure("Naga spike node unavailable");
        SAFE_STRCPY(state.m_Filename, "Models\\Ammo\\Nagaspike.abc");
        SAFE_STRCPY(state.m_SkinName, "Skins\\Powerups\\Nagaspike_pu.dtx");
        auto* child = s->CreateObject(s->GetClass("Model"), &state);
        if (!child)
            return RuntimeFailure("Naga held spike unavailable");
        spike = child->m_hObject;
        DVector offset{}, color;
        DRotation rotation{};
        float alpha;
        if (s->CreateAttachment(m_hObject, spike, "l_gun", &offset, &rotation, &spike_attachment) != DE_OK ||
            !s->GetObjectColor(m_hObject, color, alpha) || !s->SetObjectColor(m_hObject, color, 0))
            return RuntimeFailure("Naga spike attachment/entrance unavailable");
        // Original: the shadow and solid flags are cleared for a fresh object only; a restore keeps
        // the saved flags (the spike and the zero alpha are set on every path, as the original does).
        if (reason != INITIALUPDATE_SAVEGAME)
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~(FLAG_SHADOW | FLAG_SOLID));
    }
    return result;
}
bool Naga::OnAcceptedDamage()
{
    if (!full_health)
        return StateFailure("Naga health denominator unavailable");
    const auto result = GetServerDE()->SendBossHealth({m_damage.m_fHitPoints / full_health});
    return result == DE_OK || result == DE_NOTFOUND || StateFailure("Naga health delivery failed");
}
bool Naga::ComputeState(int value)
{
    const auto stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("Naga stimuli unavailable");
    if (!stim)
        return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual ? StateIdle : StateSearchVisual);
    if (m_fStimuli[3] > 0 && m_fStimuli[0] <= 0)
        return SetNewState(StateAttackFar);
    return SetNewState(m_fStimuli[5] < .25f || m_fStimuli[4] > .75f || m_fStimuli[0] > .85f ? StateAttackClose
                                                                                            : StateAttackFar);
}
bool Naga::SetNewState(int state)
{
    if (GetServerDE()->IsObjectAlive(spike))
        GetServerDE()->SetObjectFlags(spike, GetServerDE()->GetObjectFlags(spike) & ~FLAG_VISIBLE);
    return AI_Mgr::SetNewState(state);
}
bool Naga::MC_Fire_Stand()
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("Naga animation table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        int draw = 100;
        DBYTE weapon;
        unsigned column;
        if (m_nState != StateAttackFar || m_fStimuli[0] > 0)
            if (!s->RandomInt(0, 99, draw))
                return StateFailure("Naga attack RNG unavailable");
        if (m_nState == StateAttackFar)
        {
            if ((m_fStimuli[0] > 0 && draw < 50) || ++ceiling_whomps > 10)
            {
                if (ceiling_whomps > 10)
                    ceiling_whomps = 0;
                weapon = WEAP_NAGA_EYEBEAM;
                column = TYPE_RIFLE;
            }
            else
            {
                weapon = WEAP_NAGA_DEBRIS;
                column = TYPE_MELEE;
            }
        }
        else
        {
            weapon = draw < 25 ? WEAP_NAGA_EYEBEAM : WEAP_NAGA_SPIKE;
            column = draw < 25 ? TYPE_RIFLE : TYPE_MAGIC;
        }
        const auto result = m_InventoryMgr.ChangeWeapon(weapon);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("Naga weapon unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[column]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool Naga::AI_STATE_AttackClose()
{
    if (!GetTarget() && m_nCurMetacmd != 7)
        return SetNewState(StateSearchVisual);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            return MC_Fire_Stand();
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
bool Naga::MC_Extra(std::string_view key)
{
    if (!m_bAnimating)
        return true;
    if (!same_name(key, "take_spike") && !same_name(key, "fire_spike"))
        return AI_Mgr::MC_Extra(key);
    auto* s = GetServerDE();
    if (!s->IsObjectAlive(spike))
        return StateFailure("Naga held spike lost");
    const bool fire = same_name(key, "fire_spike");
    s->SetObjectFlags(spike, fire ? s->GetObjectFlags(spike) & ~FLAG_VISIBLE : s->GetObjectFlags(spike) | FLAG_VISIBLE);
    return !fire || Fire();
}
bool Naga::Fire(bool alternate)
{
    auto* s = GetServerDE();
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    ++fire_calls;
    if (!weapon)
        return StateFailure("Naga weapon missing");
    DRotation rotation;
    const auto type = weapon->data.m_nType;
    if (type == WEAP_NAGA_EYEBEAM || type == WEAP_NAGA_SPIKE)
    {
        if (!s->GetModelNodeTransform(m_hObject, type == WEAP_NAGA_SPIKE ? "l_gun" : "head_gun", &fire_origin,
                                      &rotation))
            return StateFailure("Naga weapon node unavailable");
    }
    else if (s->GetObjectPos(m_hObject, &fire_origin) != DE_OK)
        return StateFailure("Naga position unavailable");
    fire_direction = vector_subtract(m_vTargetPos, fire_origin);
    const float length = std::hypot(fire_direction.x, fire_direction.y, fire_direction.z);
    if (!std::isfinite(length) || length < 1e-6f)
        return StateFailure("Naga aim degenerate");
    fire_direction = vector_scale(fire_direction, 1 / length);
    int seed;
    if (!s->RandomInt(0, 255, seed))
        return StateFailure("Naga fire RNG unavailable");
    WeaponFire request;
    request.position = fire_origin;
    request.direction = fire_direction;
    request.weapon = type;
    request.alternate = alternate;
    request.seed = static_cast<DBYTE>(seed);
    fire_result = m_InventoryMgr.FireCurrentWeapon(request);
    if (fire_result == FireResult::Fired)
        ++fire_shots;
    else if (fire_result == FireResult::Dry)
        ++fire_dry;
    else
    {
        ++fire_failures;
        fire_error = "Naga weapon fire failed";
        return false;
    }
    return true;
}
bool Naga::Fade(bool in)
{
    auto* s = GetServerDE();
    const int command = in ? 101 : 20;
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        auto flags = s->GetObjectFlags(m_hObject);
        s->SetObjectFlags(m_hObject, in ? flags | FLAG_SOLID : flags & ~FLAG_SOLID);
        m_bAnimating = true;
        m_nCurMetacmd = command;
    }
    else
    {
        DVector color;
        float alpha;
        if (!s->GetObjectColor(m_hObject, color, alpha))
            return StateFailure("Naga fade color unavailable");
        alpha = std::clamp(alpha + (in ? 1.f : -1.f) / 64, 0.f, 1.f);
        if (!s->SetObjectColor(m_hObject, color, alpha))
            return StateFailure("Naga fade rejected");
        if (alpha == (in ? 1.f : 0.f))
        {
            if (!in)
                s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_VISIBLE);
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
bool Naga::MC_Dead()
{
    return Fade(false);
}
bool Naga::AI_STATE_Special(int state)
{
    if (state != 1)
        return AI_Mgr::AI_STATE_Special(state);
    if (Metacmd == 1)
        return Fade(true);
    if (Metacmd == 2)
    {
        GetServerDE()->SetObjectFlags(m_hObject, GetServerDE()->GetObjectFlags(m_hObject) | FLAG_SOLID);
        return SetNewState(StateIdle);
    }
    return true;
}
