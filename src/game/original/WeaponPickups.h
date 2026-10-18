#pragma once
#include "PickupObject.h"
// Original single-player WeaponPickup/BerettaPU and non-bouncing drop spawn.
class WeaponPickup : public PickupObject
{
  public:
    WeaponPickup()
    {
        pickup_sound = "sounds\\powerups\\weapon1.wav";
        bounce = true;
    }

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class BerettaPU : public WeaponPickup
{
  public:
    BerettaPU();
};
class SubMachineGunPU : public WeaponPickup
{
  public:
    SubMachineGunPU();
};
class AssaultRiflePU : public WeaponPickup
{
  public:
    AssaultRiflePU();
};
class FlareGunPU : public WeaponPickup
{
  public:
    FlareGunPU();
};
class TeslaCannonPU : public WeaponPickup
{
  public:
    TeslaCannonPU();
};
// The sniper rifle weapon is not integrated: the player inventory refuses the
// offer and the pickup stays in the world (see fanatic_prophet_contracts.md).
class SniperRiflePU : public WeaponPickup
{
  public:
    SniperRiflePU();
};
const char* WeaponPickupClass(DDWORD type);
HOBJECT SpawnWeaponPickup(DDWORD type, HOBJECT source, DVector, DRotation, bool include_ammo);
