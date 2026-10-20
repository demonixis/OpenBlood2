#pragma once
#include "AI_Mgr.h"
// Shared original GabriellaAI/REV and OpheliaAI/REV methods, retaining their distinct branches.
class ChosenAI : public AI_Mgr
{
  public:
    bool ComputeState(int = -1) override;
    bool Fire(bool = false) override;
    bool MC_BestWeapon() override;
    bool AI_STATE_AttackClose() override
    {
        return Attack(false);
    }
    bool AI_STATE_AttackFar() override
    {
        return Attack(true);
    }
    bool AI_STATE_Escape_RunAway() override;
    bool AI_STATE_Escape_Hide() override;
    bool AI_STATE_Dodge() override;
    bool AI_STATE_Special(int) override
    {
        return true;
    } // Original base Special1..8 are empty.
    bool Dodge(bool right);
    bool Ophelia() const
    {
        return npc_type == NPCType::Ophelia || npc_type == NPCType::OpheliaREV;
    }
    bool Revenant() const
    {
        return npc_type == NPCType::GabriellaREV || npc_type == NPCType::OpheliaREV;
    }
    std::uint64_t alternate_choices = 0, weapon_switches = 0, sidesteps = 0;

  private:
    bool Attack(bool far);
};
class GabriellaAI final : public ChosenAI
{
};
class GabriellaREV final : public ChosenAI
{
};
class OpheliaAI final : public ChosenAI
{
};
class OpheliaREV final : public ChosenAI
{
};
