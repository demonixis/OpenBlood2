#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/Fanatic: decisions, attacks, dodge, escape and the Special1
// kamikaze (smoke, sprint, howitzer detonation). See fanatic_prophet_contracts.md.
class Fanatic : public AI_Mgr
{
  public:
    float m_fStartTime = 0; // Kamikaze start; nonzero also changes ComputeState and Dying.
    HATTACHMENT m_hAttach = 0;
    HOBJECT m_hSmokeSource = nullptr; // Owned by the original code; removed on detonation/death.
    bool m_bStartSmoke = true;
    std::uint64_t kamikaze_starts = 0, smoke_publications = 0, detonations = 0;
    std::uint64_t smoke_effect = 0;
    HOBJECT last_shell = nullptr; // Checked historical identity of the detonation shell.
    std::string kamikaze_error;
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override
    {
        return CrouchAttack(false, true);
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
    bool AI_STATE_Escape_Hide() override;
    bool AI_STATE_Special(int) override;
    bool AI_STATE_Dying() override;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool Kamikaze();
    bool Detonate();
    bool UpdateSmoke();
};
