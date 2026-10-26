#include "WeaponPickups.h"
#include "WeaponDefs.h"
#include "game/game_settings.h"
BerettaPU::BerettaPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_BERETTA, 12};
    file = "Beretta_pu";
    display_name = "Beretta";
    resource_id = 350;
}
SubMachineGunPU::SubMachineGunPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_SUBMACHINEGUN, 20};
    file = "Mac10_pu";
    display_name = "Sub-machine Gun";
    resource_id = IDS_WEAPON_MAC10;
}
ShotgunPU::ShotgunPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_SHOTGUN, 8};
    file = "Shotgun_pu";
    display_name = "Sawed-off Shotgun";
    resource_id = IDS_WEAPON_SHOTGUN;
}
OrbPU::OrbPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_ORB, 0};
    file = "Orb_pu";
    display_name = "The Orb";
    resource_id = IDS_WEAPON_ORB;
}
LifeLeechPU::LifeLeechPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_LIFELEECH, 0};
    file = "Lifeleech_pu";
    display_name = "Life Leech";
    resource_id = IDS_WEAPON_LIFELEECH;
}
SingularityPU::SingularityPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_SINGULARITY, 100};
    file = "Singularity_pu";
    display_name = "Singularity Generator";
    resource_id = IDS_WEAPON_SINGULARITY;
}
TeslaCannonPU::TeslaCannonPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_TESLACANNON, 40};
    file = "TeslaCannon_pu";
    display_name = "Tesla Cannon";
    resource_id = IDS_WEAPON_TESLA;
}
VoodooDollPU::VoodooDollPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_VOODOO, 0};
    file = "Voodoo_pu";
    display_name = "Voodoo Doll";
    resource_id = IDS_WEAPON_VOODOO;
}
AssaultRiflePU::AssaultRiflePU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_ASSAULTRIFLE, 50};
    file = "m16_pu";
    display_name = "Assault Rifle";
    resource_id = IDS_WEAPON_M16;
}
FlareGunPU::FlareGunPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_FLAREGUN, 8};
    file = "FlarePistol_pu";
    display_name = "Flare Pistol";
    resource_id = IDS_WEAPON_FLAREGUN;
}
SniperRiflePU::SniperRiflePU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_SNIPERRIFLE, 10};
    file = "Sniperrifle_pu";
    display_name = "Sniper Rifle";
    resource_id = IDS_WEAPON_SNIPER;
}
const char* WeaponPickupClass(DDWORD type)
{
    switch (type)
    {
        case WEAP_ORB:
            return "OrbPU";
        case WEAP_LIFELEECH:
            return "LifeLeechPU";
        case WEAP_MINIGUN:
            return "MiniGunPU";
        case WEAP_HOWITZER:
            return "HowitzerPU";
        case WEAP_NAPALMCANNON:
            return "NapalmCannonPU";
        case WEAP_DEATHRAY:
            return "DeathRayPU";
        case WEAP_BERETTA:
            return "BerettaPU";
        case WEAP_SUBMACHINEGUN:
            return "SubMachineGunPU";
        case WEAP_ASSAULTRIFLE:
            return "AssaultRiflePU";
        case WEAP_FLAREGUN:
            return "FlareGunPU";
        case WEAP_TESLACANNON:
            return "TeslaCannonPU";
        case WEAP_VOODOO:
            return "VoodooDollPU";
        case WEAP_SHOTGUN:
            return "ShotgunPU";
        case WEAP_SINGULARITY:
            return "SingularityPU";
        case WEAP_BUGSPRAY:
            return "BugSprayPU";
        case WEAP_SNIPERRIFLE:
            return "SniperRiflePU";
        default:
            return nullptr;
    }
}
DDWORD WeaponPickup::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto result = PickupObject::EngineMessageFn(message, data, reason);
    if (message == MID_INITIALUPDATE)
    {
        if (g_pServerDE->IsMultiplayerGame())
            g_pServerDE->RemoveObject(m_hObject);
        else
            g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 7));
    }
    return result;
}
HOBJECT SpawnWeaponPickup(DDWORD type, HOBJECT source, DVector position, DRotation rotation, bool include_ammo)
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(source) || !WeaponPickupClass(type))
        return nullptr;
    ObjectCreateStruct state;
    state.m_Pos = position;
    state.m_Rotation = rotation;
    state.m_Flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY;
    SAFE_STRCPY(state.m_Name, WeaponPickupClass(type));
    auto* pickup = dynamic_cast<WeaponPickup*>(server->CreateObject(server->GetClass(WeaponPickupClass(type)), &state));
    if (!pickup)
        return nullptr;
    if (!server->IsObjectAlive(pickup->m_hObject) || !pickup->error.empty())
    {
        server->RemoveObject(pickup->m_hObject);
        return nullptr;
    }
    // Source SetBounce(false), SpawnItem: keep the supplied position, no respawn.
    if (!include_ammo)
        pickup->value.amount = 0;
    return pickup->m_hObject;
}

BugSprayPU::BugSprayPU()
{
    value = {OpenBlood2::PickupKind::Weapon, 10, 20};
    file = "BugSpray_pu";
    display_name = "Bug Spray Canister";
    resource_id = 357;
}

HowitzerPU::HowitzerPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_HOWITZER, 5};
    file = "Howitzer_pu";
    display_name = "Howitzer";
    resource_id = IDS_WEAPON_HOWITZER;
}

NapalmCannonPU::NapalmCannonPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_NAPALMCANNON, 5};
    file = "NapalmCannon_pu";
    display_name = "Napalm Cannon";
    resource_id = IDS_WEAPON_NAPALM;
}

DeathRayPU::DeathRayPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_DEATHRAY, 50};
    file = "DeathRay_pu";
    display_name = "Cabalco Death Ray";
    resource_id = IDS_WEAPON_DEATHRAY;
}

MiniGunPU::MiniGunPU()
{
    value = {OpenBlood2::PickupKind::Weapon, WEAP_MINIGUN, 100};
    file = "Minigun_pu";
    display_name = "Vulcan Cannon";
    resource_id = IDS_WEAPON_MINIGUN;
}
