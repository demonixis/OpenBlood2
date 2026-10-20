// Original ObjectDLL/IshmaelAI.cpp and IshmaelREV.cpp, behind checked typed services.
#include "IshmaelAI.h"
#include "CampaignProjectiles.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
bool IshmaelAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("Ishmael stimuli unavailable");
    if (!stim)
        return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual || m_nState == StateSearchSmell ||
                                   m_nState == StateEscapeRun || m_nState == StateEscapeHide
                               ? StateIdle
                               : StateSearchVisual);
    if (m_fStimuli[5] < .1f && m_nState != StateEscapeHide)
        return SetNewState(StateEscapeHide);
    return SetNewState(m_fStimuli[3] < .65f || std::abs(m_vTargetPos.y - m_MoveObj.GetPos().y) > m_vDims.y
                           ? StateAttackFar
                           : StateAttackClose);
}
bool IshmaelAI::Heal()
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("Ishmael heal table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 102)
    {
        const auto result = m_InventoryMgr.ChangeWeapon(WEAP_ZEALOT_HEAL);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("Ishmael heal weapon unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[6]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 102;
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_damage.SetHitPoints(m_damage.m_fHitPoints * 1.1f);
        ++heals;
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool IshmaelAI::AI_STATE_Escape_Hide()
{
    return Metacmd == 1 ? Heal() : Metacmd == 2 ? ComputeState() : true;
}
bool IshmaelAI::MC_Fire_Stand()
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("Ishmael fire table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        const bool close = m_nState == StateAttackClose;
        if (!close)
            m_InventoryMgr.AddAmmo(AMMO_FOCUS, 20);
        const auto result = m_InventoryMgr.ChangeWeapon(close ? WEAP_ZEALOT_SHOCKWAVE : WEAP_ORB);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("Ishmael attack weapon unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[6]))
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
bool IshmaelAI::Attack(bool far)
{
    if (!GetTarget() && (Revenant() || m_nCurMetacmd != 7))
        return SetNewState(StateSearchVisual);
    const auto delta = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
    const float distance = m_fStimuli[3] = std::sqrt(vector_dot(delta, delta));
    const bool high = std::abs(delta.y) > m_vDims.y;
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            if (far && distance > m_fSensingDist * .75f && !high)
                return MC_Run();
            if (!far && distance <= m_fSensingDist * .20f && m_nCurMetacmd != 7)
                return SetNewState(StateTeleport);
            ++Metacmd;
            return true;
        case 3:
            if (!MC_FaceTarget())
                return false;
            --Metacmd;
            if (!far && ((distance >= m_fSensingDist * .35f && m_nCurMetacmd != 7) || high))
                return ComputeState();
            if (!far || distance <= m_fSensingDist * .75f || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            ++Metacmd;
            return true;
        case 4:
            return ComputeState();
        default:
            return true;
    }
}
bool IshmaelAI::FadeOut()
{
    auto* s = GetServerDE();
    if (!m_bAnimating || m_nCurMetacmd != 101)
    {
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~(FLAG_SOLID | (Revenant() ? FLAG_GRAVITY : 0)));
        m_bAnimating = true;
        m_nCurMetacmd = 101;
    }
    else
    {
        DVector color;
        float alpha;
        if (!s->GetObjectColor(m_hObject, color, alpha) ||
            !s->SetObjectColor(m_hObject, color, std::max(0.f, alpha - 1.f / 16)))
            return StateFailure("Ishmael fade unavailable");
        if (alpha <= 1.f / 16)
        {
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
bool IshmaelAI::AI_STATE_Teleport()
{
    auto* s = GetServerDE();
    switch (Metacmd)
    {
        case 1:
            return FadeOut();
        case 2:
        {
            int draw;
            if (!AnimSound() || !s->RandomInt(1, 3, draw))
                return StateFailure("Ishmael laugh unavailable");
            AnimSound()->PlaySound(m_hObject, "laugh" + std::to_string(draw) + ".wav", 1000, 100);
            ++Metacmd;
            return true;
        }
        case 3:
        {
            DVector direction{};
            if (!s->RandomFloat(-1, 1, direction.x) || !s->RandomFloat(-1, 1, direction.z))
                return StateFailure("Ishmael teleport RNG unavailable");
            const auto from = m_MoveObj.GetPos();
            auto to = vector_add(from, vector_scale(direction, m_fSensingDist));
            SegmentHit hit;
            const auto query = s->IntersectSegment(from, to, m_hObject, hit, false, SegmentObjects::All);
            if (query == SegmentResult::Hit)
                to = vector_add(hit.point, vector_scale(direction, -100));
            else if (query != SegmentResult::Miss)
                return StateFailure("Ishmael teleport ray unavailable");
            // Existing replacement body solver checks the destination before restoring solidity.
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_SOLID);
            const auto state = m_nState;
            if (s->MoveObject(m_hObject, &to) != DE_OK || !s->IsObjectAlive(m_hObject) || state != m_nState ||
                Metacmd != 3)
                return StateFailure("Ishmael teleport movement/callback failed");
            ++teleports;
            ++Metacmd;
            return true;
        }
        case 4:
        {
            DVector color;
            float alpha;
            if (!s->GetObjectColor(m_hObject, color, alpha) || !s->SetObjectColor(m_hObject, color, 1))
                return StateFailure("Ishmael reappearance failed");
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_SOLID | (Revenant() ? FLAG_GRAVITY : 0));
            ++Metacmd;
            return true;
        }
        case 5:
            return MC_FaceTarget();
        case 6:
            return ComputeState();
        default:
            return true;
    }
}
bool IshmaelAI::MC_Dead()
{
    auto* s = GetServerDE();
    if (!m_bAnimating || m_nCurMetacmd != 20)
    {
        DVector color;
        float alpha;
        if (!AnimSound() || !s->GetObjectColor(m_hObject, color, alpha) || !s->SetObjectColor(m_hObject, color, 1) ||
            !SetAnimation(AnimSound()->m_nAnim_DEATH[0]))
            return StateFailure("Ishmael death pose unavailable");
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
        const auto origin = m_MoveObj.GetPos();
        if (!PublishDivineShockball(m_hObject, origin, m_MoveObj.GetUpVector(), death_effect,
                                    "sounds\\enemies\\zealot\\magic.wav"))
            return StateFailure("Ishmael death presentation failed");
        std::vector<HOBJECT> candidates;
        if (!s->CollectObjectsTouchingSphere(origin, 450, candidates))
            return StateFailure("Ishmael death sphere unavailable");
        for (auto target : candidates)
        {
            if (!s->IsObjectAlive(m_hObject))
                return false;
            if (target == m_hObject || !s->IsObjectAlive(target))
                continue;
            const auto type = s->GetObjectType(target);
            if (type != OT_MODEL && type != OT_WORLDMODEL)
                continue;
            DVector position, velocity, gravity;
            if (s->GetObjectPos(target, &position) != DE_OK || s->GetVelocity(target, &velocity) != DE_OK ||
                s->GetGlobalForce(&gravity) != DE_OK)
                return StateFailure("Ishmael death candidate pose unavailable");
            const auto delta = vector_subtract(position, origin);
            const float distance = std::sqrt(vector_dot(delta, delta));
            const auto radial =
                distance > 1e-6f ? vector_scale(delta, std::max(0.f, 450 - distance) * 5 / distance) : DVector{};
            velocity = vector_add(velocity, vector_add(vector_scale(gravity, -.15f), radial));
            // Original SetVelocity/rotation calls are commented out. This is only the damage direction.
            int damage;
            if (!s->RandomInt(25, 50, damage))
                return StateFailure("Ishmael death damage RNG unavailable");
            const auto message = s->StartMessageToObject(this, target, damage_message);
            if (!message)
                return StateFailure("Ishmael death damage allocation failed");
            const auto written =
                s->WriteDamage(message, {velocity, position, float(damage), DamageType::Explode, m_hObject});
            const auto delivered = s->EndMessage(message);
            if (written != DE_OK || delivered != DE_OK)
                return StateFailure("Ishmael death damage delivery failed");
            ++death_deliveries;
        }
        m_bRemoveMe = true;
    }
    return true;
}
