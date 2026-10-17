#pragma once
#include "AI_Mgr.h"

// Selected original Gideon combat methods. Not registered for world creation:
// initialization, fade/teleport states and boss-health delivery remain pending.
// See GIDEON_COMBAT_CONTRACTS.md before extending this subclass.
class Gideon : public AI_Mgr
{
  public:
    bool ComputeState(int stimulus = -1) override;
    bool MC_Fire_Stand() override;
    bool Fire(bool alternate = false) override;
    bool MC_Extra(std::string_view) override;
    bool MagicPowerup();

    float m_fShieldDuration = 60, m_fLastShield = 0, m_fLastPanic = 0;
    std::string m_szFireNode = "rr_gun";
    int attack_roll = -1;
    DBYTE selected_weapon = 0;
    std::uint64_t shield_grants = 0, powerup_calls = 0, extra_calls = 0;
    std::uint64_t powerup_effect = 0;
    std::string powerup_error;
    InventoryResult completion_selection = InventoryResult::NotAvailable;
};
