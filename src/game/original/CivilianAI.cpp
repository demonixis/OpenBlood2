// Original CivilianAI.cpp:339-366,493-639; see AI_STATE_CONTRACTS.md.
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
