#pragma once
#include "explosion_sprite_fixture.h"
namespace test_explosion_fragment
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        if (!test_explosion_sprite::mount(resources, root, error))
            return false;
        std::vector<std::uint8_t> skin;
        if (!resources.read("alpha.dtx", skin, error))
            return false;
        test_player::Directory dir;
        for (auto name : {"exp_sphere.abc", "duel_sphere.abc"})
            dir.children["models"].children["explosions"].files[name] = test_model::abc().b;
        dir.children["skins"].children["explosions"].files["explosion_1.dtx"] = skin;
        dir.children["spritetextures"].files["waterblue.dtx"] = skin;
        dir.children["models"].children["gibs"].files["gibmetal2.abc"] = test_model::abc().b;
        dir.children["skins"].children["gibs"].files["gibmetal3.dtx"] = skin;
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        auto file = root / "explosion-fragments.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
} // namespace test_explosion_fragment
