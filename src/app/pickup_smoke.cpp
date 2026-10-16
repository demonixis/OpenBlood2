#include "pickup_smoke.h"
#include "game/original/PickupObject.h"
#include "game/original/WeaponPickups.h"
#include "game/original/spawn_object.h"
namespace OpenBlood2
{
    bool prepare_pickup_smoke(GameSession& g, ObjectId& id)
    {
        auto h = SpawnObject("BulletAmmoPU Name smoke_ammo", {0, 10000, 0}, {}, {});
        auto* r = g.objects.get(h);
        if (!r || !r->model || !(r->state.m_Flags & FLAG_TOUCH_NOTIFY))
            return false;
        id = r->id;
        auto second = SpawnObject("WardPU Name inspect_ward", {200, 10000, 0}, {}, {});
        const auto* inspect = g.objects.get(second);
        auto weapon = SpawnWeaponPickup(WEAP_BERETTA, h, {400, 10000, 0}, {}, true);
        return inspect && inspect->model && weapon && g.objects.get(weapon)->model;
    }
    bool collect_pickup_smoke(GameSession& g, ObjectId id)
    {
        auto* player = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(id);
        if (!player || !r)
            return false;
        const auto h = r->handle;
        player->m_InventoryMgr.SetAmmoCount(1, 0);
        auto pos = r->state.m_Pos;
        if (g.server.TeleportObject(player->m_hObject, &pos) != DE_OK ||
            g.server.MoveObject(player->m_hObject, &pos) != DE_OK)
            return false;
        if (g.server.IsObjectAlive(h) || player->m_InventoryMgr.GetAmmoCount(1) != 100 ||
            g.server.pickup_reports.size() != 1)
            return false;
        auto weapon = SpawnWeaponPickup(WEAP_BERETTA, player->m_hObject, {600, 10000, 0}, {}, true);
        const auto* pickup = g.objects.get(weapon);
        if (!pickup || !pickup->model)
            return false;
        pos = pickup->state.m_Pos;
        if (g.server.TeleportObject(player->m_hObject, &pos) != DE_OK ||
            g.server.MoveObject(player->m_hObject, &pos) != DE_OK)
            return false;
        return !g.server.IsObjectAlive(weapon) && player->m_InventoryMgr.CurrentWeapon(true) &&
               g.server.pickup_reports.size() == 2;
    }
} // namespace OpenBlood2
