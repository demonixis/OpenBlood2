#pragma once
#include "PickupObject.h"
class ItemPickup : public PickupObject
{
  public:
    ItemPickup()
    {
        pickup_sound = "sounds/powerups/inventory1.wav";
    }

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class ItemPickupCharged : public ItemPickup
{
};
class MedKitPU : public ItemPickupCharged
{
  public:
    MedKitPU();
};
