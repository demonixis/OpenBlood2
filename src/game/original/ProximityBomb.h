#pragma once
#include "Projectile.h"
// Selected original GameProjectiles.cpp CProximityBomb. Shared bounded box flight
// is replacement engine code; arming/detection/delay/contact rules remain original.
class CProximityBomb : public CProjectile
{
  public:
    CProximityBomb();
    bool AdvanceFlight(double) override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    bool armed = false, hit_character = false, play_arm = true, play_detect = true;
    float get_away = .75f, detect_radius = 225, explode_delay = .5f;
    float pitch = 0, yaw = 0, pitch_velocity = 0, yaw_velocity = 0;
    DVector last_position{};
    HOBJECT detected = nullptr; // Historical checked identity, not an owning target.
    std::uint64_t contacts = 0, scans = 0;
    unsigned sounds = 0;
    std::string sound_error;

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    void SaveBlock(ProjectileBlock&) const override;
    std::string LoadBlock(const ProjectileBlock&) override;

  protected:
    virtual void Sound(const char*);
};
