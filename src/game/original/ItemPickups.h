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
class FlashlightPU : public ItemPickupCharged
{
  public:
    FlashlightPU();
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
        // Original _DEMO removes bomb pickups (deferred) and, for every weapon
        // item, breaks before the multiplayer/USRFLG_GLOW code.
        if (message == MID_INITIALUPDATE && IsDemoBuild())
        {
            if (value.type == 7 || value.type == 8) // INV_PROXIMITY, INV_REMOTE (INV_TIMEBOMB is unported).
                g_pServerDE->RemoveObject(m_hObject);
        }
        else if (message == MID_INITIALUPDATE)
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
