// Original ObjectDLL/Fanatic.cpp (Monolith, 1997). Construction, properties,
// model and table initialization live in AIRuntime.cpp; attacks/dodge in
// CabalAttack.cpp. See fanatic_prophet_contracts.md.
#include "Fanatic.h"
#include "Projectile.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;

DDWORD Fanatic::EngineMessageFn(DDWORD message, void* data, DFLOAT value)
{
    // Source MID_UPDATE: smoke handling precedes the base AI update.
    if (message == MID_UPDATE && npc_type == NPCType::Fanatic && RuntimeReady())
        UpdateSmoke();
    return AI_Mgr::EngineMessageFn(message, data, value);
}

// Smoke is cosmetic: failures are retained for inspection and never suspend the actor.
bool Fanatic::UpdateSmoke()
{
    auto* s = GetServerDE();
    if (m_hSmokeSource && m_bStartSmoke)
    {
        // SFX_OBJECTFX_ID: offset 0, scale 3, OBJFX_SCALENUMPARTICLES, OBJFX_SMOKING_2 (21).
        std::string error;
        smoke_effect = s->PublishObjectParticles(m_hSmokeSource, 21, error);
        smoke_publications += smoke_effect != 0;
        if (!smoke_effect)
            kamikaze_error = "Fanatic smoke unavailable: " + error;
        m_bStartSmoke = false;
        DVector offset{};
        DRotation rotation;
        if (s->CreateAttachment(m_hObject, m_hSmokeSource, "torso", &offset, &rotation, &m_hAttach) != DE_OK)
            kamikaze_error = "Fanatic smoke attachment failed";
    }
    if (m_nState == StateDying && m_hAttach && m_hSmokeSource)
    {
        s->RemoveAttachment(m_hAttach);
        s->RemoveObject(m_hSmokeSource);
        m_hSmokeSource = nullptr;
        m_hAttach = 0;
    }
    return kamikaze_error.empty();
}

bool Fanatic::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    DVector position;
    auto* server = GetServerDE();
    if (stim < 0 || stim > 4 || !server || !server->IsObjectAlive(m_hObject) ||
        server->GetObjectPos(m_hObject, &position) != DE_OK || !std::isfinite(m_fStimuli[0]) ||
        !std::isfinite(m_fStimuli[5]) || !std::isfinite(m_vDims.y) || m_vDims.y <= 0)
        return StateFailure("Fanatic state stimulus or body unavailable");
    const double height = std::abs(double(m_vTargetPos.y) - position.y);
    if (!std::isfinite(height))
        return StateFailure("Fanatic target height invalid");
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
                if (height <= m_vDims.y && m_fStimuli[5] >= .5f)
                    return SetNewState(StateSearchVisual);
                return SetNewState(StateIdle);
        }
    }
    if (m_fStartTime)
    {
        // A lit Fanatic resumes the kamikaze sprint, skipping its startup commands.
        if (!SetNewState(StateSpecial1))
            return false;
        Metacmd = 3;
        return true;
    }
    if (m_fStimuli[5] < .25f)
        return SetNewState(StateFindHealth);
    if (m_fStimuli[5] < .5f || m_fStimuli[0] <= .5f || height > m_vDims.y)
        return SetNewState(StateAttackFar);
    return SetNewState(StateAttackClose);
}

bool Fanatic::AI_STATE_Escape_RunAway()
{
    ++escape_updates;
    const int stim = ComputeStimuli();
    if (stim < 0)
        return StateFailure(perception_error.c_str());
    if (stim > 0)
        return SetNewState(StateSpecial1);
    if (!m_MoveObj.initialized || !m_MoveObj.sampled || !physics_vector(m_vDims) ||
        Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("Fanatic escape requires the movement cache and bounded state");
    const float dim = std::hypot(m_vDims.x, m_vDims.z) + .1f;
    const auto position = m_MoveObj.GetPos();
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
                return SetNewState(StateSpecial1);
            if (!RefreshTrackedPosition())
                return StateFailure(target_error.c_str());
            const DVector flat{m_vTrackObjPos.x, position.y, m_vTrackObjPos.z};
            const auto d = vector_subtract(flat, position);
            if (std::hypot(d.x, d.y, d.z) <= dim)
                return SetNewState(StateSpecial1);
            if (!MC_FacePos(m_vTrackObjPos))
                return StateFailure(target_error.c_str());
            if (!m_MoveObj.CalculatePath(m_vTrackObjPos))
            {
                // A blocked route is the source's failed CalculatePath; invalid queries stay failures.
                if (m_MoveObj.outcome != CMovement::Outcome::Blocked)
                    return StateFailure(m_MoveObj.error.c_str());
                return SetNewState(StateSpecial1);
            }
            return true;
        }
        case 2:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return SetNewState(StateEscapeRun);
            return MC_FacePos(point);
        }
        case 3:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return StateFailure("Missing source escape phase-3 point"); // Source dereferences it unchecked.
            point.y = position.y;
            const auto d = vector_subtract(point, position);
            if (std::hypot(d.x, d.y, d.z) <= dim / 2)
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

bool Fanatic::AI_STATE_Escape_Hide()
{
    ++escape_updates;
    if (Metacmd != 1)
        return true; // The source switch has only the first command.
    const auto hint = FindObjectInRadius(AIFindList::Exits, m_fSeeingDist);
    if (!find.valid)
        return StateFailure(find.error.c_str());
    if (!SetTrackedObject(hint))
        return StateFailure(target_error.c_str());
    if (!hint)
        return SetNewState(StateAttackClose);
    if (GetServerDE()->GetObjectPos(hint, &m_vDestPos) != DE_OK)
        return StateFailure("Fanatic hide hint position unavailable");
    return SetNewState(StateRunTo);
}

bool Fanatic::AI_STATE_Special(int index)
{
    // Only Special1 is overridden; the original base Special2..8 are empty.
    return index == 1 ? Kamikaze() : true;
}

bool Fanatic::Kamikaze()
{
    auto* s = GetServerDE();
    switch (Metacmd)
    {
        case 1:
            return MC_Special(0);
        case 2:
        {
            ++kamikaze_starts;
            // Light the fuse: hide the dynamite, refill to 50/50 and start the 30 s clock.
            if (s->SetModelNodeHideStatus(m_hObject, "torso_extra1", DTRUE) != DE_OK)
                return StateFailure("Fanatic torso_extra1 node unavailable");
            m_damage.SetHitPoints(50);
            m_damage.SetArmorPoints(50);
            m_fStartTime = s->GetTime();
            ObjectCreateStruct state;
            state.m_ObjectType = OT_NORMAL;
            state.m_NextUpdate = .01f;
            state.m_Flags = FLAG_FORCECLIENTUPDATE;
            if (s->QueryModelNodeTransform(m_hObject, "torso_extra1", &state.m_Pos, &state.m_Rotation) !=
                ServerDE::NodeQuery::Found)
                return StateFailure("Fanatic torso_extra1 transform unavailable");
            if (auto* smoke = s->CreateObject(s->GetClass("BaseClass"), &state))
                m_hSmokeSource = smoke->m_hObject;
            else
                kamikaze_error = "Fanatic smoke source creation failed";
            ++Metacmd;
            return true;
        }
        case 3:
        {
            if (!m_MoveObj.initialized || !m_MoveObj.sampled || !physics_vector(m_vTargetPos))
                return StateFailure("Fanatic sprint requires the movement cache and a target position");
            const auto d = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
            m_fStimuli[0] = std::hypot(d.x, d.y, d.z);
            if (m_fStimuli[0] <= 100 || s->GetTime() - m_fStartTime >= 30)
            {
                ++Metacmd;
                return true;
            }
            if (!MC_FaceTarget())
                return false;
            --Metacmd;
            return MC_Run();
        }
        case 4:
            if (!Detonate())
                return false;
            ++Metacmd;
            return true;
        default:
            return true; // Case 5: wait for removal.
    }
}

bool Fanatic::AI_STATE_Dying()
{
    return m_fStartTime ? Detonate() : AI_Mgr::AI_STATE_Dying();
}

// Shared by Special1 command 4 and the lit Dying override: a downward howitzer
// shell at the body, explosion gibs, smoke removal and actor removal.
bool Fanatic::Detonate()
{
    if (m_bRemoveMe)
        return true;
    auto* s = GetServerDE();
    if (!m_MoveObj.initialized || !m_MoveObj.sampled)
        return StateFailure("Fanatic detonation requires the movement cache");
    ++detonations;
    ObjectCreateStruct state;
    state.m_Rotation = m_MoveObj.GetRotation();
    state.m_Pos = m_MoveObj.GetPos();
    if (auto type = s->GetClass("CHowitzerShell"))
    {
        auto* shell = dynamic_cast<CProjectile*>(s->CreateObject(type, &state));
        if (!shell)
            kamikaze_error = "Howitzer shell creation failed";
        else
        {
            last_shell = shell->m_hObject;
            // WEAP_HOWITZER, damage 50, velocity 250, radius 200.
            if (!shell->Setup(vector_scale(m_MoveObj.GetUpVector(), -1), 6, 50, 250, 200, m_hObject))
                kamikaze_error = "Howitzer shell setup failed: " + shell->error;
        }
    }
    // CreateGibs(up, ((int)GetMass()) >> 5, DAMAGE_TYPE_EXPLODE, 50); cosmetic failures stay in gib_error.
    if (std::isfinite(m_damage.m_fMass) && m_damage.m_fMass >= 0 && m_damage.m_fMass <= 2048)
        CreateGibs(m_MoveObj.GetUpVector(), static_cast<unsigned>(m_damage.m_fMass) / 32, 50);
    else
        gib_error = "AI gib mass exceeds bounded fragment request";
    if (m_hAttach && m_hSmokeSource)
    {
        s->RemoveAttachment(m_hAttach);
        s->RemoveObject(m_hSmokeSource);
        m_hSmokeSource = nullptr;
        m_hAttach = 0;
    }
    m_bRemoveMe = true;
    return true;
}

void Fanatic::SaveSubclass(HMESSAGEWRITE h)
{
    g_pServerDE->WriteToMessageFloat(h, m_fStartTime); // Raw absolute: valid under the restored clock.
}
void Fanatic::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const float start = s->ReadFromMessageFloat(h);
    if (!std::isfinite(start))
    {
        s->RejectPayload(h, "Fanatic Load: fuse clock is not finite");
        return;
    }
    if (start)
    {
        // The smoke source is an unsaved BaseClass; the original recreates it at torso_extra1 and
        // lets the next update publish the smoke and attach it again.
        ObjectCreateStruct state;
        state.m_ObjectType = OT_NORMAL;
        state.m_NextUpdate = .01f;
        state.m_Flags = FLAG_FORCECLIENTUPDATE;
        if (s->QueryModelNodeTransform(m_hObject, "torso_extra1", &state.m_Pos, &state.m_Rotation) !=
            ServerDE::NodeQuery::Found)
        {
            s->RejectPayload(h, "Fanatic Load: torso_extra1 transform unavailable");
            return;
        }
        auto* smoke = s->CreateObject(s->GetClass("BaseClass"), &state);
        if (!smoke)
        {
            s->RejectPayload(h, "Fanatic Load: smoke source creation failed");
            return;
        }
        m_hSmokeSource = smoke->m_hObject;
        m_bStartSmoke = true;
    }
    m_fStartTime = start;
}
