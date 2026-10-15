#include "trigger_smoke.h"
#include "game/original/Door.h"
#include "game/original/KeyPickup.h"
#include "game/original/Trigger.h"
#include <spdlog/spdlog.h>
namespace OpenBlood2
{
    namespace
    {
        bool command(GameSession& g, HOBJECT to, const char* text)
        {
            auto* player = g.server_shell.Player(g.local_client);
            auto string = g.server.CreateString(text);
            auto message = g.server.StartMessageToObject(player, to, 1000);
            const bool ok = message && g.server.WriteToMessageHString(message, string) == DE_OK &&
                            g.server.EndMessage(message) == DE_OK;
            g.server.FreeString(string);
            return ok;
        }
    } // namespace
    bool prepare_trigger_smoke(GameSession& g, ObjectId& door_id, ObjectId& trigger_id)
    {
        auto* player = g.server_shell.Player(g.local_client);
        if (!player)
            return false;
        for (auto id : g.objects.snapshot())
        {
            auto* r = g.objects.get(id);
            auto* door = dynamic_cast<Door*>(r->game.get());
            auto* child = door ? g.objects.get(door->trigger) : nullptr;
            auto* trigger = child ? dynamic_cast<Trigger*>(child->game.get()) : nullptr;
            if (!trigger || !trigger->active || !trigger->player_activate || trigger->named_activate ||
                trigger->key_name.empty() || door->speed <= 0 || door->move_dist <= 0 || door->open_delay >= .05f)
                continue;
            spdlog::info("Trigger smoke door: {} key {}", r->state.m_Name, trigger->key_name);
            door_id = id;
            trigger_id = child->id;
            if (!command(g, r->handle, "LOCK") || !trigger->locked || !command(g, child->handle, "TRIGGER") ||
                !trigger->locked || door->state != Door::Closed)
            {
                spdlog::error("Trigger smoke lock/denial failed");
                return false;
            }
            // Use LabKey's authored asset paths: the source KeyPickup default
            // key.abc is absent in the installed game (the real asset is key_pu).
            std::vector<std::uint8_t> bytes;
            World source_world;
            std::string error, model, skin;
            if (!g.resources.read("worlds/05_cdm.dat", bytes, error) || !read_world(bytes, source_world, error))
                return false;
            for (const auto& source : source_world.objects)
            {
                if (source.class_name != "KeyPickup")
                    continue;
                Properties props;
                for (const auto& property : source.properties)
                    props.emplace(property.name, property.value);
                if (props.contains("Name") && std::get<std::string>(props.at("Name")) == "LabKey")
                {
                    model = std::get<std::string>(props.at("ModelFile"));
                    skin = std::get<std::string>(props.at("ModelSkin"));
                    break;
                }
            }
            if (model.empty() || skin.empty())
                return false;
            // Controlled fixture: authored assets, actual door key name, removed before baseline counts.
            DVector position;
            g.server.GetObjectPos(player->m_hObject, &position);
            auto h = g.objects.create(g.objects.find_class("KeyPickup"), {}, Creation::String,
                                      {{"Name", std::string("smoke_key")},
                                       {"ObjectName", trigger->key_name},
                                       {"UseCount", std::int32_t(1)},
                                       {"ModelFile", model},
                                       {"ModelSkin", skin},
                                       {"Pos", position}});
            auto* pickup = h ? dynamic_cast<KeyPickup*>(g.objects.get(h)->game.get()) : nullptr;
            if (pickup)
                spdlog::info("Trigger smoke pickup: {}", pickup->error);
            if (!pickup || !pickup->error.empty() || g.server.MoveObject(player->m_hObject, &position) != DE_OK ||
                !pickup->picked_up || player->m_InventoryMgr.keys.size() != 1 || !g.objects.finish_frame())
                return false;
            return true;
        }
        return false;
    }
    bool activate_trigger_smoke(GameSession& g, ObjectId trigger_id)
    {
        auto* r = g.objects.get(trigger_id);
        auto* t = r && r->life == Life::Alive ? dynamic_cast<Trigger*>(r->game.get()) : nullptr;
        if (!t || !command(g, r->handle, "TRIGGER"))
            return false;
        auto* player = g.server_shell.Player(g.local_client);
        return t->activations == 1 && t->key_queries == 2 && !t->locked && !t->active && player &&
               player->m_InventoryMgr.keys.empty();
    }
} // namespace OpenBlood2
