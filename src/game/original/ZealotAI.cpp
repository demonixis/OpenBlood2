// Original ObjectDLL/ZealotAI.cpp. Handles, collision and messages use checked adapters.
#include "ZealotAI.h"
#include "CampaignProjectiles.h"
#include "engine/collision.h"
#include "radius_damage.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
void ZealotAI::ClearStaff()
{
    if (!g_pServerDE)
        return;
    if (staff_attachment)
        g_pServerDE->RemoveAttachment(staff_attachment);
    if (g_pServerDE->IsObjectAlive(staff))
        g_pServerDE->RemoveObject(staff);
    staff_attachment = 0;
    staff = nullptr;
    start_staff = false;
}
ZealotAI::~ZealotAI()
{
    ClearStaff();
}
bool ZealotAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("Zealot stimuli unavailable");
    if (!stim)
        return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual || m_nState == StateSearchSmell ||
                                   m_nState == StateEscapeRun || m_nState == StateEscapeHide
                               ? StateIdle
                               : StateSearchVisual);
    if (m_fStimuli[5] < .5f && m_nState != StateEscapeHide)
        return SetNewState(StateEscapeHide);
    return SetNewState(m_fStimuli[3] >= .85f ? StateAttackClose : StateAttackFar);
}
bool ZealotAI::MC_Idle()
{
    if (!AnimSound())
        return StateFailure("Zealot idle table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 0)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_IDLE[floating ? 0 : 1]))
            return false;
        GetServerDE()->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 0;
    }
    else if (GetServerDE()->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool ZealotAI::Heal(bool shield)
{
    const int command = shield ? 103 : 102;
    if (!AnimSound())
        return StateFailure("Zealot spell table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        const auto result = m_InventoryMgr.ChangeWeapon(shield ? WEAP_ZEALOT_SHIELD : WEAP_ZEALOT_HEAL);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("Zealot spell unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[6]))
            return false;
        GetServerDE()->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = command;
    }
    else if (GetServerDE()->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        if (!shield)
            m_damage.SetHitPoints(m_damage.m_fHitPoints * 1.5f);
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool ZealotAI::AI_STATE_Escape_Hide()
{
    return Metacmd == 1 ? Heal() : Metacmd == 2 ? ComputeState() : true;
}
bool ZealotAI::Float(bool up)
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("Zealot float table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 100)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_IDLE[0]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        if (up)
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY);
        if (!Move(vector_scale(m_MoveObj.GetUpVector(), up ? 45.f : -65.f), 1e-11f))
            return false;
        m_bAnimating = true;
        m_nCurMetacmd = 100;
    }
    else
    {
        bool complete;
        if (up)
        {
            SegmentHit hit;
            const auto from = m_MoveObj.GetPos();
            const auto result =
                s->IntersectSegment(from, vector_add(from, vector_scale(m_MoveObj.GetUpVector(), -75)), m_hObject, hit,
                                    false, SegmentObjects::SolidOnly, SegmentFilter::None, SegmentStart::SkipInterior);
            if (result != SegmentResult::Hit && result != SegmentResult::Miss)
                return StateFailure("Zealot float ground ray unavailable");
            complete = result == SegmentResult::Miss;
        }
        else
        {
            DVector velocity;
            if (s->GetVelocity(m_hObject, &velocity) != DE_OK)
                return StateFailure("Zealot float velocity unavailable");
            complete = velocity.y >= -.1f;
        }
        if (complete)
        {
            if (up)
            {
                if (!StopVelocity())
                    return false;
            }
            else
                s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_GRAVITY);
            floating = up;
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
bool ZealotAI::MC_Fire_Stand()
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("Zealot fire table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        int draw = 1;
        const bool close = m_nState == StateAttackClose;
        if (close && !s->RandomInt(1, 2, draw))
            return StateFailure("Zealot fire RNG unavailable");
        const auto result = m_InventoryMgr.ChangeWeapon(
            close ? (draw == 1 ? WEAP_ZEALOT_GROUNDFIRE : WEAP_ZEALOT_SHOCKWAVE) : WEAP_ZEALOT_ENERGYBLAST);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("Zealot fire weapon unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[close ? 5 : 4]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        ClearStaff();
        ObjectCreateStruct state;
        state.m_ObjectType = OT_NORMAL;
        state.m_Flags = FLAG_FORCECLIENTUPDATE;
        if (!s->GetModelNodeTransform(m_hObject, "r_hand_extra2", &state.m_Pos, &state.m_Rotation))
            return StateFailure("Zealot staff node unavailable");
        auto* child = s->CreateObject(s->GetClass("BaseClass"), &state);
        if (!child)
            return StateFailure("Zealot staff creation failed");
        staff = child->m_hObject;
        start_staff = true;
        m_bAnimating = true;
        m_nCurMetacmd = 7;
    }
    else
    {
        if (start_staff && !s->IsObjectAlive(staff))
            start_staff = false; // A restored cast lost its unsaved staff source; the original's NULL calls do nothing.
        if (start_staff)
        {
            // Presentation recipes not yet ported are reported independently of combat.
            s->PublishObjectParticles(staff, npc_divine ? 3 : 4, staff_error);
            DVector offset{};
            DRotation rotation{};
            if (s->CreateAttachment(m_hObject, staff, "r_hand_extra2", &offset, &rotation, &staff_attachment) != DE_OK)
                return StateFailure("Zealot staff attachment failed");
            start_staff = false;
        }
        if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            ClearStaff();
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
bool ZealotAI::Attack(bool far)
{
    if (!GetTarget() && m_nCurMetacmd != 7)
        return SetNewState(StateSearchVisual);
    const auto delta = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
    const float distance = m_fStimuli[3] = std::sqrt(vector_dot(delta, delta));
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            if (far && distance > m_fSensingDist * .75f)
                return floating ? MC_Run() : Float(true);
            if (!far && distance <= m_fSensingDist * .1f && m_nCurMetacmd != 7)
                return SetNewState(StateTeleport);
            ++Metacmd;
            return true;
        case 3:
            if (!MC_FaceTarget())
                return false;
            --Metacmd;
            if (!far && distance >= m_fSensingDist * .15f && m_nCurMetacmd != 7)
                return ComputeState();
            if (!far || distance <= m_fSensingDist * .75f || m_nCurMetacmd == 7)
                return floating || m_nCurMetacmd == 100 ? Float(false) : MC_Fire_Stand();
            ++Metacmd;
            return true;
        case 4:
            return ComputeState();
        default:
            return true;
    }
}
bool ZealotAI::AI_STATE_AttackClose()
{
    return Attack(false);
}
bool ZealotAI::AI_STATE_AttackFar()
{
    return Attack(true);
}
bool ZealotAI::AI_STATE_Teleport()
{
    auto* s = GetServerDE();
    DVector color;
    float alpha;
    switch (Metacmd)
    {
        case 1:
            if (!m_bAnimating || m_nCurMetacmd != 101)
            {
                s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_SOLID);
                m_bAnimating = true;
                m_nCurMetacmd = 101;
            }
            else
            {
                if (!s->GetObjectColor(m_hObject, color, alpha))
                    return StateFailure("Zealot fade color unavailable");
                alpha = std::max(0.f, alpha - 1.f / 16);
                if (!s->SetObjectColor(m_hObject, color, alpha))
                    return StateFailure("Zealot fade rejected");
                if (!alpha)
                {
                    m_bAnimating = false;
                    ++Metacmd;
                }
            }
            return true;
        case 2:
        {
            int draw;
            if (!s->RandomInt(1, 3, draw))
                return StateFailure("Zealot laugh RNG unavailable");
            AnimSound()->PlaySound(m_hObject, "laugh" + std::to_string(draw) + ".wav", 2000, 100);
            ++Metacmd;
            return true;
        }
        case 3:
        {
            DVector direction{};
            if (!s->RandomFloat(-1, 1, direction.x) || !s->RandomFloat(-1, 1, direction.z))
                return StateFailure("Zealot teleport RNG unavailable");
            const auto from = m_MoveObj.GetPos();
            auto to = vector_add(from, vector_scale(direction, m_fSensingDist));
            SegmentHit hit;
            const auto result = s->IntersectSegment(from, to, m_hObject, hit, false, SegmentObjects::All);
            if (result == SegmentResult::Hit)
                to = vector_add(hit.point, vector_scale(direction, -100));
            else if (result != SegmentResult::Miss)
                return StateFailure("Zealot teleport ray unavailable");
            // Bounded replacement movement must check the body before restoring solidity.
            // Keep the source ray/backoff and one movement attempt; do not materialize inside a solid.
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_SOLID);
            const int before = m_nState;
            if (s->MoveObject(m_hObject, &to) != DE_OK || !s->IsObjectAlive(m_hObject) || m_nState != before ||
                Metacmd != 3)
                return StateFailure("Zealot teleport movement or callback state failed");
            ++Metacmd;
            return true;
        }
        case 4:
            if (!s->GetObjectColor(m_hObject, color, alpha) || !s->SetObjectColor(m_hObject, color, 1))
                return StateFailure("Zealot reappearance failed");
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_SOLID);
            ++Metacmd;
            return true;
        case 5:
            return MC_FaceTarget();
        case 6:
            return ComputeState();
        default:
            return true;
    }
}
bool ZealotAI::MC_Dead()
{
    auto* s = GetServerDE();
    if (!m_bAnimating || m_nCurMetacmd != 20)
    {
        DVector color;
        float alpha;
        if (!AnimSound() || !s->GetObjectColor(m_hObject, color, alpha) || !s->SetObjectColor(m_hObject, color, 1) ||
            !SetAnimation(AnimSound()->m_nAnim_DEATH[0]))
            return false;
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY);
        if (!Move(vector_scale(m_MoveObj.GetUpVector(), 30), 1e-11f))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 20;
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
        PublishCampaignExplosion(m_hObject, m_MoveObj.GetPos(), m_MoveObj.GetUpVector(), npc_divine ? 22 : 19,
                                 "sounds\\enemies\\zealot\\magic.wav", 2000);
        int damage;
        if (!s->RandomInt(25, 50, damage))
            return StateFailure("Zealot death damage RNG unavailable");
        DamageObjectsInRadius(m_hObject, this, m_MoveObj.GetPos(), 450, float(damage), DamageType::Explode);
        ClearStaff();
        m_bRemoveMe = true;
    }
    return true;
}
