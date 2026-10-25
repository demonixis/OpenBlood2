#pragma once
#include "Projectile.h"
#include "engine/audio.h"

// Original GameProjectiles.cpp Life Leech primary and alternate callbacks.
class CLeechPrimeProjectile final : public CProjectile
{
  public:
    CLeechPrimeProjectile();
    ~CLeechPrimeProjectile() override;
    DVector color{1, 1, 0}, light_position{};
    float curve = 0, amplitude = 0, trail_scale = 1;
    OpenBlood2::SoundId flight_sound = 0;
    std::uint64_t trail = 0;
    OpenBlood2::SceneExplosionReport effect;
    std::string sound_error, light_error;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    std::string LoadBlock(const ProjectileBlock&) override;
};

class CLeechAltProjectile final : public CProjectile
{
  public:
    CLeechAltProjectile();
    bool AdvanceFlight(double) override
    {
        return !failed;
    }
    std::uint64_t pushes = 0;
    OpenBlood2::SceneExplosionReport effect;

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
