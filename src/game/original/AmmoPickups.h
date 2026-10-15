#pragma once
#include "PickupObject.h"
#include "dlink.h"
class AmmoPickup : public PickupObject
{
  public:
    AmmoPickup();
    ~AmmoPickup() override;
    static DLink m_PickupHead;
    static DDWORD m_dwNumPickups;
    DLink m_Link{};

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class BulletAmmoPU : public AmmoPickup
{
  public:
    BulletAmmoPU();
};
class ShellAmmoPU : public AmmoPickup
{
  public:
    ShellAmmoPU();
};
class BMGAmmoPU : public AmmoPickup
{
  public:
    BMGAmmoPU();
};
class FlareAmmoPU : public AmmoPickup
{
  public:
    FlareAmmoPU();
};
class DieBugDieAmmoPU : public AmmoPickup
{
  public:
    DieBugDieAmmoPU();
};
class HowitzerAmmoPU : public AmmoPickup
{
  public:
    HowitzerAmmoPU();
};
class FuelAmmoPU : public AmmoPickup
{
  public:
    FuelAmmoPU();
};
class BatteryAmmoPU : public AmmoPickup
{
  public:
    BatteryAmmoPU();
};
