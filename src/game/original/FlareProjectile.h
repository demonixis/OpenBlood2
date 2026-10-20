#pragma once
#include "Projectile.h"
// Original GameProjectiles.cpp Flare family. No invented substitute effects.
class CFlareProjectile final : public CProjectile
{
  public:
    CFlareProjectile();
    ~CFlareProjectile() override;
    bool AdvanceFlight(double) override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    DVector color{}, offset{};
    float track_start = 0, last_damage_time = 0;
    bool tracking = false, color_ready = false;
    OpenBlood2::SoundId sound = 0;
    std::string sound_error;
    unsigned ticks = 0;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    void StopSound();
};
class CFlareAltProjectile final : public CProjectile
{
  public:
    CFlareAltProjectile();
    DVector color{}, cached_velocity{};
    bool color_ready = false, get_velocity = true;
    unsigned children = 0;
    DDWORD explosion_type = 15;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
class CFlareBurstProjectile final : public CProjectile
{
  public:
    CFlareBurstProjectile();
    DVector color{};
    bool color_ready = false;
    bool SetColor(DVector);

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    std::string LoadBlock(const ProjectileBlock&) override;
};
