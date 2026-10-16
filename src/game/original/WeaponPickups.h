#pragma once
#include "PickupObject.h"
// Original single-player WeaponPickup/BerettaPU and non-bouncing drop spawn.
class WeaponPickup : public PickupObject
{
  public:
    WeaponPickup()
    {
        pickup_sound = "sounds\\powerups\\weapon1.wav";
    }

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class BerettaPU : public WeaponPickup
{
  public:
    BerettaPU();
};
HOBJECT SpawnWeaponPickup(DDWORD type, HOBJECT source, DVector, DRotation, bool include_ammo);
