#pragma once
#include "AI_Mgr.h"

// Original Gideon lifecycle and selected combat methods.
// See gideon_runtime_contracts.md for message ordering and remaining services.
// See gideon_combat_contracts.md before extending this subclass.
class Gideon : public AI_Mgr
{
  public:
    bool m_bNoFire = false; // EnhancedGideon script suppression only.
    std::uint64_t suppressed_shots = 0, suppressed_powerups = 0;
    float m_fFullHealth = 0, boss_health_fraction = 0;
    bool m_bCreateHealth = true;
    std::uint64_t health_requests = 0, health_deliveries = 0, health_no_client = 0, damage_specials = 0;
    DRESULT health_result = DE_NOTFOUND;
    std::string health_error;
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

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    bool OnAcceptedDamage() override;

  private:
    bool PublishHealth(float);
    bool Dodge(bool right);
    bool AttackReady(bool distance);
    bool TeleportStep(bool special);
    bool teleport_busy = false;
};
