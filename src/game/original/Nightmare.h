#pragma once
#include "AI_Mgr.h"
#include "Projectile.h"

class Nightmare final : public AI_Mgr
{
  public:
    bool ComputeState(int = -1) override;
    bool AI_STATE_Idle() override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    bool MC_Fire_Stand() override;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    bool OnAcceptedDamage() override;

  private:
    bool PublishHealth(float);
    float full_health_ = 0;
    bool shockwave_ = false;
};

class CNightmareFireballs final : public CWeapon
{
  public:
    CNightmareFireballs() : CWeapon(WEAP_NIGHTMARE_FIREBALLS) {}
};

class CNightmareFireball final : public CProjectile
{
  public:
    CNightmareFireball();
    ~CNightmareFireball() override;
    HOBJECT tracked_target = nullptr;
    std::uint64_t trail = 0;
    OpenBlood2::SoundId flight_sound = 0;
    std::string sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;

  private:
    bool AcquireTarget(DVector position, const DRotation&);
    bool acquired_ = false;
    bool hit_location_valid_ = false;
    DVector hit_location_{};
};
