#pragma once
#include "AI_Mgr.h"
class ZealotAI final : public AI_Mgr
{
  public:
    ~ZealotAI() override;
    bool ComputeState(int = -1) override;
    bool MC_Idle() override;
    bool MC_Fire_Stand() override;
    bool MC_Dead() override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    bool AI_STATE_Escape_Hide() override;
    bool AI_STATE_Teleport() override;
    bool Float(bool);
    bool Heal(bool shield = false);
    bool floating = false, start_staff = false;
    HOBJECT staff = nullptr;
    HATTACHMENT staff_attachment = 0;
    std::string staff_error;

  protected:
    // Original ZealotAI::Save/Load (save/load lot 12).
    void SaveSubclass(HMESSAGEWRITE) override;
    void LoadSubclass(HMESSAGEREAD) override;

  private:
    void ClearStaff();
    bool Attack(bool far);
};
