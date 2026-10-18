#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/SoulDrudge: melee decisions, close/far attacks and the random
// standing attack clip. Construction/model/table live in AIRuntime.cpp. See
// souldrudge_contracts.md.
class SoulDrudge : public AI_Mgr
{
  public:
    std::uint64_t attack_updates = 0, fire_stand_clips = 0;
    int last_fire_clip = 0;          // Source m_nAnim_FIRE_STAND index drawn (4..6).
    float attack_distance = 0, attack_height = 0;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override
    {
        return Attack(false);
    }
    bool AI_STATE_AttackFar() override
    {
        return Attack(true);
    }
    bool MC_Fire_Stand() override;

  private:
    bool Attack(bool far);
};
