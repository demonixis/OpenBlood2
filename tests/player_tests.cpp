#include "camera_fixture.h"
#include "game/original/SmellHint.h"
#include "game/server_adapter.h"
#include "player_fixture.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>

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
    using test_player::setup;
    bool handles(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(setup(game, root));
        auto client = game.server.connect_client();
        CHECK(client && !game.server.connect_client());
        auto base = game.objects.create(game.objects.find_class("BaseClass"));
        CHECK(base);
        CHECK(game.server.SetClientUserData(client, base) == DE_INVALIDPARAMS);
        CHECK(game.server.SendToClient(client, PlayerInitRequest{}) == DE_INVALIDPARAMS);
        CHECK(game.server.disconnect_client(client));
        auto next = game.server.connect_client();
        CHECK(next && next != client);
        CHECK(game.server.SetClientUserData(client, nullptr) == DE_INVALIDPARAMS);
        CHECK(!game.server.disconnect_client(client));
        CHECK(game.server.disconnect_client(next));
        game.server.set_game_settings(1, 1);
        std::string error;
        CHECK(!game.enter_player(error) && game.server.client_count() == 0);
        return true;
    }
    bool lifecycle(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(setup(game, root));
        std::string error;
        CHECK(game.enter_player(error));
        auto client = game.local_client;
        auto* player = game.server_shell.Player(client);
        CHECK(player && player->initialized && player->attributes_received && player->respawns == 2);
        auto handle = player->m_hObject;
        auto id = game.objects.get(handle)->id;
        auto muzzle = player->m_InventoryMgr.m_hMuzzleFlash;
        CHECK(muzzle && game.objects.get(muzzle));
        CHECK(player->GetClient() == client && player->m_InventoryMgr.m_hClient == client);
        CHECK(player->m_damage.m_fHitPoints == 100 && player->m_InventoryMgr.GetAmmoCount(1) == 50);
        CHECK(player->m_damage.m_fMaxHitPoints == 200 && player->m_fMoveVel == 280);
        CHECK(CDestructable::m_dwNumDestructables == 1 && SmellHint::m_dwNumSmells == 1);
        CHECK(game.client_shell.m_bInWorld && !game.client_shell.m_bFirstWorld);
        CHECK(game.client_shell.position.x == 32 && game.client_shell.position.z == 64);
        CHECK(std::abs(game.client_shell.position.y - 41.001f) < .01f && game.client_shell.rotation.yaw == .75f);
        CHECK(!game.enter_player(error));
        CHECK(game.step(.016));
        CHECK(player->updates == 1 && !player->m_bFirstUpdate);
        CHECK(game.client_shell.stats.health == 100 && game.client_shell.stats.bullets == 50);
        CHECK(game.client_shell.camera_position.y > game.client_shell.position.y);
        auto old_model = game.objects.get(handle)->model;
        CHECK(game.server.SetModelFilenames(handle, "missing.abc", "skin.dtx") == DE_ERROR);
        CHECK(game.objects.get(handle)->model == old_model);
        player->m_damage.m_bDead = DTRUE;
        CHECK(!player->Respawn());
        player->m_damage.m_bDead = DFALSE;
        CHECK(player->Respawn());
        CHECK(game.pump_player_messages(error));
        CHECK(game.leave_player());
        CHECK(!game.objects.get(id) && !game.client_shell.player && game.server.client_count() == 0);
        CHECK(!game.objects.get(muzzle) &&
              game.objects.snapshot().size() == 2); // Start point + world-owned scent only.
        CHECK(CDestructable::m_dwNumDestructables == 0);
        CHECK(game.enter_player(error));
        CHECK(game.local_client != client && game.client_shell.player != handle);
        CHECK(game.unload_world());
        CHECK(game.server.string_count() == 0 && game.server.list_count() == 0 && game.server.message_count() == 0);
        CHECK(game.objects.snapshot().empty() && SmellHint::m_dwNumSmells == 0 &&
              CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool smell(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(p->m_hObject);
        CHECK(p->smell_drops == 1 && SmellHint::m_dwNumSmells == 1);
        HOBJECT tail;
        CHECK(SmellHint::LastLive(tail) && tail == p->last_smell);
        const auto first = tail;
        // Isolate the original update block from movement/respawn placement.
        r->state.m_Pos = g.objects.get(first)->state.m_Pos;
        r->state.m_Pos.y += 10;                              // Full 3D distance, not horizontal distance.
        CHECK(g.objects.advance(.5) && p->smell_drops == 1); // strict boundary
        CHECK(g.objects.advance(.002) && p->smell_drops == 2 && p->smell_error.empty());
        CHECK(std::abs(p->smell_distance - 10) < .001f);
        const auto time = p->m_fSmellTime;
        CHECK(g.objects.advance(.501) && p->smell_drops == 2 && p->m_fSmellTime == time);
        ObjectCreateStruct other;
        other.m_Pos = r->state.m_Pos;
        other.m_Pos.x += 20;
        auto hint = g.objects.create(g.objects.find_class("SmellHint"), other);
        CHECK(hint && p->UpdateSmellTrail() && p->smell_drops == 3); // global tail, not own last hint
        const auto pending = p->last_smell;
        CHECK(g.objects.remove(pending) && !g.server.IsObjectAlive(pending));
        CHECK(SmellHint::LastLive(tail) && tail == hint); // pending last entry skipped
        p->m_damage.m_bDead = true;
        CHECK(g.objects.advance(.501) && p->smell_drops == 3);
        p->m_damage.m_bDead = false;
        p->m_bSpectatorMode = true; // Guard storage only; no spectator implementation claimed.
        CHECK(p->UpdateSmellTrail() && p->smell_drops == 3);
        p->m_bSpectatorMode = false;
        CHECK(p->UpdateSmellTrail() && p->smell_drops == 4);
        p->m_fSmellDelay = std::numeric_limits<float>::quiet_NaN();
        CHECK(!p->UpdateSmellTrail() && !p->smell_error.empty());
        p->m_fSmellDelay = .5f;
        auto last = p->last_smell;
        CHECK(g.leave_player() && g.server.IsObjectAlive(last)); // world owns trail
        CHECK(g.objects.advance(19) && g.objects.finish_frame());
        CHECK(!g.objects.get(first) && g.objects.get(last));
        CHECK(g.objects.advance(1.01) && g.objects.finish_frame());
        CHECK(SmellHint::m_dwNumSmells == 0 && SmellHint::LastLive(tail) && !tail);
        CHECK(g.enter_player(error));
        CHECK(g.client_shell.player && !g.objects.get(last));
        CHECK(g.unload_world() && SmellHint::m_dwNumSmells == 0);
        return true;
    }
    bool smell_private(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            CHECK(test_camera::complete_subway_intro(g));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p && p->smell_checks > 700 && p->smell_error.empty());
            const auto before = p->smell_drops;
            const auto position = g.client_shell.position;
            g.client_shell.movement_input.forward = true;
            for (unsigned frame = 0; frame < 120; ++frame)
                CHECK(g.step(1.0 / 60));
            g.client_shell.movement_input = {};
            CHECK(p->smell_drops > before && p->smell_error.empty());
            const auto current = g.client_shell.position;
            CHECK(std::hypot(current.x - position.x, current.z - position.z) >= 10);
            HOBJECT last;
            CHECK(SmellHint::LastLive(last) && last == p->last_smell && g.objects.get(last));
            std::cout << "SubwayA smell cycle " << cycle << ": original intro, forward movement, " << p->smell_drops
                      << " drops, " << p->smell_checks << " checks, " << SmellHint::m_dwNumSmells << " live hints\n";
            CHECK(g.unload_world() && SmellHint::m_dwNumSmells == 0 && !g.objects.get(last));
        }
        return true;
    }
    bool failure_rollback(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(setup(game, root, false));
        std::string error;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            CHECK(!game.enter_player(error));
            CHECK(!error.empty());
            CHECK(game.objects.snapshot().empty());
            CHECK(!game.local_client && !game.client_shell.m_bInWorld && game.server.client_count() == 0);
            CHECK(game.server.string_count() == 0 && game.server.list_count() == 0);
            CHECK(CDestructable::m_dwNumDestructables == 0 && SmellHint::m_dwNumSmells == 0);
        }
        CHECK(game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                  {{"Name", std::string("fallback")}, {"Pos", DVector{0, 100, 0}}}));
        CHECK(game.enter_player(error));
        CHECK(game.client_shell.position.x == 0); // Last non-MP fallback.
        auto player = game.client_shell.player;
        CHECK(game.objects.remove(player));
        CHECK(game.step(.016));
        CHECK(!game.local_client && !game.client_shell.m_bInWorld && !game.objects.get(player));
        CHECK(CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool messages(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(setup(game, root));
        std::string error;
        CHECK(game.enter_player(error));
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(player);
        CHECK(!game.server_shell.OnMessage(game.local_client,
                                           PlayerInit{})); // Duplicate attributes cannot reset inventory/leak strings.
        CHECK(!game.client_shell.OnMessage(PlayerInitRequest{}, game.server_shell));
        CHECK(game.server.SendToClient(game.local_client, PlayerPosition{{std::numeric_limits<float>::quiet_NaN(), 0,
                                                                          0}}) == DE_INVALIDPARAMS);
        const auto strings = game.server.string_count();
        player->m_nAttribStrength = 255;
        player->m_nAttribSpeed = 255;
        player->ValidateAttributes();
        CHECK(player->m_nAttribStrength == 3 && player->m_nAttribSpeed == 3);
        for (unsigned i = 0; i < 256; ++i)
            CHECK(game.server.SendToClient(game.local_client, PlayerPosition{{float(i), 42, 3}}) == DE_OK);
        CHECK(game.server.SendToClient(game.local_client, PlayerPosition{}) == DE_ERROR);
        CHECK(game.pump_player_messages(error));
        CHECK(game.client_shell.position.x == 255);
        CHECK(game.server.string_count() == strings);
        CHECK(game.leave_player());
        CHECK(!game.client_shell.OnMessage(PlayerPosition{}, game.server_shell));
        return true;
    }
    GameSession* callback_game = nullptr;
    bool rejected_reentry = false, remove_on_trigger = false;
    struct EntryTrigger : BaseClass
    {
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read) override
        {
            auto& game = *callback_game;
            std::string error;
            const auto attempts = game.startup.attempts;
            const auto state = game.state;
            rejected_reentry = !game.leave_player() && !game.unload_world() && !game.enter_player(error) &&
                               !game.run_world(error) && !game.step(.01) && !game.pump_player_messages(error) &&
                               game.local_client && game.startup.attempts == attempts && game.state == state;
            if (remove_on_trigger)
                game.objects.remove(game.server.GetClientUserData(game.local_client));
            return BaseClass::ObjectMessageFn(sender, message, read);
        }
    };
    std::unique_ptr<BaseClass> make_trigger()
    {
        return std::make_unique<EntryTrigger>();
    }
    bool callbacks(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(setup(game, root, false));
        callback_game = &game;
        static const ClassInfo type{"EntryTrigger", game.objects.find_class("BaseClass"), make_trigger, {}};
        CHECK(game.objects.register_class(type));
        CHECK(game.objects.create(&type, {}, Creation::World, {{"Name", std::string("entry_trigger")}}));
        CHECK(game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                  {{"Name", std::string("start")},
                                   {"Pos", DVector{0, 100, 0}},
                                   {"TriggerTarget", std::string("entry_trigger")},
                                   {"TriggerMessage", std::string("TRIGGER")}}));
        std::string error;
        CHECK(game.enter_player(error));
        CHECK(rejected_reentry);
        CHECK(game.state == SessionState::Inspection && game.client_shell.m_bInWorld);
        CHECK(game.leave_player());
        const auto count = game.objects.snapshot().size(), strings = game.server.string_count();
        remove_on_trigger = true;
        rejected_reentry = false;
        CHECK(!game.enter_player(error) && rejected_reentry);
        CHECK(game.objects.snapshot().size() == count && game.server.string_count() == strings &&
              game.server.client_count() == 0);
        CHECK(game.unload_world());
        callback_game = nullptr;
        return true;
    }
    bool private_player(const char* root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = path_from_utf8(root);
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            CHECK(game.request_world("01_SubwayA", error));
            CHECK(game.server_shell.paths.size() == 7);
            CHECK(game.enter_player(error));
            auto* player = game.server_shell.Player(game.local_client);
            CHECK(player);
            CHECK(game.objects.get(player->m_hObject)->model->animations.size() == 86);
            CHECK(game.objects.snapshot().size() ==
                  230); // 204 source + 7 door children + 16 NPC muzzle lights + player/muzzle/scent.
            CHECK(game.client_shell.position.x == -32 && game.client_shell.position.z == -3224);
            CHECK(std::abs(game.client_shell.position.y - 170) < .01f);
            for (int frame = 0; frame < 120; ++frame)
                CHECK(game.step(1.0 / 60.0));
            CHECK(player->updates == 120 && game.server_shell.updates == 120 && game.client_shell.updates == 121);
            CHECK(game.client_shell.stats.health == 100 && game.client_shell.stats.bullets == 50);
            CHECK(game.run_world(error) && error.empty() && game.state == SessionState::Ready);
            std::cout << "Cycle " << iteration << ": CPlayerObj at " << game.client_shell.position.x << ","
                      << game.client_shell.position.y << "," << game.client_shell.position.z << "; 120 updates; "
                      << game.client_shell.messages << " client messages; " << game.server_shell.paths.size()
                      << " paths\n";
            CHECK(game.unload_world());
            CHECK(game.objects.snapshot().empty() && game.server.client_count() == 0);
            CHECK(game.server.string_count() == 0 && game.server.list_count() == 0 && game.server.message_count() == 0);
            CHECK(CDestructable::m_dwNumDestructables == 0 && SmellHint::m_dwNumSmells == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string name = argv[1];
    bool ok = false;
    if (name == "handles")
        ok = handles(argv[2]);
    else if (name == "lifecycle")
        ok = lifecycle(argv[2]);
    else if (name == "rollback")
        ok = failure_rollback(argv[2]);
    else if (name == "messages")
        ok = messages(argv[2]);
    else if (name == "callbacks")
        ok = callbacks(argv[2]);
    else if (name == "smell")
        ok = smell(argv[2]);
    else if (name == "smell_private")
        ok = smell_private(argv[2]);
    else if (name == "private")
        ok = private_player(argv[2]);
    return ok ? 0 : 1;
}
