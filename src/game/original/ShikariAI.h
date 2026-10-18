#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/ShikariAI: smell-driven decisions, claw/spit attacks, pouncing jump,
// dodges, guard and Special1. Construction/model/table live in AIRuntime.cpp. See
// shikari_contracts.md.
class ShikariAI : public AI_Mgr
{
  public:
    std::uint64_t attack_updates = 0, pounces = 0, dodges = 0, spit_clips = 0, claw_clips = 0;
    int last_fire_clip = 0;
    float attack_distance = 0, attack_height = 0;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    bool AI_STATE_Dodge() override;
    bool AI_STATE_GuardLocation() override;
    bool AI_STATE_Special(int) override;
    bool MC_Fire_Stand() override;

  private:
    bool Pounce();
    bool Dodge(bool right);
    bool Ready(const char* what);
    float Height() const;
    bool Above() const;
};
