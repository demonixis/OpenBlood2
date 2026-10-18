// Original ObjectDLL/MadScientistAI.cpp (Monolith). See madscientist_contracts.md.
#include "MadScientistAI.h"

bool MadScientistAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    if (stim < 0)
        return StateFailure("MadScientist stimulus unavailable");
    if (stim)
        return SetNewState(StateEscapeHide);
    int state;
    if (!StateStrToInt(m_szAIState, state))
        return StateFailure("Invalid MadScientist authored state");
    return SetNewState(state);
}

// Source override: no hiding spot search, only the facing beg taunt.
bool MadScientistAI::AI_STATE_Escape_Hide()
{
    ++hide_updates;
    if (!GetTarget())
        return ComputeState();
    if (!MC_FaceTarget())
        return StateFailure("MadScientist target facing failed");
    --Metacmd; // MC_FaceTarget's command increment is undone, as in the source.
    switch (Metacmd)
    {
        case 1:
            return MC_Taunt_Beg();
        case 2:
            return ComputeState();
        default:
            return true;
    }
}

bool MadScientistAI::AI_STATE_Idle()
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject))
        return StateFailure("AI state owner unavailable");
    const int stim = ComputeStimuli();
    if (stim < 0)
        return StateFailure("MadScientist idle perception failed");
    if (stim > 0)
    {
        server->SetNextUpdate(m_hObject, .001f);
        return ComputeState(stim);
    }
    server->SetNextUpdate(m_hObject, .1f);
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
