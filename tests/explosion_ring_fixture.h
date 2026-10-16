#pragma once
#include "explosion_sprite_fixture.h"
namespace test_explosion_ring
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error,
                      bool all = true)
    {
        if (!test_explosion_sprite::mount(resources, root, error))
            return false;
        std::vector<std::uint8_t> skin;
        if (!resources.read("alpha.dtx", skin, error))
            return false;
        test_player::Directory dir;
        for (auto name : {"firepart64.dtx", "smoke32_4.dtx", "smoke32_5.dtx", "smoke32_6.dtx", "smoke64_2.dtx"})
            if (all || std::string_view(name) == "firepart64.dtx")
                dir.children["spritetextures"].files[name] = skin;
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        auto file = root / "explosion-rings.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
} // namespace test_explosion_ring
