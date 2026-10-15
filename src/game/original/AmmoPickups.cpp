#include "AmmoPickups.h"
DLink AmmoPickup::m_PickupHead{};
DDWORD AmmoPickup::m_dwNumPickups = 0;
AmmoPickup::AmmoPickup()
{
    if (!m_dwNumPickups)
        dl_TieOff(&m_PickupHead);
    pickup_sound = "sounds\\powerups\\ammo1.wav";
}
AmmoPickup::~AmmoPickup()
{
    if (m_Link.m_pData && m_dwNumPickups)
    {
        dl_Remove(&m_Link);
        --m_dwNumPickups;
    }
}
DDWORD AmmoPickup::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_INITIALUPDATE && !m_Link.m_pData)
    {
        dl_Insert(&m_PickupHead, &m_Link);
        m_Link.m_pData = this;
        ++m_dwNumPickups;
    }
    return PickupObject::EngineMessageFn(message, data, reason);
}
BulletAmmoPU::BulletAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 1, 100};
    file = "BulletAmmo_pu";
    display_name = "Bullets";
    resource_id = 317;
}
ShellAmmoPU::ShellAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 2, 20};
    file = "ShellAmmo_pu";
    display_name = "Shells";
    resource_id = 318;
}
BMGAmmoPU::BMGAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 3, 20};
    file = "BMGAmmo_pu";
    display_name = "BMG Rounds";
    resource_id = 319;
}
FlareAmmoPU::FlareAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 4, 20};
    file = "FlareAmmo_pu";
    display_name = "Flares";
    resource_id = 320;
}
DieBugDieAmmoPU::DieBugDieAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 5, 20};
    file = "DieBugDieAmmo_pu";
    display_name = "DieBugDie";
    resource_id = 321;
}
HowitzerAmmoPU::HowitzerAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 6, 5};
    file = "HowitzerAmmo_pu";
    display_name = "Howizter Shells";
    resource_id = 322;
}
FuelAmmoPU::FuelAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 7, 10};
    file = "FuelAmmo_pu";
    display_name = "FuelTank";
    resource_id = 323;
}
BatteryAmmoPU::BatteryAmmoPU()
{
    value = {OpenBlood2::PickupKind::Ammo, 8, 40};
    file = "BatteryAmmo_pu";
    display_name = "Chemical Batteries";
    resource_id = 324;
}
