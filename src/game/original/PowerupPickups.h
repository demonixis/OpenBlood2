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
