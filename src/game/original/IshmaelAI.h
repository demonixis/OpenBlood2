#pragma once
#include "AI_Mgr.h"
// Original IshmaelAI/REV share methods with distinct target-loss and gravity branches.
class IshmaelAI : public AI_Mgr
{
  public:
    bool ComputeState(int = -1) override;
    bool MC_Fire_Stand() override;
    bool MC_Dead() override;
    bool AI_STATE_AttackClose() override
    {
        return Attack(false);
    }
    bool AI_STATE_AttackFar() override
    {
        return Attack(true);
    }
    bool AI_STATE_Escape_Hide() override;
    bool AI_STATE_Teleport() override;
    bool AI_STATE_Recoil() override
    {
        return AI_STATE_Teleport();
    }
    bool AI_STATE_Special(int) override
    {
        return true;
    }
    bool Heal();
    bool FadeOut();
    bool Revenant() const
    {
        return npc_type == NPCType::IshmaelREV;
    }
    std::uint64_t heals = 0, teleports = 0, death_deliveries = 0;
    OpenBlood2::SceneExplosionReport death_effect;

  private:
    bool Attack(bool far);
};
class IshmaelREV final : public IshmaelAI
{
};
