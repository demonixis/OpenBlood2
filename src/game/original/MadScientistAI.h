#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/MadScientistAI: idle until a stimulus, then faces the target and
// begs. Construction/model/table live in AIRuntime.cpp. See madscientist_contracts.md.
class MadScientistAI : public AI_Mgr
{
  public:
    std::uint64_t hide_updates = 0;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_Escape_Hide() override;
    bool AI_STATE_Idle() override;
};
