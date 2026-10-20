#include "app/model_scene.h"
#include "game/original/AI_Mgr.h"
#include "player_fixture.h"
#include <algorithm>
#include <functional>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << "\n";                                               \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool roundtrip(const char* root)
    {
        GameSession g;
        std::string e;
        CHECK(test_player::setup(g, root) && g.enter_player(e));
        auto* p = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(p->m_hObject);
        CHECK(g.server.SetObjectColor(p->m_hObject, {.2f, .4f, .6f}, .5f));
        auto old = p->m_hObject;
        auto old_client = g.local_client;
        // Deliberately non-default state; source Load and first Update must not Respawn/ValidateAttributes.
        p->m_damage.SetHitPoints(37);
        p->m_damage.SetArmorPoints(23);
        p->m_damage.m_fMaxHitPoints = 311;
        p->m_damage.m_fResistance = .42f;
        p->m_nAttribStrength = 6;
        p->m_nAttribSpeed = 6;
        p->m_nAttribResistance = 6;
        p->m_nAttribMagic = 6;
        p->m_InventoryMgr.SetAmmoCount(1, 1234);
        p->m_fAirLevel = 42;
        p->m_fYaw = 1.25f;
        p->m_fPitch = -.25f;
        p->crouched = p->crouch_requested = true;
        p->eye_level = 12;
        p->move_state = CPlayerObj::MoveState::Crouch;
        r->state.m_Pos = {100, 21, 200};
        r->dimensions = {12, 20, 12};
        r->state.m_Scale = {1.1f, 1.2f, .9f};
        r->state.m_Rotation = rotation_from_euler({0, 1.25f, 0});
        r->animation.time = 17;
        r->animation.first = false;
        r->animation.looping = false;
        SAFE_STRCPY(r->state.m_Name, "saved-owner");
        r->user_flags = 0x1234;
        r->blocking_priority = 40;
        r->velocity = {1, 2, 3};
        r->acceleration = {4, 5, 6};
        r->due = g.objects.time() + .4;
        PlayerRestoreState saved;
        CHECK(g.capture_player(saved, e) && saved.inventory.ammo[1] == 1234 && saved.player.strength == 6);
        CHECK(!g.restore_player(saved, e) && g.server_shell.Player(g.local_client) == p);
        CHECK(g.leave_player() && !g.objects.get(old) && !g.server.GetClientUserData(old_client));
        CHECK(g.objects.advance(10));
        if (!g.restore_player(saved, e))
        {
            std::cerr << e << '\n';
            return false;
        }
        p = g.server_shell.Player(g.local_client);
        r = g.objects.get(p->m_hObject);
        CHECK(p->m_hObject != old && g.local_client != old_client && g.client_shell.player == p->m_hObject);
        CHECK(r->color.x == .2f && r->color.y == .4f && r->color.z == .6f && r->alpha == .5f);
        CHECK(p->restore_initialization && p->restore_engine_loaded && p->restore_model_loaded && !p->m_bFirstUpdate);
        CHECK(p->respawns == 0 && p->updates == 0 && p->attributes_received && r->client_movement);
        CHECK(r->state.m_Pos.x == 100 && r->state.m_Pos.y == 21 && r->dimensions.y == 20);
        CHECK(std::string(r->state.m_Name) == "saved-owner");
        CHECK(r->state.m_Scale.y == 1.2f && r->user_flags == 0x1234 && r->blocking_priority == 40);
        CHECK(r->animation.time == 17 && !r->animation.first && !r->animation.looping);
        CHECK(r->scheduled && std::abs(r->due - 10.4) < .00001 && !(r->state.m_Flags & FLAG_SHADOW));
        CHECK(vector_dot(r->velocity, r->velocity) == 0 && vector_dot(r->acceleration, r->acceleration) == 0);
        CHECK(p->m_damage.m_fHitPoints == 37 && p->m_damage.m_fArmorPoints == 23 && p->m_damage.m_fMaxHitPoints == 311);
        CHECK(p->m_damage.m_fResistance == .42f && p->m_nAttribStrength == 6 && p->m_nAttribSpeed == 6);
        CHECK(p->m_InventoryMgr.GetAmmoCount(1) == 1234 && p->m_fAirLevel == 42);
        CHECK(p->m_fMoveVel == 350 && p->m_fJumpVel == 600 && p->eye_level == 12 && p->crouched);
        CHECK(g.client_shell.stats.health == 37 && g.client_shell.stats.armor == 23 &&
              g.client_shell.stats.bullets == 1234);
        CHECK(g.client_shell.rotation.yaw == 1.25f && g.client_shell.rotation.pitch == -.25f);
        CHECK(g.client_shell.camera_position.y == 33 && g.client_shell.restored_entry && !g.client_shell.m_bFirstWorld);
        CHECK(g.client_shell.OnMessage(PlayerInitRequest{}, g.server_shell)); // Harmless restored init repeat.
        CHECK(p->m_damage.m_fHitPoints == 37 && p->m_InventoryMgr.GetAmmoCount(1) == 1234);
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerInit{}));
        CHECK(!g.server_shell.OnMessage(old_client, PlayerControls{}));
        CHECK(!g.server_shell.OnRestoredClientEnterWorld(g.local_client, old));
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerMove{})); // Wrong movement generation.
        auto position = r->state.m_Pos;
        CHECK(g.server.advance_physics(.1) && r->state.m_Pos.x == position.x && r->state.m_Pos.y == position.y);
        g.client_shell.movement_input.forward = true;
        g.client_shell.movement_input.crouch = true;
        CHECK(g.step(.05) && p->movement_messages == 6);
        CHECK(r->state.m_Pos.x != position.x || r->state.m_Pos.z != position.z);
        CHECK(g.objects.advance(.4) && p->updates == 1 && p->m_nAttribStrength == 6 &&
              p->m_damage.m_fMaxHitPoints == 311);
        CHECK(g.unload_world() && !g.server.client_count() && !g.server.string_count() &&
              g.objects.attachments().empty());
        return true;
    }
    bool failures(const char* root)
    {
        PlayerRestoreState s;
        std::string e;
        {
            GameSession g;
            CHECK(test_player::setup(g, root) && g.enter_player(e));
            CHECK(g.capture_player(s, e));
            auto* p = g.server_shell.Player(g.local_client);
            PlayerRestoreState untouched;
            untouched.world = "unchanged";
            p->m_bPowerupActivated[PU_TRIPLEDAMAGE] = true; // Active Anger now travels with its expiry.
            PlayerRestoreState powered;
            CHECK(g.capture_player(powered, e) && powered.player.powers[PU_TRIPLEDAMAGE]);
            p->m_bPowerupActivated[PU_TRIPLEDAMAGE] = false;
            p->ctf_team = 1; // CTF match state stays unsaved.
            CHECK(!g.capture_player(untouched, e) && untouched.world == "unchanged");
            p->ctf_team = 0;
            p->door_push_pending = true;
            CHECK(!g.capture_player(untouched, e));
            p->door_push_pending = false;
            CHECK(g.leave_player());
            const auto count = g.objects.snapshot().size(), strings = g.server.string_count();
            const auto factions = AI_Mgr::m_dwNumCabal;
            const std::vector<std::function<void(PlayerRestoreState&)>> corrupt{
                [](auto& x) { x.color.x = NAN; },
                [](auto& x) { x.alpha = 1.1f; },
                [](auto& v) { v.world = "other.dat"; },
                [](auto& v) { v.model = "missing.abc"; },
                [](auto& v) { v.skin = "missing.dtx"; },
                [](auto& v) { v.position.x = NAN; },
                [](auto& v) { v.dimensions.y = -1; },
                [](auto& v) { v.rotation.m_Spin = 10; },
                [](auto& v) { v.scale.y = 0; },
                [](auto& v) { v.next_update = INFINITY; },
                [](auto& v) { v.animation.time = -1; },
                [](auto& v) { v.animation.animation = UINT32_MAX; },
                [](auto& v) { v.hidden_nodes = {1}; },
                [](auto& v) { v.player.dead = true; },
                [](auto& v) { v.eye_level = 123; },
                [](auto& v) { v.position.y = 0; }, // Floor overlap.
                [](auto& v) { v.inventory.right[1] = 255; },
                [](auto& v) { v.damage.health = NAN; },
                [](auto& v) { v.player.strength = 0; },
                [](auto& v) { v.player.imprison_length = -1; },
                [](auto& v) { v.player.imprison_start_offset = NAN; },
                [](auto& v) { v.player.name = std::string(101, 'x'); }};
            for (const auto& mutate : corrupt)
            {
                auto bad = s;
                mutate(bad);
                CHECK(!g.restore_player(bad, e) && !e.empty() && g.restoration.error == e);
                CHECK(!g.local_client && !g.server.client_count() && !g.client_shell.m_bInWorld);
                CHECK(g.objects.snapshot().size() == count && g.server.string_count() == strings);
                CHECK(g.objects.attachments().empty() && AI_Mgr::m_dwNumCabal == factions);
            }
            CHECK(g.restore_player(s, e) && g.restoration.error.empty());
            CHECK(g.restoration.restored == 1 && g.unload_world());
        }
        GameSession g;
        // Fail late (view weapon resource), after successful association. No leaks/false success.
        CHECK(test_player::setup(g, std::filesystem::path(root) / "missing", true, false));
        const auto before_count = g.objects.snapshot().size(), before_strings = g.server.string_count();
        CHECK(!g.restore_player(s, e) && !e.empty());
        CHECK(!g.local_client && !g.server.client_count() && !g.client_shell.m_bInWorld);
        CHECK(g.objects.snapshot().size() == before_count && g.server.string_count() == before_strings &&
              g.objects.attachments().empty());
        CHECK(g.restoration.restored == 0);
        return true;
    }
    bool lifetime(const char* root)
    {
        GameSession g;
        std::string e;
        CHECK(test_player::setup(g, root) && g.enter_player(e));
        PlayerRestoreState s;
        CHECK(g.capture_player(s, e));
        const auto floor = g.collision;
        HCLIENT client = g.local_client;
        for (unsigned i = 0; i < 4; ++i)
        {
            auto old = g.client_shell.player;
            CHECK(g.unload_world() && !g.objects.get(old) && !g.server.GetClientUserData(client));
            CHECK(!CDestructable::m_dwNumDestructables && !AI_Mgr::m_dwNumCabal && !AI_Mgr::m_dwNumMonster);
            g.collision = floor;
            g.server.collision_ready = true;
            g.world_path = "synthetic.dat";
            CHECK(g.server_shell.PostStartWorld()); // No start marker needed for restored entry.
            CHECK(g.restore_player(s, e));
            client = g.local_client;
            auto* player = g.server_shell.Player(client);
            CHECK(player && player->respawns == 0 && player->m_InventoryMgr.m_hClient == client);
            CHECK(g.client_shell.move_mgr.m_ClientMoveCode == player->m_byClientMoveCode);
            CHECK(g.step(1. / 60) && player->movement_messages == 2);
        }
        CHECK(g.unload_world() && !g.server.string_count() && g.objects.attachments().empty());
        return true;
    }
    bool private_restore(const char* root)
    {
        GameSession g;
        std::string e;
        Config config;
        config.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(config, e));
        ModelView scene;
        for (unsigned i = 0; i < 3; ++i)
        {
            CHECK(g.request_world("01_SubwayA", e) && g.enter_player(e) && g.step(1. / 60));
            auto* p = g.server_shell.Player(g.local_client);
            p->m_damage.SetHitPoints(73);
            p->m_InventoryMgr.SetAmmoCount(1, 17);
            PlayerRestoreState s;
            CHECK(g.capture_player(s, e));
            auto old = p->m_hObject;
            CHECK(g.leave_player() && !g.objects.get(old));
            if (!g.restore_player(s, e))
            {
                std::cerr << e << '\n';
                return false;
            }
            p = g.server_shell.Player(g.local_client);
            CHECK(p->respawns == 0 && p->m_damage.m_fHitPoints == 73 && p->m_InventoryMgr.GetAmmoCount(1) == 17);
            CHECK(g.client_shell.stats.health == 73 && g.client_shell.position.x == s.position.x);
            g.client_shell.player_visible = true;
            CHECK(update_model_scene(g, scene, e) && !scene.world_draws.empty());
            auto it = std::find_if(scene.entries.begin(), scene.entries.end(),
                                   [&](const auto& v) { return v.object == g.objects.get(p->m_hObject)->id; });
            CHECK(it != scene.entries.end() && it->triangles == 941);
            CHECK(g.step(1. / 60) && p->movement_messages == 2);
            CHECK(g.unload_world() && !g.server.string_count() && g.objects.attachments().empty());
            CHECK(!CDestructable::m_dwNumDestructables && !AI_Mgr::m_dwNumCabal && !AI_Mgr::m_dwNumMonster);
            scene.clear();
            std::cout << "cycle=" << i
                      << " restored client, 73 HP/17 bullets, Caleb 941 triangles, movement, complete unload\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> cases{
        {"roundtrip", roundtrip}, {"failures", failures}, {"lifetime", lifetime}, {"private", private_restore}};
    if (argc != 3)
        return 2;
    const auto test = cases.find(argv[1]);
    return test != cases.end() && test->second(argv[2]) ? 0 : 1;
}
