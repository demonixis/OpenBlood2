#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/prophet: decisions, attacks, dodge, melee escape, melee
// standing fire and alternate-fire choice. See fanatic_prophet_contracts.md.
class Prophet : public AI_Mgr
{
  public:
    std::uint64_t melee_swings = 0, alternate_choices = 0;
    int melee_roll = -1;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override
    {
        return CrouchAttack(false, false);
    }
    bool AI_STATE_AttackFar() override
    {
        return CrouchAttack(true, true);
    }
    bool AI_STATE_Dodge() override
    {
        return RollDodge();
    }
    bool AI_STATE_Escape_RunAway() override;
    bool AI_STATE_Escape_Hide() override
    {
        return true; // Not overridden by the Prophet; the original base is empty.
    }
    bool AI_STATE_Special(int) override
    {
        return true; // Original base Special1..8 are empty for the Prophet.
    }
    bool MC_Fire_Stand() override;
    bool Fire(bool alternate = false) override;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
