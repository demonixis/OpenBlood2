#include "interaction_smoke.h"
#include "game/original/RotatingDoor.h"
#include "game/original/Trigger.h"
#include <spdlog/spdlog.h>
namespace OpenBlood2
{
    bool prepare_interaction_smoke(GameSession& g, ObjectId& id)
    {
        auto* player = g.server_shell.Player(g.local_client);
        if (!player)
            return false;
        // Explicit synthetic hinge above the retail level; real Caleb assets and
        // original player/trigger/key/door code. Retail geometry has separate tests.
        WorldModel model;
        model.name = "interaction_smoke_hinge";
        model.translation = {0, 10050, 0};
        const std::array<DVector, 8> points{{{0, 10000, -2},
                                             {40, 10000, -2},
                                             {40, 10100, -2},
                                             {0, 10100, -2},
                                             {0, 10000, 2},
                                             {40, 10000, 2},
                                             {40, 10100, 2},
                                             {0, 10100, 2}}};
        const std::array<std::array<unsigned, 4>, 6> faces{
            {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 3, 7, 4}, {1, 5, 6, 2}, {0, 4, 5, 1}, {3, 2, 6, 7}}};
        const std::array<DVector, 6> normals{{{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}}};
        model.points.assign(points.begin(), points.end());
        for (unsigned i = 0; i < 6; ++i)
        {
            model.planes.push_back({normals[i], vector_dot(normals[i], points[faces[i][0]])});
            model.surfaces.push_back({});
            model.surfaces.back().plane = i;
            Polygon polygon;
            polygon.surface = i;
            for (auto index : faces[i])
                polygon.points.push_back(static_cast<std::uint16_t>(index));
            model.polygons.push_back(std::move(polygon));
        }
        g.world.models.push_back(std::move(model));
        ++g.world_revision; // Invalidate diagnostic GPU geometry after fixture insertion.
        auto h = g.objects.create(g.objects.find_class("RotatingDoor"), {}, Creation::World,
                                  {{"Name", std::string("interaction_smoke_hinge")},
                                   {"Speed", 90.0f},
                                   {"RotationPoint", DVector{0, 10050, 0}},
                                   {"RotationAngles", DVector{0, 90, 0}},
                                   {"TriggerClosed", true},
                                   {"SelfTrigger", true},
                                   {"TriggerDims", DVector{45, 100, 4}},
                                   {"Locked", true},
                                   {"UnlockKeyName", std::string("smoke_key")}});
        if (!h)
            return false;
        id = g.objects.get(h)->id;
        DVector position{20, 10050, -60};
        if (g.server.TeleportObject(player->m_hObject, &position) != DE_OK || !player->TeleportClientToServerPos())
            return false;
        g.client_shell.rotation = {};
        if (!g.server_shell.OnMessage(g.local_client, PlayerUse{}))
            return false;
        auto* door = dynamic_cast<RotatingDoor*>(g.objects.get(h)->game.get());
        auto* trigger = dynamic_cast<Trigger*>(g.objects.get(door->trigger)->game.get());
        if (player->use_hit.object != door->trigger || !trigger->locked || door->state != Door::Closed)
            return false;
        return player->m_InventoryMgr.AddKey({"smoke_key", "Smoke key", "key.pcx", "key_h.pcx", 1}) ==
               InventoryResult::Ok;
    }
    bool check_interaction_smoke(GameSession& g, ObjectId id)
    {
        auto* record = g.objects.get(id);
        auto* door = record ? dynamic_cast<RotatingDoor*>(record->game.get()) : nullptr;
        auto* player = g.server_shell.Player(g.local_client);
        const bool ok = door && door->angles.y > 0 && player && player->use_commands == 2 &&
                        player->use_messages == 2 && player->m_InventoryMgr.keys.empty() && record->mover_moves > 0;
        if (!ok)
            spdlog::error("USE/rotation smoke: door angle {}, USE commands {}, messages {}", door ? door->angles.y : -1,
                          player ? player->use_commands : 0, player ? player->use_messages : 0);
        return ok;
    }
} // namespace OpenBlood2
