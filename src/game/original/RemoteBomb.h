#pragma once
#include "Projectile.h"
// Original CRemoteBomb rules. Flight uses the existing replacement box solver.
class CRemoteBomb final : public CProjectile
{
  public:
    CRemoteBomb();
    bool AdvanceFlight(double) override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    bool Detonate();
    bool armed = false, falling = false;
    float pitch = 0, yaw = 0, pitch_velocity = 0, yaw_velocity = 0;
    DVector last_position{};
    std::uint64_t contacts = 0;

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
