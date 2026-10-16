#include "game/original/ExitTrigger.h"
#include "player_fixture.h"
#include <functional>
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        return true;
    }
    ExitTrigger* create(GameSession& g, Properties properties = {})
    {
        const auto h = g.objects.create(g.objects.find_class("ExitTrigger"), {}, Creation::World, properties);
        return dynamic_cast<ExitTrigger*>(g.server.HandleToObject(h));
    }
    bool send(GameSession& g, HOBJECT target, HOBJECT sender)
    {
        auto text = g.server.CreateString("TRIGGER");
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(sender), target, 1000);
        const bool ok =
            message && g.server.WriteToMessageHString(message, text) == DE_OK && g.server.EndMessage(message) == DE_OK;
        g.server.FreeString(text);
        return ok;
    }
    bool defaults(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* t = create(g);
        CHECK(t && t->start_point == "start" && t->exit.next_world.empty() && t->exit.type == WorldExitType::World);
        CHECK(!t->touch_activate && t->player_activate && !t->ai_activate && !t->object_activate && t->relay_activate);
        CHECK(t->reset_time == 0 && t->max_delay == 0);
        CHECK(g.objects.engine_message(t->m_hObject, MID_TOUCHNOTIFY, g.client_shell.player) && t->attempts == 0);
        auto relay = g.objects.create(g.objects.find_class("Trigger"), {}, Creation::World);
        CHECK(send(g, t->m_hObject, relay) && t->attempts == 1 && t->deliveries == 1 && !t->active);
        std::string error;
        CHECK(g.pump_player_messages(error) && g.client_shell.world_exit.received == 1);
        CHECK(!g.client_shell.world_exit.error.empty() && g.client_shell.world_exit.blocked == 0);
        CHECK(g.client_shell.m_bInWorld && g.server_shell.start_point_name == "start");
        CHECK(send(g, t->m_hObject, relay) && t->attempts == 1); // Source zero reset stays inactive.
        CHECK(create(g, {{"EndEpisode", true}})->exit.type == WorldExitType::World);
        CHECK(create(g, {{"EndEpisode", true}, {"SubWorld", true}})->exit.type == WorldExitType::Subworld);
        CHECK(create(g, {{"EndOfEpisode", true}, {"SubWorld", true}})->exit.type == WorldExitType::Episode);
        CHECK(!create(g, {{"NextWorld", std::string(101, 'x')}}));
        CHECK(!g.objects.create(g.objects.find_class("ExitTrigger"), {}, Creation::Restore));
        return true;
    }
    bool delayed(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* t = create(g, {{"NextWorld", std::string("next")},
                             {"MessageDelay", .2f},
                             {"MessageDelay3", .4f},
                             {"TargetName2", std::string("absent")},
                             {"MessageName2", std::string("unused")}});
        CHECK(t && send(g, t->m_hObject, g.client_shell.player));
        // Immediate slots require target+message; pending delayed slots do not.
        // Each virtual dispatch emits the whole exit, without named dispatch.
        CHECK(t->attempts == 1 && t->deliveries == 1 && t->slots[0].pending && t->slots[2].pending);
        std::string error;
        CHECK(g.pump_player_messages(error) && g.client_shell.world_exit.blocked == 1);
        CHECK(g.step(.1) && t->attempts == 1 && g.step(.11) && t->attempts == 2);
        CHECK(g.client_shell.world_exit.blocked == 2 && !t->active && !t->slots[0].pending && t->slots[2].pending);
        CHECK(g.step(.21) && t->attempts == 3 && g.client_shell.world_exit.blocked == 3 && !t->slots[2].pending);
        CHECK(g.step(.1) && t->attempts == 3 && !t->active);
        return true;
    }
    bool broadcast(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* t = create(g, {{"NextWorld", std::string("next")}, {"StartPointName", std::string("alternate")}});
        auto relay = g.objects.create(g.objects.find_class("Trigger"), {}, Creation::World);
        CHECK(t && relay && send(g, t->m_hObject, relay));
        CHECK(t->last_delivery == DE_NOTFOUND && t->deliveries == 0 && g.server_shell.start_point_name == "alternate");
        CHECK(g.objects.remove(t->m_hObject) && g.objects.finish_frame());
        CHECK(g.server_shell.start_point_name == "alternate");
        auto* empty = create(g, {{"StartPointName", std::string{}}});
        CHECK(empty && send(g, empty->m_hObject, relay) && g.server_shell.start_point_name == "alternate");
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.start_point_name == "alternate"); // Source persists through PreStartWorld.
        CHECK(g.unload_world() && g.server_shell.start_point_name.empty());
        return true;
    }
    bool blocked(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player);
        player->m_damage.m_fHitPoints = 73;
        const auto ids = g.objects.snapshot();
        const auto before_revision = g.world_revision;
        const auto before_path = g.world_path;
        const auto before_client = g.local_client;
        const auto bullets = player->m_InventoryMgr.GetAmmoCount(1);
        WorldMessage load;
        std::string error;
        CHECK(original_world_request("next", 0, LOADTYPE_NEW_LEVEL, 0, load));
        CHECK(!g.load_world(load, error) && error.find("KeepAliveSave/KeepAliveLoad") != std::string::npos);
        CHECK(g.server.SendWorldExit({"next", WorldExitType::Subworld}, "arrival") == DE_OK);
        CHECK(g.client_shell.world_exit.received == 0 && g.pump_player_messages(error));
        const auto& exit = g.client_shell.world_exit;
        CHECK(exit.received == 1 && exit.blocked == 1 && !exit.pending && exit.request.type == WorldExitType::Subworld);
        CHECK(exit.load.bytes == load.bytes && exit.load.id == CMSG_LOADWORLD);
        CHECK(exit.error.find("KeepAliveSave/KeepAliveLoad") != std::string::npos);
        CHECK(g.objects.snapshot() == ids && g.world_revision == before_revision && g.world_path == before_path);
        CHECK(g.local_client == before_client && g.client_shell.player == player->m_hObject);
        CHECK(player->m_damage.m_fHitPoints == 73 && player->m_InventoryMgr.GetAmmoCount(1) == bullets);
        CHECK(!g.client_shell.m_bFirstWorld && g.client_shell.m_bInWorld &&
              g.server_shell.start_point_name == "arrival");
        CHECK(g.step(.016) && g.client_shell.world_exit.blocked == 1);
        return true;
    }
    bool dead(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player);
        player->m_damage.m_bDead = DTRUE; // Diagnostic state; this case isolates HandleExitWorld's first guard.
        CHECK(g.server.SendWorldExit({"next", WorldExitType::Episode}, "dead-arrival") == DE_OK);
        std::string error;
        CHECK(g.pump_player_messages(error));
        CHECK(g.client_shell.world_exit.received == 1 && g.client_shell.world_exit.ignored_dead == 1);
        CHECK(g.client_shell.world_exit.request.next_world.empty() && g.client_shell.world_exit.load.bytes.empty());
        CHECK(g.client_shell.world_exit.blocked == 0 && g.client_shell.world_exit.error.empty());
        CHECK(g.server_shell.start_point_name == "dead-arrival" && g.client_shell.player == player->m_hObject);
        return true;
    }
    bool queue(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        CHECK(g.server.SendWorldExit({std::string(101, 'x')}, "bad") == DE_INVALIDPARAMS);
        CHECK(g.server.SendWorldExit({std::string("a\0b", 3)}, "bad") == DE_INVALIDPARAMS);
        CHECK(g.server.SendWorldExit({"next", static_cast<WorldExitType>(3)}, "bad") == DE_INVALIDPARAMS);
        CHECK(g.server.SendWorldExit({"next"}, std::string(101, 's')) == DE_INVALIDPARAMS);
        CHECK(g.server_shell.start_point_name.empty());
        for (unsigned i = 0; i < 256; ++i)
            CHECK(g.server.SendWorldExit({"next"}, "arrival") == DE_OK);
        CHECK(g.server.SendWorldExit({"next"}, "copied-before-failed-send") == DE_ERROR);
        CHECK(g.server_shell.start_point_name == "copied-before-failed-send");
        std::string error;
        CHECK(g.pump_player_messages(error));
        CHECK(g.client_shell.world_exit.received == 256 && g.client_shell.world_exit.blocked == 256);
        CHECK(g.server.SendToClient(g.local_client, WorldExit{"next", static_cast<WorldExitType>(255)}) ==
              DE_INVALIDPARAMS);
        CHECK(g.client_shell.m_bInWorld && g.client_shell.world_exit.error.find("KeepAliveSave") != std::string::npos);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* t = create(g, {{"NextWorld", std::string("owned-next")}, {"StartPointName", std::string("owned-start")}});
        CHECK(t && send(g, t->m_hObject, g.client_shell.player));
        auto source = t->m_hObject;
        CHECK(g.objects.remove(source) && g.objects.finish_frame());
        std::string error;
        CHECK(g.pump_player_messages(error) && g.client_shell.world_exit.request.next_world == "owned-next");
        CHECK(g.server_shell.start_point_name == "owned-start");
        const auto client = g.local_client;
        CHECK(g.server.SendWorldExit({"stale-next"}, "stale-start") == DE_OK);
        CHECK(g.leave_player() && g.enter_player(error));
        CHECK(g.local_client != client && g.client_shell.world_exit.received == 0);
        CHECK(g.pump_player_messages(error) && g.client_shell.world_exit.received == 0);
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.objects.snapshot().empty());
        CHECK(g.server.client_count() == 0 && g.server_shell.start_point_name.empty());
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        HOBJECT previous = nullptr;
        for (unsigned iteration = 0; iteration < 3; ++iteration)
        {
            CHECK(g.request_world("01_SubwayA", error));
            std::map<EntryState, unsigned> counts;
            for (const auto& entry : g.entries)
                ++counts[entry.state];
            CHECK(counts[EntryState::Created] == 203 && counts[EntryState::Unsupported] == 1 &&
                  counts[EntryState::WorldSound] == 4 && counts[EntryState::EditorOnly] == 372 &&
                  counts[EntryState::Failed] == 0);
            CHECK(g.objects.snapshot().size() == 225 && g.enter_player(error) && g.objects.snapshot().size() == 228);
            unsigned exits = 0;
            for (const auto& entry : g.entries)
            {
                auto* record = g.objects.get(entry.object);
                auto* t = record ? dynamic_cast<ExitTrigger*>(record->game.get()) : nullptr;
                if (!t)
                    continue;
                ++exits;
                CHECK(t->m_hObject != previous);
                previous = t->m_hObject;
                std::cout << "Authored " << record->state.m_Name << " -> " << t->exit.next_world << " / start "
                          << t->start_point << '\n';
                CHECK(t->exit.next_world == "02_cityhub" && t->start_point == "01_egressa");
                CHECK(!t->player_activate && !t->relay_activate && t->object_activate && !t->touch_activate);
                CHECK(t->max_delay == 2 && t->slots[0].delay == 2 && t->slots[0].target.empty());
                CHECK(send(g, t->m_hObject, g.client_shell.player) && t->rejected == 1 && t->activations == 0);
                HOBJECT relay = nullptr;
                for (auto id : g.objects.snapshot())
                    if (auto* candidate = dynamic_cast<Trigger*>(g.objects.get(id)->game.get());
                        candidate && candidate->slots[9].target == record->state.m_Name)
                        relay = candidate->m_hObject;
                CHECK(relay && send(g, t->m_hObject, relay) && t->activations == 1 && t->attempts == 0);
                for (unsigned tick = 0; tick < 119; ++tick)
                    CHECK(g.step(1.0 / 60));
                CHECK(t->deliveries == 0 && g.step(.05) && t->deliveries == 1);
            }
            CHECK(exits == 1);
            CHECK(g.objects.snapshot().size() == 230 && g.objects.attachments().size() == 11);
            CHECK(g.client_shell.world_exit.received == 1 && g.client_shell.world_exit.blocked == 1);
            CHECK(g.world_path == "worlds/01_subwaya.dat" && g.client_shell.m_bInWorld);
            CHECK(!g.run_world(error) && error.find("1 uninstantiated") != std::string::npos);
            CHECK(g.unload_world() && g.server.string_count() == 0 && g.objects.snapshot().empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests{
        {"defaults", defaults}, {"delayed", delayed}, {"broadcast", broadcast}, {"blocked", blocked},
        {"dead", dead},         {"queue", queue},     {"lifetime", lifetime},   {"catalog", catalog}};
    const auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
