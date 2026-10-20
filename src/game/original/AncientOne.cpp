// Original AncientOne.cpp and AncientOneTentacle.cpp selected runtime branches.
#include "AncientOne.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
bool AncientOne::PublishHealth(float fraction)
{
    ++health_requests;
    health_error.clear();
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) || !std::isfinite(fraction))
    {
        health_result = DE_INVALIDPARAMS;
        health_error = "AncientOne health owner/ratio unavailable";
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
        health_error = "AncientOne boss-health delivery failed";
        return StateFailure(health_error.c_str());
    }
    return true;
}
DDWORD AncientOne::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
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
bool AncientOne::OnAcceptedDamage()
{
    // Same consumed-reader adaptation as Gideon: apply one typed damage value,
    // publish health, then the existing base AI reaction; never duplicate damage.
    if (!std::isfinite(m_fFullHealth) || m_fFullHealth == 0)
    {
        health_error = "AncientOne damage health denominator is zero";
        return StateFailure(health_error.c_str());
    }
    return PublishHealth(m_damage.m_fHitPoints / m_fFullHealth) && HandleDamage();
}

namespace
{
    bool ready(AI_Mgr& a)
    {
        auto* s = a.GetServerDE();
        return s && s->IsObjectAlive(a.m_hObject) && a.AnimSound() && a.m_MoveObj.initialized && a.m_MoveObj.sampled &&
               physics_vector(a.m_vTargetPos) && a.Metacmd > -1000000 && a.Metacmd < 1000000;
    }
    bool fire_animation(AI_Mgr& a)
    {
        if (!ready(a))
        {
            a.state_error = "Ancient attack owner/table unavailable";
            ++a.state_failures;
            return false;
        }
        auto* s = a.GetServerDE();
        if (!a.m_bAnimating || a.m_nCurMetacmd != 7)
        {
            if (auto* w = a.m_InventoryMgr.CurrentWeapon())
                a.m_fAttackLoadTime = w->data.m_fReloadTime;
            if (!a.SetAnimation(a.AnimSound()->m_nAnim_FIRE_STAND[TYPE_MELEE]))
                return false;
            s->SetModelLooping(a.m_hObject, false);
            a.m_bAnimating = true;
            a.m_nCurMetacmd = 7;
        }
        else if (s->GetModelPlaybackState(a.m_hObject) & MS_PLAYDONE)
        {
            a.m_bAnimating = false;
            ++a.Metacmd;
        }
        return true;
    }
    bool idle_state(AI_Mgr& a)
    {
        const int stim = a.ComputeStimuli();
        if (stim < 0)
            return false;
        if (stim)
            return a.ComputeState(stim);
        return a.Metacmd == 1 ? a.MC_Idle() : a.Metacmd == 2 ? a.ComputeState() : true;
    }
} // namespace
bool AncientOne::ComputeState(int stimulus)
{
    const int s = stimulus == -1 ? ComputeStimuli() : stimulus;
    if (s < 0 || s > 4)
        return StateFailure("Ancient One stimulus unavailable");
    return SetNewState(s ? StateAttackFar : m_nState == StateSearchVisual ? StateIdle : StateSearchVisual);
}
bool AncientOne::MC_Fire_Stand()
{
    if (!ready(*this))
        return StateFailure("Ancient One attack state unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        const auto d = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
        m_fStimuli[0] = std::sqrt(vector_dot(d, d));
        const auto r =
            m_InventoryMgr.ChangeWeapon(m_fStimuli[0] < 300 ? WEAP_ANCIENTONE_TENTACLE : WEAP_ANCIENTONE_BEAM);
        if (r != InventoryResult::Ok && r != InventoryResult::Current)
            return StateFailure("Ancient weapon unavailable");
    }
    return fire_animation(*this);
}
bool AncientOne::AI_STATE_AttackFar()
{
    if (!GetTarget())
        return SetNewState(StateSearchVisual);
    if (!ready(*this) || !MC_FaceTarget())
        return false;
    --Metacmd;
    return Metacmd == 1 ? MC_Idle() : Metacmd == 2 ? MC_Fire_Stand() : Metacmd == 3 ? ComputeState() : true;
}
bool AncientOne::AI_STATE_Idle()
{
    return idle_state(*this);
}
bool AncientOne::SpawnTentacle()
{
    auto* s = GetServerDE();
    DVector pos;
    float x, z;
    int sx, sz;
    if (!s || s->GetObjectPos(m_hObject, &pos) != DE_OK || !s->RandomFloat(0, 500, x) || !s->RandomInt(0, 1, sx) ||
        !s->RandomFloat(0, 500, z) || !s->RandomInt(0, 1, sz))
        return StateFailure("Tentacle spawn position/RNG unavailable");
    pos.x += sx ? x : -x;
    pos.z += sz ? z : -z;
    // Original tests absolute WORLD coordinates, with integer truncation.
    if (std::abs(pos.x) < 300 && std::abs(pos.z) < 300)
    {
        pos.x += pos.x < 0 ? -300 : 300;
        pos.z += pos.z < 0 ? -300 : 300;
        pos.y += 200;
    }
    ObjectCreateStruct state;
    state.m_ObjectType = OT_MODEL;
    state.m_Pos = pos;
    auto* child = dynamic_cast<AncientOneTentacle*>(s->CreateObject(s->GetClass("AncientOneTentacle"), &state));
    if (!child || !child->RuntimeReady())
    {
        if (child)
            s->RemoveObject(child->m_hObject);
        return StateFailure("Tentacle spawn initialization failed");
    }
    last_tentacle = child->m_hObject;
    ++tentacles_spawned;
    return true;
}
DDWORD AncientOneTentacle::ObjectMessageFn(HOBJECT sender, DDWORD id, HMESSAGEREAD message)
{
    auto* s = GetServerDE();
    if (id == damage_message && s && s->IsKindOf(s->GetObjectClass(sender), s->GetClass("AncientOne")))
    {
        ++ignored_boss_damage;
        return 1;
    } // Intentional ignore is accepted typed delivery, no damage/reaction.
    return AI_Mgr::ObjectMessageFn(sender, id, message);
}
bool AncientOneTentacle::ComputeState(int stimulus)
{
    const int s = stimulus == -1 ? ComputeStimuli() : stimulus;
    if (s < 0 || s > 4 || !ready(*this))
        return StateFailure("Tentacle stimulus unavailable");
    if (!s)
        return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual ? StateIdle : StateSearchVisual);
    const auto d = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
    m_fStimuli[0] = std::sqrt(vector_dot(d, d));
    return SetNewState(m_fStimuli[0] <= 500 ? StateAttackClose : StateIdle);
}
bool AncientOneTentacle::MC_Fire_Stand()
{
    return fire_animation(*this);
}
bool AncientOneTentacle::MC_Idle()
{
    if (!ready(*this))
        return StateFailure("Tentacle idle table unavailable");
    auto* s = GetServerDE();
    if (!m_bAnimating || m_nCurMetacmd != 0)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_IDLE[0]))
            return false;
        s->SetModelLooping(m_hObject, true);
        m_bAnimating = true;
        m_nCurMetacmd = 0;
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
    }
    return true; // The source's 30-second hide block is commented out.
}
bool AncientOneTentacle::AI_STATE_Idle()
{
    return idle_state(*this);
}
bool AncientOneTentacle::AI_STATE_AttackClose()
{
    if (!GetTarget() && m_nCurMetacmd != 7)
        return SetNewState(StateSearchVisual);
    if (!ready(*this))
        return StateFailure("Tentacle attack cache unavailable");
    switch (Metacmd)
    {
        case 1:
            return MC_ScaleUp();
        case 2:
            return MC_FaceTarget();
        case 3:
        {
            const auto d = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
            m_fStimuli[0] = std::sqrt(vector_dot(d, d));
            return m_fStimuli[0] <= 500 || m_nCurMetacmd == 7 ? MC_Fire_Stand() : SetNewState(StateIdle);
        }
        default:
            return ComputeState();
    }
}
bool AncientOneTentacle::MC_Dead()
{
    if (!ready(*this))
        return StateFailure("Tentacle revival owner unavailable");
    m_fTimeOfDeath = GetServerDE()->GetTime();
    m_damage.SetHitPoints(200);
    ++revivals;
    return SetNewState(StateSpecial2);
}
bool AncientOneTentacle::AI_STATE_Special(int state)
{
    switch (state)
    {
        case 1:
            if (Metacmd == 1)
                return MC_ScaleUp();
            if (Metacmd == 2)
            {
                m_bHiding = false;
                return SetNewState(StateIdle);
            }
            return true;
        case 2:
        case 4:
            if (Metacmd == 1)
                return MC_ScaleDown();
            if (Metacmd == 2)
            {
                if (state == 2)
                    return SetNewState(StateSpecial3);
                m_bHiding = true;
                return SetNewState(StateIdle);
            }
            return true;
        case 3:
            if (Metacmd == 1 && GetServerDE()->GetTime() - m_fTimeOfDeath >= 10)
                ++Metacmd;
            else if (Metacmd == 2)
                return SetNewState(StateSpecial1);
            return true;
        default:
            return AI_Mgr::AI_STATE_Special(state);
    }
}
bool AncientOneTentacle::Fade(bool in)
{
    if (!ready(*this))
        return StateFailure("Tentacle fade owner unavailable");
    auto* s = GetServerDE();
    const int cmd = in ? 101 : 102;
    if (!m_bAnimating || m_nCurMetacmd != cmd)
    {
        const auto flags = s->GetObjectFlags(m_hObject);
        s->SetObjectFlags(m_hObject, in ? flags | FLAG_SOLID : flags & ~FLAG_SOLID);
        m_bAnimating = true;
        m_nCurMetacmd = cmd;
        return true;
    }
    DVector color;
    float a;
    if (!s->GetObjectColor(m_hObject, color, a))
        return StateFailure("Tentacle color unavailable");
    a = in ? std::min(1.f, a + 1.f / 64) : std::max(0.f, a - 1.f / 64);
    if (!s->SetObjectColor(m_hObject, color, a))
        return StateFailure("Tentacle fade failed");
    if (in ? a >= 1 : a <= 0)
    {
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool AncientOneTentacle::MC_FadeIn()
{
    return Fade(true);
}
bool AncientOneTentacle::MC_FadeOut()
{
    return Fade(false);
}
bool AncientOneTentacle::Scale(bool up)
{
    if (!ready(*this) || !std::isfinite(m_fScaleBy))
        return StateFailure("Tentacle scale state unavailable");
    auto* s = GetServerDE();
    const int cmd = up ? 104 : 103;
    if (!m_bAnimating || m_nCurMetacmd != cmd)
    {
        if (!up)
        {
            s->SetObjectFlags(m_hObject, (s->GetObjectFlags(m_hObject) & ~FLAG_SOLID) | FLAG_GRAVITY);
            m_fScaleBy = 2.5f;
            if (!SetAnimation(AnimSound()->m_nAnim_IDLE[0]))
                return false;
            s->SetModelLooping(m_hObject, true);
        }
        m_bAnimating = true;
        m_nCurMetacmd = cmd;
        return true;
    }
    m_fScaleBy = up ? std::min(2.5f, m_fScaleBy + .125f) : std::max(.01f, m_fScaleBy - .125f);
    DVector dims, pos;
    if (s->GetModelAnimUserDims(m_hObject, &dims, 0) != DE_OK)
        return StateFailure("Tentacle base dimensions unavailable");
    m_vScale = {m_fScaleBy, m_fScaleBy, m_fScaleBy};
    dims = vector_scale(dims, m_fScaleBy);
    if (!s->ScaleObject(m_hObject, m_vScale) || s->SetObjectDims(m_hObject, &dims) != DE_OK)
        return StateFailure("Tentacle scale/dimensions rejected");
    if (up)
    {
        if (s->GetObjectPos(m_hObject, &pos) != DE_OK)
            return StateFailure("Tentacle scale position unavailable");
        pos.y += 200;
        if (s->TeleportObject(m_hObject, &pos) != DE_OK)
            return StateFailure("Tentacle scale lift failed");
    }
    const auto ground = s->MoveObjectToGround(m_hObject);
    if (ground != DE_OK && ground != DE_NOTFOUND)
        return StateFailure("Tentacle scale grounding failed");
    ++scale_updates;
    if (up ? m_fScaleBy >= 2.5f : m_fScaleBy <= .01f)
    {
        if (up)
        {
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_SOLID | FLAG_GRAVITY);
            if (!SetAnimation(AnimSound()->m_nAnim_IDLE[0]))
                return false;
            s->SetModelLooping(m_hObject, true);
            m_bAnimating = true;
        }
        ++Metacmd; // Both branches intentionally leave animating set.
    }
    return true;
}
bool AncientOneTentacle::MC_ScaleDown()
{
    return Scale(false);
}
bool AncientOneTentacle::MC_ScaleUp()
{
    return Scale(true);
}
