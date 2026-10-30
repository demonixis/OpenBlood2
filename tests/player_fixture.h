#pragma once
#include "game/server_adapter.h"
#include "resource_fixture.h"
#include <fstream>
#include <iostream>
namespace test_player
{
    using namespace OpenBlood2;
#define FIXTURE_CHECK(x)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << "fixture: " #x "\n";                                                                          \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
    using test_resource::cstr;
    using test_resource::Directory;
    using test_resource::write_dir;
    using test_resource::Writer;
    inline bool setup(GameSession& game, const std::filesystem::path& root, bool with_start = true,
                      bool weapon_assets = true, bool pickup_assets = false)
    {
        FIXTURE_CHECK(game.initialize());
        Directory dir;
        dir.children["models"].children["characters"].files["caleb.abc"] = test_model::abc(false, true).b;
        dir.children["skins"].children["characters"].files["caleb1.dtx"] = {1, 2, 3}; // Indexed only, never decoded.
        if (weapon_assets)
            for (auto path : {"beretta_pv.abc", "lh_beretta_pv.abc", "c_knife_pv.abc"})
                dir.children["models"].children["weapons"].files[path] = test_model::abc(true).b;
        if (weapon_assets)
            for (auto path : {"c_beretta_pv_t.dtx", "c_knife_pv_t.dtx"})
                dir.children["skins"].children["weapons"].files[path] = {1, 2, 3};
        if (weapon_assets)
            for (auto name : {"beretta_pu", "knifec_pu"})
            {
                dir.children["models"].children["powerups"].files[std::string(name) + ".abc"] = test_model::abc(true).b;
                dir.children["skins"].children["powerups"].files[std::string(name) + ".dtx"] = {1, 2, 3};
            }
        if (pickup_assets)
            for (auto name : {"bulletammo_pu", "shellammo_pu", "bmgammo_pu", "flareammo_pu", "diebugdieammo_pu",
                              "howitzerammo_pu", "medkit_pu", "flashlight_pu", "binoculars_pu", "fuelammo_pu",
                              "batteryammo_pu", "health_pu", "megahealth_pu", "ward_pu", "necroward_pu"})
            {
                dir.children["models"].children["powerups"].files[std::string(name) + ".abc"] =
                    test_model::abc(false, true).b;
                dir.children["skins"].children["powerups"].files[std::string(name) + ".dtx"] = {1, 2, 3};
            }
        Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        FIXTURE_CHECK(!ec);
        {
            std::ofstream file(root / "synthetic.rez", std::ios::binary);
            file.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
            FIXTURE_CHECK(file.good());
        }
        std::string error;
        FIXTURE_CHECK(game.resources.mount(root / "synthetic.rez", error));
        WorldModel floor;
        floor.points = {{-1000, 0, -1000}, {1000, 0, -1000}, {1000, 0, 1000}, {-1000, 0, 1000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        FIXTURE_CHECK(game.collision.build(floor, error));
        game.server.collision_ready = true;
        game.world_path = "synthetic.dat";
        game.state = SessionState::Inspection;
        if (with_start)
            FIXTURE_CHECK(game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                              {{"Name", std::string("START")},
                                               {"Pos", DVector{32, 100, 64}},
                                               {"Rotation", EulerAngles{{0, .75f, 0}}}}));
        game.server_shell.PreStartWorld();
        FIXTURE_CHECK(game.server_shell.PostStartWorld());
        return true;
    }

#undef FIXTURE_CHECK
} // namespace test_player
