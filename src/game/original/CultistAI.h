#pragma once
#include "AI_Mgr.h"
// Original decision rules only. Initialization and combat actions are not yet
// integrated; this class has no production registry/factory entry.
class CultistAI : public AI_Mgr
{
  public:
    bool ComputeState(int stimulus = -1) override;
};
