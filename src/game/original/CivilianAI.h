#pragma once
#include "AI_Mgr.h"
// Original selected Civilian rules, registered lifecycle and update loop.
// Remaining damage/death actions are explicit limitations in ai_runtime_contracts.md.
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
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool m_bScared = false;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_Idle() override;
    bool AI_STATE_Special(int) override;

  protected:
    // Original CivilianAI::Save/Load (CivilianAI.cpp:657-699): the packed gender/labtech/scared byte.
    void SaveSubclass(HMESSAGEWRITE) override;
    void LoadSubclass(HMESSAGEREAD) override;
};
