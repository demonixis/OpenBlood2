#include "pickup_smoke.h"
#include "game/original/PickupObject.h"
#include "game/original/WeaponPickups.h"
#include "game/original/spawn_object.h"
#include "movement_controls.h"
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
        auto medkit = g.objects.create(g.objects.find_class("MedKitPU"), {}, Creation::World,
                                       {{"Name", std::string("inspect_medkit")}, {"Pos", DVector{800, 10000, 0}}});
        return inspect && inspect->model && weapon && g.objects.get(weapon)->model && medkit &&
               g.objects.get(medkit)->model;
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
        if (g.server.IsObjectAlive(weapon) || !player->m_InventoryMgr.CurrentWeapon(true) ||
            g.server.pickup_reports.size() != 2)
            return false;
        HOBJECT medkit = nullptr;
        for (auto object : g.objects.snapshot())
            if (auto* record = g.objects.get(object); record->type->name == "MedKitPU")
                medkit = record->handle;
        if (!medkit || !g.objects.engine_message(medkit, MID_TOUCHNOTIFY, player->m_hObject) ||
            !player->m_InventoryMgr.medkit)
            return false;
        player->m_damage.SetHitPoints(25); // Explicit diagnostic damage, no claim of a real encounter.
        MovementControls controls;
        controls.focused = true;
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = 1;
        event.key.scancode = SDL_SCANCODE_H;
        controls.event(event, 1);
        g.client_shell.medkit_down = controls.sample_medkit(false);
        std::string error;
        if (!g.client_shell.UpdateUse(g.server_shell) || !g.pump_player_messages(error))
            return false;
        g.client_shell.medkit_down = false;
        if (!g.client_shell.UpdateUse(g.server_shell))
            return false;
        return player->m_damage.m_fHitPoints == 100 && player->m_InventoryMgr.medkit->count == 25 &&
               g.client_shell.item_sounds == 1 && g.server.audio.get(g.client_shell.item_voice) &&
               g.server.pickup_reports.size() == 3;
    }
} // namespace OpenBlood2
