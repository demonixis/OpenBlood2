#pragma once
#include "AI_Mgr.h"
// Original decisions and close/far attack sequencing. Complete initialization,
// AI_Update and remaining actions are missing; no production registration.
class CultistAI : public AI_Mgr
{
  public:
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    float attack_distance = 0, attack_height = 0, attack_threshold = 0;
    std::uint64_t attack_updates = 0;

  private:
    bool Attack(bool far);
};
