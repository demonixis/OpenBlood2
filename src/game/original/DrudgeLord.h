#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/DrudgeLord.cpp; construction/model/table live in AIRuntime.cpp.
class DrudgeLord : public AI_Mgr
{
  public:
    bool ComputeState(int stimulus = -1) override;
    bool MC_Fire_Stand() override;
    bool AI_STATE_AttackClose() override
    {
        return Attack(false);
    }
    bool AI_STATE_AttackFar() override
    {
        return Attack(true);
    }
    std::uint64_t attack_updates = 0, fireball_clips = 0, hook_clips = 0;

  private:
    bool Attack(bool far);
};
