// Selected ItemPickups.cpp: single-player charged item and MedKitPU constructor.
#include "ItemPickups.h"
#include "game/game_settings.h"
DDWORD ItemPickup::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    // Multiplayer scaling/respawn are not implemented; string creation bounces (PickupObject).
    if (message == MID_PRECREATE &&
        ((reason != PRECREATE_WORLDFILE && reason != PRECREATE_STRINGPROP) ||
         g_pServerDE->GetVarValueFloat(g_pServerDE->GetGameConVar("GAME_GAMETYPE")) != GAMETYPE_SINGLE))
        return 0;
    return PickupObject::EngineMessageFn(message, data, reason);
}
MedKitPU::MedKitPU()
{
    value = {OpenBlood2::PickupKind::Item, 1, 1};
    file = "Medkit_pu";
    display_name = "Medkit";
    resource_id = 302;
}

ProximitiesPU::ProximitiesPU()
{
    value = {OpenBlood2::PickupKind::Item, 7, 1};
    file = "Proximities_pu";
    display_name = "Proximity Bomb";
    resource_id = 306;
}

RemotesPU::RemotesPU()
{
    value = {OpenBlood2::PickupKind::Item, 8, 1};
    file = "Remotes_pu";
    display_name = "Remote Bomb";
    resource_id = 307;
}

FlashlightPU::FlashlightPU()
{
    value = {OpenBlood2::PickupKind::Item, 2, 1};
    file = "Flashlight_pu";
    display_name = "Flashlight";
    resource_id = 301;
}

BinocularsPU::BinocularsPU()
{
    value = {OpenBlood2::PickupKind::Item, 5, 1};
    file = "Binoculars_pu";
    display_name = "Binoculars";
    resource_id = 304;
}
