#include "LaterEnemies.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
namespace
{
    bool idle_state(AI_Mgr& ai, bool smell = false)
    {
        return ai.SetNewState(ai.m_nState == AI_Mgr::StateIdle || ai.m_nState == AI_Mgr::StateSearchVisual ||
                                      ai.m_nState == AI_Mgr::StateSearchSmell
                                  ? AI_Mgr::StateIdle
                              : smell ? AI_Mgr::StateSearchSmell
                                      : AI_Mgr::StateSearchVisual);
    }
    bool face(AI_Mgr& ai)
    {
        if (!ai.MC_FaceTarget())
            return false;
        --ai.Metacmd;
        return true;
    }
    float distance(AI_Mgr& ai)
    {
        const auto v = vector_subtract(ai.m_vTargetPos, ai.m_MoveObj.GetPos());
        return ai.m_fStimuli[0] = std::sqrt(vector_dot(v, v));
    }
    SoundId ambient(HOBJECT owner, const char* resource, float volume, std::string& error)
    {
        SoundRequest request;
        request.resource = resource;
        request.inner_radius = 100;
        request.outer_radius = 1000;
        request.volume = volume;
        request.priority = 3;
        request.speaker = owner;
        request.loop = true;
        return g_pServerDE->PlayObjectLoopSound(owner, request, error);
    }
} // namespace
bool Behemoth::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("Behemoth stimuli unavailable");
    if (!stim)
        return idle_state(*this, true);
    if (m_fStimuli[5] < .25f)
        return SetNewState(StateAttackFar);
    // Original tests the argument, not the computed smell stimulus.
    return SetNewState(value == 3 ? StateSearchSmell : m_fStimuli[0] > .70f ? StateAttackClose : StateAttackFar);
}
bool Behemoth::MC_Fire_Stand()
{
    if (!AnimSound())
        return StateFailure("Behemoth animation table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        const bool close = m_nState == StateAttackClose;
        const auto result = m_InventoryMgr.ChangeWeapon(close ? WEAP_BEHEMOTH_CLAW : WEAP_BEHEMOTH_SHOCKWAVE);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("Behemoth weapon unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[close ? 4 : m_fStimuli[5] <= .33f ? 6 : 5]))
            return false;
        GetServerDE()->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
    }
    else if (GetServerDE()->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_InventoryMgr.ChangeWeapon(WEAP_BEHEMOTH_CLAW);
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool Behemoth::AI_STATE_AttackClose()
{
    if (!GetTarget())
        return SetNewState(StateSearchVisual);
    const float dist = distance(*this);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            if (dist <= 100 || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            return face(*this) && (dist >= m_fSeeingDist * .30f ? ComputeState() : MC_Run());
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
bool Behemoth::AI_STATE_AttackFar()
{
    if (!GetTarget())
        return SetNewState(StateSearchVisual);
    const float dist = distance(*this);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            if (dist <= m_fSeeingDist * .70f || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            [[fallthrough]];
        case 3:
            return face(*this) && (dist < m_fSeeingDist * .30f ? ComputeState() : MC_Walk());
        case 4:
            return ComputeState();
        default:
            return true;
    }
}
DrudgePriest::~DrudgePriest()
{
    if (loop && g_pServerDE)
        g_pServerDE->KillSound(loop);
}
DDWORD DrudgePriest::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    const auto result = AI_Mgr::EngineMessageFn(msg, data, reason);
    if (msg == MID_INITIALUPDATE && RuntimeReady())
    {
        auto* s = GetServerDE();
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY);
        loop = ambient(m_hObject, "sounds\\enemies\\drudgepriest\\dp-looptry1.wav", .6f, sound_error);
    }
    return result;
}
bool DrudgePriest::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("DrudgePriest stimuli unavailable");
    if (!stim)
        return idle_state(*this);
    return SetNewState(m_fStimuli[5] < .25f || m_fStimuli[4] > .75f || m_fStimuli[0] <= .5f ? StateAttackFar
                                                                                            : StateAttackClose);
}
bool DrudgePriest::MC_Fire_Stand()
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("DrudgePriest animation table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        if (!Move({}, 1e-11f))
            return false;
        const bool far = m_nState == StateAttackFar;
        const auto result = m_InventoryMgr.ChangeWeapon(far ? WEAP_DRUDGE_FIREBALL : WEAP_DRUDGE_LIGHTNING);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return StateFailure("DrudgePriest weapon unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[far ? 0 : 5]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_InventoryMgr.ChangeWeapon(WEAP_DRUDGE_FIREBALL);
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool DrudgePriest::MC_Taunt_Bold()
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("DrudgePriest spawn animation unavailable");
    const auto type = s->GetClass("BoneLeech");
    if (!m_bAnimating || m_nCurMetacmd != 18)
    {
        if (s->GetTime() - last_spawn < 30)
        {
            m_bAnimating = false;
            ++Metacmd;
            return true;
        }
        m_bCabal = true; // Source temporarily selects its own monster list.
        const auto found = FindObjectInRadius(AIFindList::Enemies, m_fSeeingDist, FindSpecific, type);
        m_bCabal = false;
        if (!find.error.empty())
            return StateFailure(find.error.c_str());
        if (found)
        {
            m_bAnimating = false;
            ++Metacmd;
            return true;
        }
        if (!SetAnimation(AnimSound()->m_nAnim_TAUNT[4]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 18;
    }
    else
    {
        if (s->GetTime() - last_spawn >= 1.5f)
        {
            ObjectCreateStruct state;
            state.m_Pos = vector_add(m_MoveObj.GetPos(), vector_scale(m_MoveObj.GetForwardVector(), 20));
            state.m_Rotation = m_MoveObj.GetRotation();
            if (!s->CreateObject(type, &state))
                return StateFailure("DrudgePriest BoneLeech spawn failed");
            ++spawned;
            last_spawn = s->GetTime();
        }
        if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            last_spawn = s->GetTime();
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
bool DrudgePriest::MC_Dead()
{
    if (loop)
    {
        GetServerDE()->KillSound(loop);
        loop = 0;
    }
    return AI_Mgr::MC_Dead();
}
bool DrudgePriest::AI_STATE_AttackClose()
{
    if (!GetTarget() && m_nCurMetacmd != 7)
        return SetNewState(StateSearchVisual);
    const auto dist = distance(*this);
    if (!face(*this))
        return false;
    switch (Metacmd)
    {
        case 1:
            return MC_Taunt_Bold();
        case 2:
            if (dist <= m_fSeeingDist * .33f || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            return dist >= m_fSeeingDist * .5f ? ComputeState() : MC_Run();
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
bool DrudgePriest::AI_STATE_AttackFar()
{
    if (!GetTarget() && m_nCurMetacmd != 7)
        return SetNewState(StateSearchVisual);
    const auto dist = distance(*this);
    if (!face(*this))
        return false;
    switch (Metacmd)
    {
        case 1:
            if (dist <= m_fSeeingDist * .75f || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            [[fallthrough]];
        case 2:
            return dist < m_fSeeingDist * .5f ? ComputeState() : MC_Run();
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
DeathShroud::~DeathShroud()
{
    if (loop && g_pServerDE)
        g_pServerDE->KillSound(loop);
}
DDWORD DeathShroud::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    const auto result = AI_Mgr::EngineMessageFn(msg, data, reason);
    if (msg == MID_INITIALUPDATE && RuntimeReady())
    {
        auto* s = GetServerDE();
        m_vScale = {1.5f, 1.5f, 1.5f}; // Source changes scale metadata after base initialization.
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_SHADOW);
        DVector color;
        float alpha;
        if (!s->GetObjectColor(m_hObject, color, alpha) || !s->SetObjectColor(m_hObject, color, .9f))
            return RuntimeFailure("DeathShroud color unavailable");
        loop = ambient(m_hObject, "sounds\\enemies\\deathshroud\\de_loop_2.wav", .5f, sound_error);
    }
    return result;
}
bool DeathShroud::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("DeathShroud stimuli unavailable");
    return stim ? SetNewState(StateAttackFar) : idle_state(*this);
}
bool DeathShroud::Fade(bool out)
{
    auto* s = GetServerDE();
    if (!AnimSound())
        return StateFailure("DeathShroud fade animation unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 18)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_TAUNT[4]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 18;
        if (out)
            s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_SOLID);
    }
    else
    {
        DVector color;
        float alpha;
        if (!s->GetObjectColor(m_hObject, color, alpha))
            return StateFailure("DeathShroud color unavailable");
        alpha = out ? std::max(.5f, alpha - .05f) : std::min(.9f, alpha + .05f);
        if (!s->SetObjectColor(m_hObject, color, alpha))
            return false;
        if ((s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE) && (out ? alpha <= .5f : alpha >= .9f))
        {
            if (!out)
            {
                s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_SOLID);
                if (s->ResolveInitialWorldOverlap(m_hObject) != DE_OK)
                    return StateFailure("DeathShroud solid reappearance blocked");
            }
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
bool DeathShroud::MC_Fire_Stand()
{
    if (!AnimSound())
        return StateFailure("DeathShroud attack animation unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[8]))
            return false;
        GetServerDE()->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
    }
    else if (GetServerDE()->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}
bool DeathShroud::AI_STATE_AttackFar()
{
    const auto height = std::abs(m_vTargetPos.y - m_MoveObj.GetPos().y);
    if (!GetTarget() && m_nCurMetacmd != 7)
        return height <= m_vDims.y ? SetNewState(StateSearchVisual) : ComputeState();
    const auto dist = distance(*this);
    if (!face(*this))
        return false;
    switch (Metacmd)
    {
        case 1:
            return Fade(false);
        case 2:
            if (dist <= m_fSeeingDist * .66f || m_nCurMetacmd == 7 || height > m_vDims.y)
                return MC_Fire_Stand();
            ++Metacmd;
            return true;
        case 3:
            return Fade(true);
        case 4:
            if (dist <= m_fSeeingDist * .33f || height > m_vDims.y)
            {
                ++Metacmd;
                return true;
            }
            return MC_Walk();
        case 5:
            return ComputeState();
        default:
            return true;
    }
}
bool DeathShroud::MC_Dead()
{
    auto* s = GetServerDE();
    DVector color;
    float alpha;
    if (!s->GetObjectColor(m_hObject, color, alpha))
        return StateFailure("DeathShroud death color unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 20)
    {
        if (loop)
        {
            s->KillSound(loop);
            loop = 0;
        }
        s->SetObjectColor(m_hObject, color, 1);
        // Original selects corpse metadata but never starts the death clip here.
        constexpr int nodes[]{1, 2, 4, 7, 10, 14};
        for (unsigned i = 0; i < 6; ++i)
            if (m_damage.node_hit == nodes[i] && (m_damage.side_hit == 0 || m_damage.side_hit == 6))
                m_nCorpseType = AnimSound()->m_nAnim_DEATH[i + m_damage.side_hit];
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 20;
    }
    else
    {
        m_vScale = vector_scale(m_vScale, .9f);
        m_vDims = vector_scale(m_vDims, .9f);
        if (!s->ScaleObject(m_hObject, m_vScale) || s->SetObjectDims(m_hObject, &m_vDims) != DE_OK ||
            !s->SetObjectColor(m_hObject, color, std::max(0.f, alpha - .05f)))
            return StateFailure("DeathShroud shrink failed");
        if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            m_bAnimating = false;
            ++Metacmd;
            m_bRemoveMe = true;
        }
    }
    return true;
}
bool TheHandAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("TheHand stimuli unavailable");
    return stim ? SetNewState(StateAttackClose) : idle_state(*this);
}
bool TheHandAI::AI_STATE_AttackClose()
{
    if (!GetTarget() && m_nCurMetacmd != 10)
        return ComputeState();
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            return MC_Taunt_Bold();
        case 3:
        {
            if (distance(*this) <= 100 || m_nCurMetacmd == 10)
                return FireJump();
            if (!face(*this))
                return false;
            bool ledge;
            if (!IsLedge(m_MoveObj.GetForwardVector(), ledge))
                return StateFailure(locomotion_error.c_str());
            return ledge ? MC_Idle() : MC_Run();
        }
        case 4:
            return ComputeState();
        default:
            return true;
    }
}
