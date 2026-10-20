#pragma once
#include "AI_Mgr.h"
// Original decisions/attacks and production lifecycle; remaining action failures
// are documented in ai_runtime_contracts.md.
class CultistAI : public AI_Mgr
{
  public:
    CultistAI()
    {
        m_fRollSpeed = 105;
    } // Selected original constructor default.
    bool AI_STATE_Escape_RunAway() override
    {
        return EscapeRun(false);
    }
    bool AI_STATE_Escape_Hide() override
    {
        return EscapeHide();
    }
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_Dodge() override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    float attack_distance = 0, attack_height = 0, attack_threshold = 0;
    std::uint64_t attack_updates = 0;

  protected:
    // Original CultistAI::Save/Load (CultistAI.cpp:589-647): the gender byte.
    void SaveSubclass(HMESSAGEWRITE) override;
    void LoadSubclass(HMESSAGEREAD) override;

  private:
    bool Attack(bool far);
};
