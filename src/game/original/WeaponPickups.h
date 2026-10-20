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
class VoodooDollPU : public WeaponPickup
{
  public:
    VoodooDollPU();
};
class ShotgunPU : public WeaponPickup
{
  public:
    ShotgunPU();
};
class LifeLeechPU final : public WeaponPickup
{
  public:
    LifeLeechPU();
};
class SingularityPU : public WeaponPickup
{
  public:
    SingularityPU();
};
// Original sniper pickup; its weapon/zoom now use the shared inventory path.
class SniperRiflePU : public WeaponPickup
{
  public:
    SniperRiflePU();
};
const char* WeaponPickupClass(DDWORD type);
HOBJECT SpawnWeaponPickup(DDWORD type, HOBJECT source, DVector, DRotation, bool include_ammo);

class BugSprayPU : public WeaponPickup
{
  public:
    BugSprayPU();
};

class HowitzerPU final : public WeaponPickup
{
  public:
    HowitzerPU();
};

class NapalmCannonPU final : public WeaponPickup
{
  public:
    NapalmCannonPU();
};

class DeathRayPU final : public WeaponPickup
{
  public:
    DeathRayPU();
};

class MiniGunPU final : public WeaponPickup
{
  public:
    MiniGunPU();
};

class OrbPU final : public WeaponPickup
{
  public:
    OrbPU();
};
