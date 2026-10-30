#include "../brush_fixture.h"
#include "../player_fixture.h"
#include "game/original/VolumeBrush.h"
#include "game/original/trigger_helpers.h"
#include <random>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool enter(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error));
        return true;
    }
    HOBJECT volume(GameSession& g, std::string type, std::string name, Properties extra = {})
    {
        g.world.models.push_back(test_brush::box(name, {-100, -10, -100}, {100, 180, 160}));
        extra["Name"] = name;
        if (type != "Container")
            extra["ShowSurface"] = false;
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, extra);
    }
    bool same(const ClientContainerInfo& a, const ContainerInfo& b, const Objects& objects)
    {
        auto* record = objects.get(b.object);
        return a.object == (record ? record->id : ObjectId{}) && a.code == b.code && a.current.x == b.current.x &&
               a.current.y == b.current.y && a.current.z == b.current.z && a.gravity == b.gravity &&
               a.viscosity == b.viscosity && a.hidden == b.hidden;
    }
    bool parity(GameSession& g, DVector head)
    {
        ContainerState server;
        ClientContainerState client;
        CHECK(g.server.GetContainers(g.client_shell.player, head, server));
        CHECK(g.client_session.collision.environment(g.client_session.movement.owner, g.client_session.movement.body,
                                                     head, client));
        CHECK(client.count == server.count && client.total == server.total &&
              same(client.head, server.head, g.objects));
        for (std::size_t i = 0; i < client.count; ++i)
            CHECK(same(client.body[i], server.body[i], g.objects));
        return true;
    }
    bool membership(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        auto water = volume(g, "Water", "hidden", {{"Hidden", true}, {"Current", DVector{12, 3, -8}}});
        CHECK(water);
        for (auto type : {"Ladder", "Conveyor", "FreeFall", "Water", "Damage"})
            CHECK(volume(g, type, type, {{"Damage", 0.f}}));
        CHECK(g.local_movement.publish_collision());
        auto* player = g.server_shell.Player(g.local_client);
        // Poisoning the cached server membership cannot feed the client.
        player->containers = {};
        player->containers.total = 999;
        int character = -1;
        ClientContainerState state;
        CHECK(g.local_movement.environment(g.server_shell, state, character));
        CHECK(state.total == 6 && state.count == 4 && state.head.hidden && state.head.code == CC_NOTHING);
        CHECK(player->containers.total == 999 && character == player->m_nCharacter);
        auto frozen = g.client_session.collision;
        auto name = g.server.CreateString("hidden"), command = g.server.CreateString("ON");
        SendTriggerMsgToObjects(player, name, command);
        g.server.FreeString(name);
        g.server.FreeString(command);
        CHECK(g.local_movement.environment(g.server_shell, state, character));
        CHECK(!state.head.hidden && state.head.code == CC_WATER);
        ClientContainerState old;
        CHECK(frozen.environment(g.client_session.movement.owner, g.client_session.movement.body,
                                 g.client_session.movement.body.position, old));
        CHECK(old.head.hidden && old.head.code == CC_NOTHING);
        auto* brush = dynamic_cast<VolumeBrush*>(g.objects.get(water)->game.get());
        CHECK(brush);
        const auto shape_copies = g.local_movement.shape_copies;
        brush->info.current = {-50, 2, 10};
        brush->info.gravity = -333;
        brush->info.viscosity = .75f;
        CHECK(g.local_movement.environment(g.server_shell, state, character));
        CHECK(state.head.current.x == -50 && state.head.gravity == -333 && state.head.viscosity == .75f);
        CHECK(old.head.current.x == 12 && g.local_movement.shape_copies == shape_copies);
        auto* record = g.objects.get(g.client_shell.player);
        const auto original = record->state.m_Pos;
        std::mt19937 rng(73117);
        for (unsigned i = 0; i < 300; ++i)
        {
            record->state.m_Pos = {float(int(rng() % 401) - 200), float(rng() % 250), float(int(rng() % 401) - 200)};
            CHECK(g.local_movement.synchronize() && g.local_movement.publish_collision());
            auto head = record->state.m_Pos;
            head.y += player->eye_level;
            CHECK(parity(g, head));
        }
        record->state.m_Pos = original;
        CHECK(g.local_movement.synchronize());
        const auto id = g.objects.get(water)->id;
        g.server.RemoveObject(water);
        CHECK(g.local_movement.environment(g.server_shell, state, character));
        CHECK(state.total == 5 && state.head.code == CC_LADDER && !g.client_session.collision.get(id));
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        ClientCollisionWorld detached;
        ClientObjectId owner;
        ClientMotionState body;
        {
            GameSession g;
            CHECK(enter(g, root));
            auto h = volume(g, "Container", "builtin", {{"ContainerCode", 0}});
            CHECK(h && volume(g, "Water", "water"));
            CHECK(g.local_movement.publish_collision());
            detached = g.client_session.collision;
            owner = g.client_session.movement.owner;
            body = g.client_session.movement.body;
            CHECK(detached.get(g.objects.get(h)->id)->shape.get() != g.objects.get(h)->container().get());
            CHECK(parity(g, body.position));
            CHECK(g.unload_world());
            ClientContainerState cleared;
            CHECK(!g.client_session.collision.environment(owner, body, body.position, cleared));
        }
        ClientContainerState state;
        CHECK(detached.environment(owner, body, body.position, state));
        CHECK(state.total == 2 && state.head.code == CC_NOTHING && state.head.gravity == 0);
        CHECK(state.body[1].code == CC_WATER);
        return true;
    }
    bool passive(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        auto h = volume(g, "Water", "water", {{"Current", DVector{120, 0, 0}}, {"Damage", 10.f}});
        CHECK(h);
        auto* brush = dynamic_cast<VolumeBrush*>(g.objects.get(h)->game.get());
        auto* player = g.server_shell.Player(g.local_client);
        const auto health = player->m_damage.m_fHitPoints;
        const auto air = player->m_fAirLevel;
        const auto calls = brush->physics_calls;
        const auto messages = player->movement_messages;
        const auto position = g.objects.get(player->m_hObject)->state.m_Pos;
        ClientContainerState state;
        int character = 0;
        CHECK(g.local_movement.environment(g.server_shell, state, character));
        for (unsigned i = 0; i < 100; ++i)
            CHECK(g.client_session.collision.environment(g.client_session.movement.owner,
                                                         g.client_session.movement.body, position, state));
        CHECK(player->m_damage.m_fHitPoints == health && player->m_fAirLevel == air);
        CHECK(brush->physics_calls == calls && player->movement_messages == messages);
        CHECK(g.objects.get(player->m_hObject)->state.m_Pos.x == position.x);
        CHECK(g.step(1. / 60));
        CHECK(player->m_fAirLevel == air - float(1. / 60) * (100.f / 15.f));
        CHECK(player->movement_messages - messages == g.client_shell.move_mgr.substeps);
        CHECK(g.client_session.movement.predictions == g.client_session.movement.acknowledgements);
        CHECK(g.client_session.movement.prediction_failures == 0);
        CHECK(g.client_shell.move_mgr.m_bBodyInLiquid && g.client_shell.move_mgr.containers.head.code == CC_WATER);
        return true;
    }
    bool private_worlds(const std::filesystem::path& root)
    {
        for (const auto* world : {"01_SubwayA", "02_CityHub"})
        {
            GameSession g;
            CHECK(g.initialize());
            Config config;
            config.data_dir = root;
            std::string error;
            CHECK(g.resources.mount_game(config, error));
            CHECK(g.request_world(world, error) && g.enter_player(error));
            CHECK(g.local_movement.publish_collision());
            std::size_t volumes = 0;
            for (const auto& object : g.client_session.collision.frame().objects)
                volumes += object.environment.has_value();
            CHECK(volumes > 0);
            for (unsigned tick = 0; tick < 120; ++tick)
            {
                CHECK(g.step(1. / 60));
                CHECK(g.local_movement.synchronize() && g.local_movement.publish_collision());
                auto head = g.client_session.movement.body.position;
                head.y += g.server_shell.Player(g.local_client)->eye_level;
                CHECK(parity(g, head));
            }
            // Sample every authored volume, including ones outside the starting room.
            // No gameplay runs while temporarily placing the query body at these points.
            auto* body = g.objects.get(g.client_shell.player);
            const auto position = body->state.m_Pos;
            const auto scene = g.client_session.collision;
            for (const auto& object : scene.frame().objects)
            {
                if (!object.environment)
                    continue;
                body->state.m_Pos = vector_scale(vector_add(object.shape->min, object.shape->max), .5f);
                CHECK(g.local_movement.synchronize());
                CHECK(parity(g, body->state.m_Pos));
            }
            body->state.m_Pos = position;
            CHECK(g.local_movement.synchronize());
            CHECK(g.local_movement.primary_copies == 1);
            CHECK(g.client_session.movement.prediction_failures == 0);
            CHECK(g.client_session.movement.predictions == g.client_session.movement.acknowledgements);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    const std::filesystem::path root = argv[2];
    const bool ok = mode == "membership" ? membership(root)
                    : mode == "lifetime" ? lifetime(root)
                    : mode == "passive"  ? passive(root)
                    : mode == "private"  ? private_worlds(root)
                                         : false;
    return ok ? 0 : 1;
}
