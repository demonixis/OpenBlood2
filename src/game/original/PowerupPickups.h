#pragma once
#include "PickupObject.h"
class PowerupPickup : public PickupObject
{
  public:
    PowerupPickup()
    {
        rotate_fx = true;
    }
};
class HealthBase : public PowerupPickup
{
};
class ArmorBase : public PowerupPickup
{
};
// Original EnhancementBase: its multiplayer respawn/scaling branch is unported (single player only).
class EnhancementBase : public PowerupPickup
{
};
class AngerPU : public EnhancementBase
{
  public:
    AngerPU();
};
class RevenantPU : public EnhancementBase
{
  public:
    RevenantPU();
};
class StealthPU : public EnhancementBase
{
  public:
    StealthPU();
};
class InvulnerabilityPU : public EnhancementBase
{
  public:
    InvulnerabilityPU();
};
class HealthPU : public HealthBase
{
  public:
    HealthPU();
};
class MegaHealthPU : public HealthBase
{
  public:
    MegaHealthPU();
};
class WardPU : public ArmorBase
{
  public:
    WardPU();
};
class NecroWardPU : public ArmorBase
{
  public:
    NecroWardPU();
};
