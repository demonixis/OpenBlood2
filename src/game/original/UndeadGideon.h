#pragma once
#include "AI_Mgr.h"
// Original ObjectDLL/UndeadGideon; checked services replace the engine ABI.
class UndeadGideon final : public AI_Mgr
{
  public:
    float m_fFullHealth = 0, boss_health_fraction = 0;
    bool m_bCreateHealth = true;
    std::uint64_t health_requests = 0, health_deliveries = 0, health_no_client = 0;
    DRESULT health_result = DE_NOTFOUND;
    std::string health_error;
    std::uint64_t attack_updates = 0, jumps = 0, dodges = 0;
    float attack_distance = 0, attack_height = 0;
    DBYTE selected_weapon = 0;
    unsigned fire_column = 0;
    bool ComputeState(int stimulus = -1) override;
    bool MC_Fire_Stand() override;
    bool MC_Jump() override;
    bool MC_Dodge_Left();
    bool MC_Dodge_Right();
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    bool AI_STATE_Dodge() override;
    bool AI_STATE_Recoil() override
    {
        return SetNewState(StateDodge);
    }
    bool Fire(bool alternate = false) override;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    bool OnAcceptedDamage() override;
    // Original UndeadGideon::Save/Load (save/load lot 12).
    void SaveSubclass(HMESSAGEWRITE) override;
    void LoadSubclass(HMESSAGEREAD) override;

  private:
    bool PublishHealth(float);
    bool Ready();
    bool Dodge(bool right);
    bool SelectAttack(DBYTE, unsigned);
};
