#pragma once
#include "sprite_view_fixture.h"
namespace test_explosion_sprite
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error,
                      bool smoke = true)
    {
        if (!test_sprite_view::mount(resources, root, error))
            return false;
        test_model::Writer spr;
        spr.u32(2);
        spr.u32(10);
        spr.u32(1);
        spr.u32(0);
        spr.u32(0);
        for (int i = 0; i < 2; ++i)
        {
            const std::string path = "alpha.dtx";
            spr.u16(path.size());
            spr.b.insert(spr.b.end(), path.begin(), path.end());
        }
        test_player::Directory dir;
        for (auto name : {"fireball2.spr", "debrisimp.spr", "tesaltloop.spr", "explosn.spr"})
            dir.children["sprites"].files[name] = spr.b;
        if (smoke)
            dir.children["sprites"].files["smokepuff2.spr"] = spr.b;
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        auto file = root / "explosion-sprites.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
} // namespace test_explosion_sprite
