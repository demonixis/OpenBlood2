#pragma once
#include "AI_Mgr.h"
// Selected original state rules only. Not registered for world creation:
// full initialization, fleeing, damage/trigger handling and AI_Update remain open.
class CivilianAI : public AI_Mgr
{
  public:
    bool m_bScared = false;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_Idle() override;
    bool AI_STATE_Special(int) override;
};
