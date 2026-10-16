#include "objectives_smoke.h"
#include "game/original/ExitTrigger.h"
#include "game/original/ObjectivesTrigger.h"
namespace OpenBlood2
{
    bool objective_smoke(GameSession& g)
    {
        auto* player = g.server_shell.Player(g.local_client);
        if (!player)
            return false;
        unsigned found = 0;
        const auto before = g.client_shell.objective_messages;
        for (auto id : g.objects.snapshot())
        {
            auto* r = g.objects.get(id);
            auto* t = r ? dynamic_cast<ObjectivesTrigger*>(r->game.get()) : nullptr;
            if (!t)
                continue;
            ++found;
            // The selected authored objective may already have fired at spawn.
            // A diagnostic rearm lets the smoke verify delivery with UI hidden.
            t->SetActive(true);
            auto text = g.server.CreateString("TRIGGER");
            auto message = g.server.StartMessageToObject(player, r->handle, 1000);
            if (!message)
            {
                g.server.FreeString(text);
                return false;
            }
            const bool sent =
                g.server.WriteToMessageHString(message, text) == DE_OK && g.server.EndMessage(message) == DE_OK;
            g.server.FreeString(text);
            if (!sent || t->last_delivery != DE_OK)
                return false;
        }
        std::string error;
        if (found != 1 || !g.pump_player_messages(error) || g.client_shell.objective_messages != before + 1 ||
            !g.client_shell.objectives_resolved || g.client_shell.objectives.resource != 1 ||
            g.client_shell.objectives.title.empty() || g.client_shell.objectives.text.empty())
            return false;
        const auto old_player = g.client_shell.player;
        const auto old_revision = g.world_revision;
        const auto old_bullets = player->m_InventoryMgr.GetAmmoCount(1);
        const auto received = g.client_shell.world_exit.received;
        const auto blocked = g.client_shell.world_exit.blocked;
        const auto captures = g.server_shell.inventory_capture.captures;
        const auto field_captures = g.server_shell.player_capture.captures;
        const auto damage_captures = g.server_shell.damage_capture.captures;
        unsigned exits = 0;
        for (auto id : g.objects.snapshot())
        {
            auto* record = g.objects.get(id);
            auto* trigger = record ? dynamic_cast<ExitTrigger*>(record->game.get()) : nullptr;
            if (!trigger)
                continue;
            ++exits;
            trigger->SetActive(true); // Labelled diagnostic rearm; source resets remain unchanged.
            // Authored exit2 rejects players and accepts this source relay via
            // ObjectActivate, then waits two seconds in its empty first slot.
            HOBJECT relay = nullptr;
            for (auto candidate_id : g.objects.snapshot())
                if (auto* candidate = dynamic_cast<Trigger*>(g.objects.get(candidate_id)->game.get());
                    candidate && candidate->slots[9].target == record->state.m_Name)
                    relay = candidate->m_hObject;
            if (!relay || trigger->max_delay != 2)
                return false;
            auto text = g.server.CreateString("TRIGGER");
            auto message = g.server.StartMessageToObject(g.server.HandleToObject(relay), record->handle, 1000);
            const bool sent = message && g.server.WriteToMessageHString(message, text) == DE_OK &&
                              g.server.EndMessage(message) == DE_OK;
            g.server.FreeString(text);
            if (!sent)
                return false;
            for (unsigned tick = 0; tick < 119; ++tick)
                if (!g.step(1.0 / 60))
                    return false;
            if (g.client_shell.world_exit.received != received || !g.step(.05) || trigger->last_delivery != DE_OK)
                return false;
        }
        return exits == 1 && g.pump_player_messages(error) && g.client_shell.world_exit.received == received + 1 &&
               g.client_shell.world_exit.blocked == blocked + 1 && g.server_shell.inventory_capture.valid &&
               g.server_shell.inventory_capture.captures == captures + 1 &&
               g.server_shell.inventory_capture.state.ammo[1] == old_bullets && g.server_shell.damage_capture.valid &&
               g.server_shell.damage_capture.captures == damage_captures + 1 &&
               g.server_shell.damage_capture.state.health == player->m_damage.m_fHitPoints &&
               g.server_shell.damage_capture.state.armor == player->m_damage.m_fArmorPoints &&
               g.server_shell.player_capture.valid && g.server_shell.player_capture.captures == field_captures + 1 &&
               g.server_shell.player_capture.state.air == player->m_fAirLevel &&
               g.server_shell.player_capture.state.name == std::optional<std::string>("") &&
               g.client_shell.world_exit.error.find("KeepAliveSave/KeepAliveLoad") != std::string::npos &&
               old_player == g.client_shell.player && old_revision == g.world_revision &&
               old_bullets == player->m_InventoryMgr.GetAmmoCount(1);
    }
} // namespace OpenBlood2
