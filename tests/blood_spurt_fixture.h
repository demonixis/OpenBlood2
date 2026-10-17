#pragma once
#include "audio_fixture.h"
#include "blood_splat_fixture.h"
namespace test_blood_spurt
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        if (!test_blood_splat::mount(resources, root, error))
            return false;
        std::vector<std::uint8_t> dtx;
        if (!resources.read("alpha.dtx", dtx, error))
            return false;
        test_player::Directory dir;
        for (int i = 1; i <= 3; ++i)
            dir.children["spritetextures"].children["particles"].files["blooddrop_" + std::to_string(i) + ".dtx"] = dtx;
        for (int i = 0; i <= 3; ++i)
            dir.children["sounds"].children["gibs"].children["flesh"].files["gib_impact" + std::to_string(i) + ".wav"] =
                test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        auto file = root / "blood-spurts.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
    inline bool floor(OpenBlood2::GameSession& g, std::string& error)
    {
        OpenBlood2::WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        g.server.collision_ready = g.collision.build(floor, error);
        return g.server.collision_ready;
    }
} // namespace test_blood_spurt
