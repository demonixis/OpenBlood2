#include "app/model_scene.h"
#include "game/original/AI_Mgr.h"
#include "game/original/GameStartPoint.h"
#include "player_fixture.h"
#include <algorithm>
#include <cmath>
#include <functional>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << " | " << e << '\n';                                 \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    GameSession* active = nullptr;
    unsigned deliveries = 0;
    bool remove_player = false, remove_start = false;
    DVector observed_position{};
    struct EntryProbe : BaseClass
    {
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read) override
        {
            if (message != 1000)
                return 0;
            auto& g = *active;
            auto text = g.server.ReadFromMessageHString(read);
            g.server.FreeString(text);
            ++deliveries;
            auto* p = g.server_shell.Player(g.local_client);
            if (p)
            {
                g.server.GetObjectPos(p->m_hObject, &observed_position);
                if (remove_player)
                    g.server.RemoveObject(p->m_hObject);
            }
            if (remove_start)
                g.server.RemoveObject(sender);
            return 1;
        }
    };
    HOBJECT marker(GameSession& g, const char* name, DVector position, bool multiplayer = false, bool trigger = false)
    {
        return g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                                {{"Name", std::string(name)},
                                 {"Pos", position},
                                 {"Multiplayer", multiplayer},
                                 {"Rotation", EulerAngles{{.4f, -1.25f, .3f}}},
                                 {"TriggerTarget", std::string(trigger ? "entry-probe" : "")},
                                 {"TriggerMessage", std::string(trigger ? "OBSERVE" : "")}});
    }
    bool destination(GameSession& g, const CollisionWorld& floor)
    {
        g.collision = floor;
        g.server.collision_ready = true;
        g.world_path = "destination.dat";
        g.state = SessionState::Inspection;
        return g.server_shell.PostStartWorld();
    }
    bool named(const char* root)
    {
        GameSession g;
        std::string e;
        CHECK(test_player::setup(g, root) && g.enter_player(e));
        auto* p = g.server_shell.Player(g.local_client);
        p->m_damage.SetHitPoints(37);
        p->m_damage.SetArmorPoints(23);
        p->m_damage.m_fMaxHitPoints = 311;
        p->m_damage.m_fResistance = .42f;
        p->m_nAttribSpeed = 6;
        p->m_InventoryMgr.SetAmmoCount(1, 1234);
        CHECK(p->m_InventoryMgr.AddKey({"old-key", "Old key", "icon", "selected", 2}) == InventoryResult::Ok);
        PlayerRestoreState s;
        CHECK(g.capture_player(s, e));
        auto old = p->m_hObject;
        auto old_client = g.local_client;
        auto floor = g.collision;
        CHECK(!g.restore_player(s, e, PlayerRestoreMode::KeepAlive, "arrival") && g.client_shell.player == old);
        CHECK(g.unload_world() && destination(g, floor));
        // Old coordinates would collide in the destination: they must not be used for placement clearance.
        s.position = {0, -20, 0};
        CHECK(marker(g, "unused", {200, 100, 200}));
        CHECK(marker(g, "ARRIVAL", {-120, 100, 220}, false, true));
        CHECK(marker(g, "last", {300, 100, 300}));
        static ClassInfo type{
            "EntryProbe", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<EntryProbe>(); }, {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct desc;
        SAFE_STRCPY(desc.m_Name, "entry-probe");
        CHECK(g.objects.create(&type, desc));
        active = &g;
        deliveries = 0;
        remove_start = true;
        CHECK(!g.restore_player(s, e)); // Ordinary restore cannot cross worlds.
        CHECK(g.restore_player(s, e, PlayerRestoreMode::KeepAlive, "arrival"));
        p = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(p->m_hObject);
        CHECK(p->m_hObject != old && g.local_client != old_client && !g.server.GetClientUserData(old_client));
        CHECK(deliveries == 1 && observed_position.y == -20); // Trigger before teleport; marker can disappear.
        CHECK(r->state.m_Pos.x == -120 && r->state.m_Pos.z == 220 && std::abs(r->state.m_Pos.y - 41.001f) < .01f);
        CHECK(p->entry_start_name == "ARRIVAL" && !p->entry_start_fallback && p->respawns == 0);
        CHECK(p->m_fPitch == 0 && p->m_fYaw == -1.25f && g.client_shell.rotation.yaw == -1.25f);
        CHECK(p->m_damage.m_fHitPoints == 37 && p->m_damage.m_fArmorPoints == 23 && p->m_damage.m_fMaxHitPoints == 311);
        CHECK(p->m_damage.m_fResistance == .42f && p->m_nAttribSpeed == 6 && p->m_fMoveVel == 350);
        CHECK(p->m_InventoryMgr.GetAmmoCount(1) == 1234 && p->m_InventoryMgr.keys.empty() &&
              s.inventory.keys.size() == 1);
        CHECK(g.client_shell.stats.health == 37 && g.client_shell.stats.bullets == 1234 &&
              g.client_shell.restored_entry);
        CHECK(g.restoration.last_mode == PlayerRestoreMode::KeepAlive &&
              g.restoration.destination_world == "destination.dat");
        CHECK(g.restoration.selected_start == "ARRIVAL" && !g.restoration.start_fallback);
        g.client_shell.movement_input.forward = true;
        CHECK(g.step(1. / 60) && p->movement_messages == 2 && deliveries == 1);
        CHECK(r->state.m_Pos.x != -120 || r->state.m_Pos.z != 220);
        CHECK(g.unload_world() && !g.server.string_count() && g.objects.attachments().empty());
        CHECK(!CDestructable::m_dwNumDestructables && !AI_Mgr::m_dwNumCabal && !AI_Mgr::m_dwNumMonster);
        return true;
    }
    bool fallback(const char* root)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            GameSession g;
            std::string e;
            CHECK(test_player::setup(g, root) && g.enter_player(e));
            PlayerRestoreState s;
            CHECK(g.capture_player(s, e));
            if (i == 3)
            {
                s.crouched = true;
                s.eye_level = 12;
                s.dimensions.y = 20;
            }
            auto floor = g.collision;
            CHECK(g.unload_world() && destination(g, floor));
            // FindNamedObjects prepends stable-slot entries: first-created is last in this fixture's list.
            auto last = marker(g, "fallback-solo", {200, 100, 200});
            CHECK(last && marker(g, "start", {100, 100, 100}));
            CHECK(marker(g, "multiplayer", {300, 100, 300}, true));
            auto* list = g.server.FindNamedObjects("Blood2StartPoint");
            CHECK(list && list->m_nInList == 3 && list->m_pFirstLink->m_pNext->m_pNext->m_hObject == last);
            g.server.RelinquishList(list);
            CHECK(g.restore_player(s, e, PlayerRestoreMode::KeepAlive, i % 2 ? "missing" : ""));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p->entry_start_name == (i % 2 ? "fallback-solo" : "start"));
            CHECK(p->entry_start_fallback == bool(i % 2));
            CHECK(g.client_shell.position.x == (i % 2 ? 200 : 100));
            CHECK(p->crouched == s.crouched && p->eye_level == s.eye_level);
            CHECK(g.objects.get(p->m_hObject)->dimensions.y == s.dimensions.y);
            CHECK(g.step(1. / 60) && p->movement_messages == 2);
            CHECK(g.unload_world() && !g.server.string_count() && g.objects.attachments().empty());
            CHECK(!CDestructable::m_dwNumDestructables && !AI_Mgr::m_dwNumCabal && !AI_Mgr::m_dwNumMonster);
        }
        return true;
    }
    bool failures(const char* root)
    {
        GameSession g;
        std::string e;
        CHECK(test_player::setup(g, root) && g.enter_player(e));
        PlayerRestoreState s;
        CHECK(g.capture_player(s, e));
        auto floor = g.collision;
        CHECK(g.unload_world() && destination(g, floor));
        const auto revision = g.world_revision;
        auto rejected = [&](PlayerRestoreMode mode, std::string_view start)
        {
            const auto before = g.objects.snapshot().size(), strings = g.server.string_count();
            return !g.restore_player(s, e, mode, start) && !e.empty() && g.objects.snapshot().size() == before &&
                   g.server.string_count() == strings && !g.local_client && !g.server.client_count() &&
                   !g.client_shell.m_bInWorld && !CDestructable::m_dwNumDestructables && !AI_Mgr::m_dwNumCabal &&
                   !AI_Mgr::m_dwNumMonster && g.objects.attachments().empty() && g.world_revision == revision &&
                   g.world_path == "destination.dat";
        };
        CHECK(rejected(PlayerRestoreMode::KeepAlive, "missing") && e == "No single-player GameStartPoint");
        CHECK(rejected(static_cast<PlayerRestoreMode>(99), ""));
        CHECK(rejected(PlayerRestoreMode::KeepAlive, std::string(101, 'x')));
        CHECK(rejected(PlayerRestoreMode::KeepAlive, std::string("a\0b", 3)));
        auto start = marker(g, "arrival", {2000, 100, 2000});
        CHECK(start && rejected(PlayerRestoreMode::KeepAlive, "arrival")); // No ground.
        g.objects.get(start)->state.m_Pos = {100, -20, 100};
        CHECK(rejected(PlayerRestoreMode::KeepAlive, "arrival")); // Embedded body.
        CHECK(g.objects.remove(start) && g.objects.finish_frame());
        CHECK(marker(g, "arrival", {100, 100, 100}, false, true));
        static ClassInfo type{
            "EntryProbe", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<EntryProbe>(); }, {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct desc;
        SAFE_STRCPY(desc.m_Name, "entry-probe");
        CHECK(g.objects.create(&type, desc));
        active = &g;
        deliveries = 0;
        remove_player = true;
        CHECK(rejected(PlayerRestoreMode::KeepAlive, "arrival") && deliveries == 1);
        remove_player = false;
        CHECK(g.restore_player(s, e, PlayerRestoreMode::KeepAlive, "arrival") && deliveries == 2);
        CHECK(g.unload_world() && !g.server.string_count());
        return true;
    }
    bool private_entry(const char* root)
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
            CHECK(p->m_InventoryMgr.AddKey({"exit-key", "Synthetic check", "icon", "selected", 1}) ==
                  InventoryResult::Ok);
            PlayerRestoreState s;
            CHECK(g.capture_player(s, e));
            auto old = p->m_hObject;
            CHECK(g.request_world("02_cityhub", e) && !g.objects.get(old));
            CHECK(g.restore_player(s, e, PlayerRestoreMode::KeepAlive, "01_egressa"));
            p = g.server_shell.Player(g.local_client);
            CHECK(p->respawns == 0 && p->m_damage.m_fHitPoints == 73 && p->m_InventoryMgr.GetAmmoCount(1) == 17);
            CHECK(p->m_InventoryMgr.keys.empty() && s.inventory.keys.size() == 1 && !p->entry_start_fallback);
            CHECK(p->entry_start_name == "01_egressa" && g.client_shell.stats.health == 73);
            g.client_shell.player_visible = true;
            CHECK(update_model_scene(g, scene, e) && !scene.world_draws.empty());
            auto found = std::find_if(scene.entries.begin(), scene.entries.end(),
                                      [&](const auto& v) { return v.object == g.objects.get(p->m_hObject)->id; });
            CHECK(found != scene.entries.end() && found->triangles == 941);
            if (i == 0)
                for (size_t n = 0; n < g.entries.size(); ++n)
                    if (g.entries[n].state == EntryState::Failed)
                        std::cout << "Destination failure: " << g.world.objects[n].class_name << ": "
                                  << g.entries[n].detail << '\n';
            auto pos = g.client_shell.position;
            CHECK(pos.x == 704 && pos.z == 2368 && std::abs(pos.y + 85.999f) < .01f);
            std::map<EntryState, size_t> counts;
            std::map<std::string, size_t> missing;
            for (size_t n = 0; n < g.entries.size(); ++n)
            {
                ++counts[g.entries[n].state];
                if (g.entries[n].state == EntryState::Unsupported || g.entries[n].state == EntryState::Failed)
                    ++missing[g.world.objects[n].class_name];
            }
            std::cout << "cycle=" << i << " CityHub named entry at " << pos.x << ',' << pos.y << ',' << pos.z
                      << "; HP=73 bullets=17 keys=0 respawns=0 Caleb triangles=941\n";
            for (auto [state, count] : counts)
                std::cout << entry_state_name(state) << '=' << count << ' ';
            std::cout << '\n';
            for (auto [name, count] : missing)
                std::cout << name << '=' << count << ' ';
            std::cout << '\n';
            CHECK(g.step(1. / 60) && p->movement_messages == 2);
            CHECK(g.unload_world() && !g.server.string_count() && g.objects.attachments().empty());
            CHECK(!CDestructable::m_dwNumDestructables && !AI_Mgr::m_dwNumCabal && !AI_Mgr::m_dwNumMonster);
            scene.clear();
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> cases{
        {"named", named}, {"fallback", fallback}, {"failures", failures}, {"private", private_entry}};
    if (argc != 3)
        return 2;
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
