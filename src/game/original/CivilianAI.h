#pragma once
#include "AI_Mgr.h"
// Selected original state rules only. Not registered for world creation:
// full initialization, damage/trigger handling and AI_Update remain open.
class CivilianAI : public AI_Mgr
{
  public:
    CivilianAI()
    {
        m_fRollSpeed = 3.5;
    } // Selected original constructor default.
    bool AI_STATE_Escape_RunAway() override
    {
        return EscapeRun(true);
    }
    bool AI_STATE_Escape_Hide() override
    {
        return EscapeHide();
    }
    bool m_bScared = false;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_Idle() override;
    bool AI_STATE_Special(int) override;
};
