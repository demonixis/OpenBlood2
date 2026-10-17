// Original CultistAI.cpp:269-332; see ai_state_contracts.md.
#include "CultistAI.h"
#include <cmath>

bool CultistAI::ComputeState(int value)
{
    const int stim = value == -1 ? ComputeStimuli() : value;
    DVector position;
    auto* server = GetServerDE();
    if (stim < 0 || stim > 4 || !server || !server->IsObjectAlive(m_hObject) ||
        server->GetObjectPos(m_hObject, &position) != DE_OK || !std::isfinite(m_fStimuli[0]) ||
        !std::isfinite(m_fStimuli[5]) || !std::isfinite(m_vDims.y) || m_vDims.y <= 0)
        return StateFailure("Cultist state stimulus or body unavailable");
    const double height = std::abs(double(m_vTargetPos.y) - position.y);
    if (!std::isfinite(height))
        return StateFailure("Cultist target height invalid");
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
    if (m_fStimuli[5] < .25f)
        return SetNewState(StateEscapeRun);
    if (m_fStimuli[5] < .5f || m_fStimuli[0] <= .5f || height > m_vDims.y)
        return SetNewState(StateAttackFar);
    return SetNewState(StateAttackClose);
}
