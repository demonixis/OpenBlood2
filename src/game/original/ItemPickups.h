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

class ItemPickupWeapon : public ItemPickup
{
  public:
    ItemPickupWeapon()
    {
        pickup_sound = "sounds/powerups/ammo2.wav";
    }

  protected:
    DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override
    {
        if (message == MID_INITIALUPDATE)
            g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 7));
        return ItemPickup::EngineMessageFn(message, data, reason);
    }
};
class RemotesPU : public ItemPickupWeapon
{
  public:
    RemotesPU();
};
class ProximitiesPU : public ItemPickupWeapon
{
  public:
    ProximitiesPU();
};
