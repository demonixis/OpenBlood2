#pragma once
#include "Projectile.h"
// Original GameProjectiles.cpp CGrenade. The bounded box flight is an engine service.
class CGrenade final : public CProjectile
{
  public:
    CGrenade();
    ~CGrenade() override;
    bool AdvanceFlight(double seconds) override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    int bounces_left = 5;
    float pitch = 0, yaw = 0, pitch_velocity = 0, yaw_velocity = 0;
    DVector last_position{};
    std::uint64_t contacts = 0, trail = 0;
    unsigned bounce_sounds = 0, explosion_passes = 0, damage_deliveries = 0;
    OpenBlood2::SoundId sound = 0;
    std::string sound_error;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;

  private:
    void StopSound();
};
