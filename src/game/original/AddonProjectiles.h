#pragma once
#include "Projectile.h"
// Original GameProjectiles.cpp _ADD_ON projectiles (nightmares_edition_contracts.md). The bounded
// box flight is the engine service shared with CGrenade.

// CRockProjectile: the Gremlin's thrown rock. A gravity model that bounces five times, damages
// characters it touches (scaled by the bounces left), never explodes, fades over its last 1.5 s
// and is removed after 6.5 s.
class CRockProjectile final : public CProjectile
{
  public:
    CRockProjectile();
    bool AdvanceFlight(double seconds) override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    int bounces_left = 5;
    float fade_time = 1.5f;
    float pitch = 0, yaw = 0, pitch_velocity = 0, yaw_velocity = 0;
    DVector last_position{};
    unsigned bounce_sounds = 0, damage_deliveries = 0;
    std::uint64_t contacts = 0;
    HOBJECT last_contact = nullptr; // Inspection: the latest counted contact's object (null: the world).
    std::string sound_error;

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override {} // m_bExplosion is set, but nothing explodes it.
};
