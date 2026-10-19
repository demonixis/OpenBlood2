#pragma once
#include "PickupObject.h"
class ItemPickup : public PickupObject
{
  public:
    ItemPickup()
    {
        pickup_sound = "sounds/powerups/inventory1.wav";
        bounce = true;
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
class BinocularsPU : public ItemPickupCharged
{
  public:
    BinocularsPU();
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
            if (value.type == 7 || value.type == 8 || value.type == 9) // Original inventory bombs.
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

class TheEyePU : public ItemPickupCharged
{
  public:
    TheEyePU();
};

class NightGogglesPU : public ItemPickupCharged
{
  public:
    NightGogglesPU();
};

class TimeBombPU final : public ItemPickupWeapon
{
  public:
    TimeBombPU();
};
