#pragma once
#include "Projectile.h"
#include "WeaponFX.h"
#include <random>

// Original GameProjectiles.cpp: COrbProjectile and COrbAltProjectile.
class COrbProjectile : public CProjectile
{
  public:
    COrbProjectile();
    ~COrbProjectile() override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    bool StartCamera();
    bool SetCamera(bool);
    HOBJECT tracked = nullptr;
    bool alternate = false, attached = false, create_camera = true, camera_started = false;
    float hit_time = 0, last_damage_time = 0;
    std::uint64_t trail = 0, damage_ticks = 0;
    OpenBlood2::SoundId loop = 0;
    OpenBlood2::SceneExplosionReport effect;
    CWeaponFX blood;
    std::string sound_error, camera_error;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    std::string LoadBlock(const ProjectileBlock&) override;

  private:
    bool Steer();
    bool FollowHead();
    std::minstd_rand cosmetic_random_{73};
};
class COrbAltProjectile final : public COrbProjectile
{
  public:
    COrbAltProjectile()
    {
        alternate = true;
    }
};
