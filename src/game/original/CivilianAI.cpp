// Original CivilianAI.cpp:339-366,493-639; see ai_state_contracts.md.
#include "CivilianAI.h"
#include <cmath>

bool CivilianAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0 || stim > 4 || !std::isfinite(m_fStimuli[5]))
        return StateFailure("Civilian state stimulus unavailable or invalid");
    if (stim && m_nState == StateEscapeHide)
        return SetNewState(StateEscapeHide);
    if (stim && (m_bScared || m_fStimuli[5] < 1.f))
    {
        if (!SetNewState(StateEscapeRun))
            return false;
        m_bScared = true;
        return true;
    }
    int state;
    if (!StateStrToInt(m_szAIState, state))
        return StateFailure("Invalid civilian authored state");
    return SetNewState(state);
}
bool CivilianAI::AI_STATE_Idle()
{
    if (!GetServerDE() || !GetServerDE()->IsObjectAlive(m_hObject))
        return StateFailure("AI state owner unavailable");
    if (m_bScared)
    {
        const int stim = ComputeStimuli();
        if (stim < 0)
            return StateFailure("Civilian idle perception failed");
        if (stim > 0)
        {
            GetServerDE()->SetNextUpdate(m_hObject, .001f);
            return ComputeState(stim);
        }
    }
    GetServerDE()->SetNextUpdate(m_hObject, .1f);
    switch (Metacmd)
    {
        case 1:
            return MC_Idle();
        case 2:
            return ComputeState();
        default:
            return true;
    }
}
bool CivilianAI::AI_STATE_Special(int special)
{
    if (!GetServerDE() || !GetServerDE()->IsObjectAlive(m_hObject))
        return StateFailure("AI state owner unavailable");
    if (special < 1 || special > 8)
        return StateFailure("Civilian special state unavailable");
    if (special == 1)
    {
        if (!MC_FaceTarget())
            return StateFailure("Civilian talk target facing failed");
        --Metacmd;
    }
    if (Metacmd == 2)
        return ComputeState();
    if (Metacmd != 1)
        return true;
    if (special == 1)
        return MC_Talk();
    constexpr int clips[]{0, 0, 0, 1, 3, 4, 5, 7, 8};
    int index = clips[special];
    // Source draws on EVERY Metacmd-1 tick, even while MC_Special keeps playing.
    if ((special == 3 || special == 6) && !GetServerDE()->RandomInt(index, index + 1, index))
        return StateFailure("Civilian special random service unavailable");
    return MC_Special(index);
}

// Source CivilianAI::ObjectMessageFn, after class-specific state guards.
DDWORD CivilianAI::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (npc_type == NPCType::Civilian && message == 1000)
    {
        auto* s = GetServerDE();
        auto string = s->ReadFromMessageHString(read);
        const char* data = string ? s->GetStringData(string) : nullptr;
        const bool trigger = data && std::string_view(data) == "TRIGGER";
        if (string)
            s->FreeString(string);
        if (trigger && m_nState != StateEscapeHide && m_nState != StateScript &&
            !(m_nState >= StateSpecial1 && m_nState <= StateSpecial9))
        {
            // Checked non-owning link replaces the source raw assignment.
            if (!SetNewTarget(sender) || !SetNewState(npc_labtech ? StateEscapeHide : StateSpecial1))
                RuntimeFailure("Civilian trigger target/state unavailable");
        }
    }
    const auto result = AI_Mgr::ObjectMessageFn(sender, message, read);
    if (!runtime_failed && npc_type == NPCType::Civilian && message == OpenBlood2::damage_message &&
        GetServerDE()->IsObjectAlive(m_hObject))
    {
        if (!SetNewState(StateEscapeRun))
            RuntimeFailure(state_error);
        return 0;
    }
    return result;
}

// Original CivilianAI::Save/Load: male | labtech << 1 | scared << 2, then the gender table setup
// with its original precedence (every male refreshes the shared male table).
void CivilianAI::SaveSubclass(HMESSAGEWRITE h)
{
    g_pServerDE->WriteToMessageByte(h, DBYTE(npc_male | (npc_labtech << 1) | (m_bScared << 2)));
}
void CivilianAI::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const DBYTE values = s->ReadFromMessageByte(h);
    if (values > 7)
    {
        s->RejectPayload(h, "CivilianAI Load: unknown flag bits");
        return;
    }
    npc_male = values & 1;
    npc_labtech = (values & 2) != 0;
    m_bScared = (values & 4) != 0;
    std::string error;
    if (!InitializeSharedTable(error))
        s->RejectPayload(h, "CivilianAI Load: " + error);
}
