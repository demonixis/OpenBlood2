#pragma once
#include "Projectile.h"
// Original GameProjectiles.cpp CSingularityProjectile; see singularity_contracts.md.
// A still, zero-scale grenade model: its first update arms it where its forward ray
// stops; then for five seconds it pulls, damages and swallows attractable models.
class CSingularityProjectile final : public CProjectile
{
  public:
    CSingularityProjectile();
    static constexpr float attract_radius = 600, kill_radius = 50, attract_time = 5; // SINGULARITY_ATTRACT_*.
    bool armed = false;
    float arm_time = 0; // Source m_fStartTime, set when armed.
    DVector ray_end{}, ray_normal{};
    OpenBlood2::SegmentResult ray = OpenBlood2::SegmentResult::Unavailable;
    bool skipped_shooter = false;
    std::uint64_t pulls = 0, swallowed = 0, damage_messages = 0, object_fx_unavailable = 0;
    OpenBlood2::SceneExplosionReport effect;
    bool AdvanceFlight(double) override
    {
        return !failed;
    } // Zero projectile velocity; the source never flies it.

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;

  private:
    void Damage(HOBJECT, float amount, DVector at);
};
