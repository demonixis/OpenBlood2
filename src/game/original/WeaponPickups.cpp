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
DDWORD WeaponPickup::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    // The source string-spawn path bounces by default; that path is not imported.
    if (message == MID_PRECREATE && reason == PRECREATE_STRINGPROP)
        return 0;
    auto result = PickupObject::EngineMessageFn(message, data, reason);
    if (message == MID_INITIALUPDATE)
    {
        if (g_pServerDE->GetVarValueFloat(g_pServerDE->GetGameConVar("GAME_GAMETYPE")) !=
            static_cast<float>(GAMETYPE_SINGLE))
            g_pServerDE->RemoveObject(m_hObject);
        else
            g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 7));
    }
    return result;
}
HOBJECT SpawnWeaponPickup(DDWORD type, HOBJECT source, DVector position, DRotation rotation, bool include_ammo)
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(source) || (type != WEAP_BERETTA && type != WEAP_SUBMACHINEGUN))
        return nullptr;
    ObjectCreateStruct state;
    state.m_Pos = position;
    state.m_Rotation = rotation;
    state.m_Flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY;
    SAFE_STRCPY(state.m_Name, type == WEAP_BERETTA ? "Dropped Beretta pickup" : "Dropped Mac-10 pickup");
    auto* pickup = dynamic_cast<WeaponPickup*>(
        server->CreateObject(server->GetClass(type == WEAP_BERETTA ? "BerettaPU" : "SubMachineGunPU"), &state));
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
