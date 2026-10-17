#pragma once
#include "AI_Mgr.h"

// Selected original Gideon combat methods. Not registered for world creation:
// initialization and boss-health delivery remain pending.
// See GIDEON_COMBAT_CONTRACTS.md before extending this subclass.
class Gideon : public AI_Mgr
{
  public:
    bool ComputeState(int stimulus = -1) override;
    bool MC_Fire_Stand() override;
    bool Fire(bool alternate = false) override;
    bool MC_Extra(std::string_view) override;
    bool MagicPowerup();
    bool MC_FadeOut();
    bool MC_Dodge_Left();
    bool MC_Dodge_Right();
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    bool AI_STATE_Recoil() override;
    std::uint64_t attack_updates = 0, dodge_calls = 0, dodge_finishes = 0;
    int dodge_roll = -1;
    float attack_distance = 0, dodge_elapsed = 0;
    bool AI_STATE_Teleport() override;
    bool AI_STATE_Special(int) override;
    std::uint64_t fade_calls = 0, teleport_moves = 0;
    DVector teleport_from{}, teleport_to{}, teleport_direction{}, teleport_requested{}, teleport_actual{};
    OpenBlood2::SegmentResult teleport_ray = OpenBlood2::SegmentResult::Unavailable;
    bool teleport_sound_started = false, teleport_position_valid = false;
    std::string teleport_sound_error;

    float m_fShieldDuration = 60, m_fLastShield = 0, m_fLastPanic = 0;
    std::string m_szFireNode = "rr_gun";
    int attack_roll = -1;
    DBYTE selected_weapon = 0;
    std::uint64_t shield_grants = 0, powerup_calls = 0, extra_calls = 0;
    std::uint64_t powerup_effect = 0;
    std::string powerup_error;
    InventoryResult completion_selection = InventoryResult::NotAvailable;

  private:
    bool Dodge(bool right);
    bool AttackReady(bool distance);
    bool TeleportStep(bool special);
    bool teleport_busy = false;
};
