#pragma once
#include "AI_Mgr.h"
// Original bosses, checked typed services replacing the engine ABI.
class AncientOne final : public AI_Mgr
{
  public:
    float m_fFullHealth = 0, boss_health_fraction = 0;
    bool m_bCreateHealth = true;
    std::uint64_t health_requests = 0, health_deliveries = 0, health_no_client = 0, tentacles_spawned = 0;
    DRESULT health_result = DE_NOTFOUND;
    std::string health_error;
    HOBJECT last_tentacle = nullptr;
    bool ComputeState(int = -1) override;
    bool MC_Fire_Stand() override;
    bool AI_STATE_AttackFar() override;
    bool AI_STATE_Idle() override;
    bool SpawnTentacle();

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    bool OnAcceptedDamage() override;

  private:
    bool PublishHealth(float);
};
class AncientOneTentacle final : public AI_Mgr
{
  public:
    float m_fTimeOfDeath = 0, m_fScaleBy = 1;
    bool m_bHiding = false;
    std::uint64_t ignored_boss_damage = 0, scale_updates = 0, revivals = 0;
    bool ComputeState(int = -1) override;
    bool MC_Fire_Stand() override;
    bool MC_Idle() override;
    bool MC_Dead() override;
    bool MC_FadeIn();
    bool MC_FadeOut();
    bool MC_ScaleDown();
    bool MC_ScaleUp();
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_Idle() override;
    bool AI_STATE_Special(int) override;

  protected:
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool OnAcceptedDamage() override
    {
        return HandleDamage();
    }

  private:
    bool Fade(bool);
    bool Scale(bool);
};
