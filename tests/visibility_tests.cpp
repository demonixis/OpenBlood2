#include "engine/resources.h"
#include "engine/visibility.h"
#include "game/original/AI_Mgr.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
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
    // Root BSP split on x = 0 (n0, leaf 0). Behind it n1 (x = -100, leaf 1) keeps x > -100 inside.
    // In front, n2 has no leaf and an outside back side, so the tree replaces it with n3
    // (x = 100, leaf 2). n3's back side n4 (leaf 3) uses a skipped surface: the tree drops it.
    // Tree: T0(n0) -> back T1(n1), front T2(n3). A full list reads, in order:
    // leaf 0, descend T1, leaf 1, descend T2, leaf 2.
    WorldModel fixture()
    {
        WorldModel m;
        m.planes = {{{1, 0, 0}, 0}, {{1, 0, 0}, -100}, {{1, 0, 0}, 100}};
        m.surfaces.resize(2);
        m.surfaces[1].visibility_skip = 1;
        m.polygons.resize(2);
        m.polygons[1].surface = 1;
        using N = BspNode;
        m.bsp_nodes = {{0, 0, 0, {1, 2}},
                       {1, 0, 1, {N::outside, N::inside}},
                       {2, 0, UINT16_MAX, {N::outside, 3}},
                       {2, 0, 2, {4, N::inside}},
                       {2, 1, 3, {N::outside, N::outside}}};
        m.root_node = 0;
        // Leaf 0 owns a full list and a conditional portal list; leaf 3 refers forward to leaf 4.
        const std::vector<std::vector<std::uint8_t>> lists{{0x01}, {0xff}, {0x06}, {0x0c}, {0x1f}};
        for (const auto& bytes : lists)
        {
            m.visibility_lists.push_back(
                {0xffff, static_cast<std::uint32_t>(m.visibility.size()), static_cast<std::uint32_t>(bytes.size())});
            m.visibility.insert(m.visibility.end(), bytes.begin(), bytes.end());
        }
        m.visibility_lists[1].portal = 0;
        m.bsp_leaves = {{0, 2}, {2, 1}, {3, 1}, {0, 0, 4}, {4, 1}};
        return m;
    }
    bool decode(const char*)
    {
        WorldVisibility v;
        std::string error;
        auto m = fixture();
        CHECK(v.build(m, error) && v.available());
        const auto& s = v.stats();
        CHECK(s.tree_nodes == 3 && s.collapsed_nodes == 1 && s.skipped_nodes == 1 && s.leaves == 5 && s.lists == 5 &&
              s.conditional_lists == 1 && s.list_bytes == 5 && s.trailing_bytes == 0);
        // The original descent keeps the last leaf met; the collapsed node never changes it.
        CHECK(v.find_leaf({-50, 0, 0}) == 1 && v.find_leaf({50, 0, 0}) == 0 && v.find_leaf({150, 0, 0}) == 2 &&
              v.find_leaf({-150, 0, 0}) == 1 && v.find_leaf({-0.00005f, 0, 0}) == 0);
        // a: linked at T1; b: linked at T2; c: straddles x = 0 (linked at the root);
        // d: behind x = -100 and e: behind n3 into the dropped node are never linked.
        const std::vector<VisibilitySphere> spheres{
            {{-50, 0, 0}, 1}, {{150, 0, 0}, 1}, {{0.5f, 0, 0}, 5}, {{-150, 0, 0}, 1}, {{50, 0, 0}, 1}};
        auto seen = [&](std::uint32_t leaf, std::vector<bool> expected)
        {
            std::vector<bool> visible;
            return v.query(leaf, spheres, visible) && visible == expected;
        };
        CHECK(seen(0, {false, false, true, false, false}));  // Leaf 0 bit only: root-linked objects.
        CHECK(seen(1, {true, false, true, false, false}));   // 0x06: T1 and its leaf; c is passed down (0.5 < 5).
        CHECK(seen(2, {false, true, true, false, false}));   // 0x0c: T2 and its leaf; c passes the front test.
        CHECK(seen(3, {false, false, false, false, false})); // Forward share copies empty lists.
        CHECK(seen(4, {true, true, true, false, false}));    // Every bit set.
        // A sphere just behind x = 0 that does not straddle it is linked at T1, not the root.
        std::vector<bool> visible{true};
        const VisibilitySphere narrow[]{{{-0.5f, 0, 0}, 0.25f}};
        CHECK(v.query(1, narrow, visible) && visible == std::vector<bool>{true});
        CHECK(v.query(0, narrow, visible) && visible == std::vector<bool>{false});
        CHECK(v.query(2, narrow, visible) && visible == std::vector<bool>{false});
        visible = {true};
        CHECK(!v.query(5, narrow, visible) && visible == std::vector<bool>{true});
        // Bits past a list are a decode failure, not the next list's bytes.
        auto short_list = m;
        short_list.visibility_lists[4].size = 0;
        CHECK(!v.build(short_list, error) && error.find("list 4") != std::string::npos && !v.available());
        // Leftover bits in the final byte are allowed; whole spare bytes are counted.
        auto spare = m;
        spare.visibility.push_back(0);
        spare.visibility_lists.back().size = 2;
        CHECK(v.build(spare, error) && v.stats().trailing_bytes == 1);
        auto cycle = m;
        cycle.bsp_nodes[1].sides[1] = 0;
        CHECK(!v.build(cycle, error) && error.find("reached twice") != std::string::npos);
        auto empty = m;
        empty.visibility_lists.clear();
        CHECK(v.build(empty, error) && !v.available() && v.find_leaf({}) == UINT32_MAX);
        CHECK(!v.query(0, narrow, visible));
        return true;
    }
    bool private_data(const char* root)
    {
        Config config;
        config.data_dir = root;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        std::size_t worlds = 0, available = 0;
        for (const auto& [path, entry] : resources.index())
        {
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World world;
            CHECK(resources.read(path, bytes, error));
            if (!read_world(bytes, world, error))
            {
                std::cerr << path << ": " << error << '\n';
                return false;
            }
            const auto& model = world.models.front();
            WorldVisibility v;
            if (!v.build(model, error))
            {
                std::cerr << path << ": " << error << '\n';
                return false;
            }
            const auto& s = v.stats();
            ++worlds;
            available += v.available();
            std::cout << path << " nodes=" << model.bsp_nodes.size() << " tree=" << s.tree_nodes
                      << " skipped=" << s.skipped_nodes << " collapsed=" << s.collapsed_nodes << " leaves=" << s.leaves
                      << " lists=" << s.lists << " conditional=" << s.conditional_lists << " bytes=" << s.list_bytes
                      << " trailing=" << s.trailing_bytes << '\n';
            CHECK(s.trailing_bytes == 0);
            if (path == "worlds/12_train_station.dat")
                CHECK(s.leaves == 1513 && s.lists == 1447 && s.list_bytes == 51589 && v.available());
        }
        std::cout << "worlds=" << worlds << " available=" << available << '\n';
        CHECK(worlds >= 36 && available == worlds);
        return true;
    }
    std::unique_ptr<GameSession> start(const char* root, const char* world)
    {
        auto game = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        std::string error;
        if (!game->initialize() || !game->resources.mount_game(config, error) ||
            !GameSession::start_new_game(game, world, 1, 0, error))
        {
            std::cerr << world << ": " << error << '\n';
            return {};
        }
        return game;
    }
    AI_Mgr* actor(GameSession& g, std::string_view name)
    {
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && name == r->state.m_Name)
                return dynamic_cast<AI_Mgr*>(r->game.get());
        return nullptr;
    }
    bool step(GameSession& g, double seconds)
    {
        for (int i = 0; i < int(seconds * 60); ++i)
            if (!g.step(1.0 / 60))
            {
                for (const auto& line : g.step_errors())
                    std::cerr << line << '\n';
                return false;
            }
        return true;
    }
    bool touch(GameSession& g, DVector feet)
    {
        std::string error;
        auto* player = g.server_shell.Player(g.local_client);
        feet.y += g.objects.get(g.client_shell.player)->dimensions.y + .01f;
        return g.server.TeleportObject(g.client_shell.player, &feet) == DE_OK && player->TeleportClientToServerPos() &&
               g.pump_player_messages(error);
    }
    // 12_Train_Station: Trigger10 scripts the Fanatics pride and wrath out of their cubby
    // (Script 2; SetMovement RUN; FollowPath; SetState ATTACK). Walls hide them from the player,
    // but the retail leaf lists wake them, so the scripts run without the player seeing them.
    // With the lists cleared, the segment stand-in leaves both asleep: the scripts never run.
    // No primary-world segment between the centres and box tops (the former stand-in) is clear.
    bool walled(GameSession& g, HOBJECT a, HOBJECT b)
    {
        const auto* x = g.objects.get(a);
        const auto* y = g.objects.get(b);
        const DVector p = x->state.m_Pos, q = y->state.m_Pos, pt{p.x, p.y + x->dimensions.y, p.z},
                      qt{q.x, q.y + y->dimensions.y, q.z};
        for (const auto& [from, to] : {std::pair{p, q}, {pt, q}, {p, qt}, {pt, qt}})
        {
            CollisionHit hit;
            if (g.collision.sweep(from, to, {}, hit) != CollisionQuery::Hit)
                return false;
        }
        return true;
    }
    bool train_station_run(const char* root, bool lists)
    {
        auto game = start(root, "12_Train_Station");
        CHECK(game);
        auto& g = *game;
        CHECK(g.visibility.available());
        if (!lists)
            g.visibility.clear();
        CHECK(step(g, .5));
        AI_Mgr* guards[]{actor(g, "pride"), actor(g, "wrath")};
        DVector before[2];
        for (int i = 0; i < 2; ++i)
        {
            CHECK(guards[i] && guards[i]->script_selections == 0);
            before[i] = g.objects.get(guards[i]->m_hObject)->state.m_Pos;
        }
        CHECK(touch(g, {-2192, 251, -1280}));
        for (auto* a : guards)
            CHECK(walled(g, g.client_shell.player, a->m_hObject));
        CHECK(step(g, 6));
        for (int i = 0; i < 2; ++i)
        {
            const auto* r = g.objects.get(guards[i]->m_hObject);
            CHECK(r && r->life == Life::Alive);
            const auto moved = vector_subtract(r->state.m_Pos, before[i]);
            const auto& probe = r->activation_probe;
            std::cout << (lists ? "lists " : "segments ") << r->state.m_Name
                      << " selections=" << guards[i]->script_selections << " updates=" << guards[i]->script_updates
                      << " pings=" << r->activation_pings << " leaf=" << probe.observer_leaf
                      << " moved=" << std::sqrt(vector_dot(moved, moved)) << '\n';
            CHECK(probe.leaf_visibility == lists);
            if (lists)
                CHECK(guards[i]->script_updates > 0 && r->activation_pings > 0 && probe.observer_leaf != UINT32_MAX &&
                      vector_dot(moved, moved) > 128 * 128);
            else
                CHECK(!r->activation_pings && vector_dot(moved, moved) < 1);
        }
        return true;
    }
    bool train_station(const char* root)
    {
        return train_station_run(root, true) && train_station_run(root, false);
    }
    // 02_CityHub: the gidtrigger at the foot of the north ramp scripts Gideon to Wait 15 s before
    // running off. From the ramp a wall hides him from every segment, but the retail leaf lists
    // wake him there, so he idles in PASSIVE and the wait keeps an idle clip. With the lists
    // cleared he sleeps in the initial run_knife1 (clip 0 of SetAnimation(0)) until the trigger
    // and waits in it, the tester report.
    bool cityhub_run(const char* root, bool lists)
    {
        auto game = start(root, "02_CityHub");
        CHECK(game);
        auto& g = *game;
        CHECK(g.visibility.available());
        if (!lists)
            g.visibility.clear();
        CHECK(step(g, .5));
        auto* gideon = actor(g, "Gideon");
        CHECK(gideon && g.server.GetModelAnimation(gideon->m_hObject) == 0);
        CHECK(touch(g, {-384, 110, 2872}) && walled(g, g.client_shell.player, gideon->m_hObject));
        CHECK(step(g, 2) && !gideon->script_selections);
        CHECK(touch(g, {-384, 256, 2608}));
        CHECK(step(g, 5));
        CHECK(actor(g, "Gideon") == gideon && gideon->script_selections > 0);
        const auto clip = g.server.GetModelAnimation(gideon->m_hObject);
        const auto* idle = gideon->AnimSound()->m_nAnim_IDLE;
        const auto* r = g.objects.get(gideon->m_hObject);
        std::cout << (lists ? "lists" : "segments") << " Gideon clip=" << clip << " pings=" << r->activation_pings
                  << " script updates=" << gideon->script_updates
                  << " observer leaf=" << r->activation_probe.observer_leaf << '\n';
        CHECK(r->activation_probe.leaf_visibility == lists);
        if (lists)
            CHECK(clip != 0 && std::find(idle, idle + 4, clip) != idle + 4 && r->activation_pings > 0);
        else
            CHECK(clip == 0);
        return true;
    }
    bool cityhub(const char* root)
    {
        return cityhub_run(root, true) && cityhub_run(root, false);
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"decode", decode}, {"private", private_data}, {"train_station", train_station}, {"cityhub", cityhub}};
    if (argc < 2)
        return 2;
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
