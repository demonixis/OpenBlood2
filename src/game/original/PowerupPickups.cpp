#include "PowerupPickups.h"
HealthPU::HealthPU()
{
    value = {OpenBlood2::PickupKind::Powerup, 1, 25};
    file = "Health_pu";
    display_name = "Life Essence";
    resource_id = 309;
    pickup_sound = "sounds\\powerups\\health1a.wav";
}
MegaHealthPU::MegaHealthPU()
{
    value = {OpenBlood2::PickupKind::Powerup, 2, 100};
    file = "MegaHealth_pu";
    display_name = "Life Seed";
    resource_id = 310;
    pickup_sound = "sounds\\powerups\\health2.wav";
}
WardPU::WardPU()
{
    value = {OpenBlood2::PickupKind::Powerup, 3, 25};
    file = "Ward_pu";
    display_name = "Ward";
    resource_id = 311;
    pickup_sound = "sounds\\powerups\\armour1.wav";
}
NecroWardPU::NecroWardPU()
{
    value = {OpenBlood2::PickupKind::Powerup, 4, 100};
    file = "NecroWard_pu";
    display_name = "NecroWard";
    resource_id = 312;
    pickup_sound = "sounds\\powerups\\powerup3.wav";
}
StealthPU::StealthPU()
{
    value = {OpenBlood2::PickupKind::Powerup, 6, 30}; // POWERUP_STEALTH, 30 seconds.
    file = "Stealth_pu";
    display_name = "Stealth";
    resource_id = 314; // IDS_POWERUP_STEALTH; default powerup4.wav pickup sound.
}
