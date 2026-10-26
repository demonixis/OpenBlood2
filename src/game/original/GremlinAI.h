#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/Gremlin.cpp (_ADD_ON GremlinAI): claw and thrown rocks, a pouncing jump,
// dodges, guard and Special1, close to ShikariAI. Construction/model/table live in
// AIRuntime.cpp. See nightmares_edition_contracts.md.
class GremlinAI : public AI_Mgr
{
  public:
    std::uint64_t attack_updates = 0, jumps = 0, dodges = 0, rock_clips = 0, claw_clips = 0, taunts = 0;
    int taunt_roll = -1;
    float attack_distance = 0, attack_height = 0;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    bool AI_STATE_Dodge() override;
    bool AI_STATE_GuardLocation() override;
    bool AI_STATE_Special(int) override;
    bool MC_Fire_Stand() override;

  private:
    bool Jump();
    bool Dodge(bool right);
    bool Ready(const char* what);
    float Height() const;
    bool Above() const;
};
