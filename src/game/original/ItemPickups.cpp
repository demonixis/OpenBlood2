// Selected ItemPickups.cpp: single-player charged item and MedKitPU constructor.
#include "ItemPickups.h"
#include "game/game_settings.h"
DDWORD ItemPickup::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    // String-spawn bounce and multiplayer scaling/respawn are not implemented.
    if (message == MID_PRECREATE &&
        (reason != PRECREATE_WORLDFILE ||
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
