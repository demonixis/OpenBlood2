#pragma once
#include "Projectile.h"
#include "engine/audio.h"
// Selected original GameProjectiles.cpp Napalm primary projectile. See cityhubc_steamtunnels_contracts.md.
// Primary: a flying sprite that explodes (EXP_NAPALM_PRIMARY).
class CNapalmProjectile : public CProjectile
{
  public:
    CNapalmProjectile()
    {
        filename = "Sprites\\napalmprime.spr";
        skin = nullptr;
        damage_type = OpenBlood2::DamageType::Explode;
    }
    ~CNapalmProjectile() override;
    OpenBlood2::SoundId flight_sound = 0;
    OpenBlood2::SceneExplosionReport effect;
    std::string smoke_error = "OBJFX_SMOKETRAIL_2 unavailable";
    std::string light_error = "AddLight(100, 1, .75, 0) projectile light unavailable", sound_error;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
