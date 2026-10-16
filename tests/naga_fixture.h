#pragma once
#include "audio_fixture.h"
#include "sprite_view_fixture.h"
namespace test_naga
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        if (!test_sprite_view::mount(resources, root, error))
            return false;
        std::vector<std::uint8_t> skin;
        if (!resources.read("alpha.dtx", skin, error))
            return false;
        test_player::Directory dir;
        dir.children["models"].children["ammo"].files["beam.abc"] = test_model::abc().b;
        dir.children["skins"].children["ammo"].files["beamred.dtx"] = skin;
        dir.children["sounds"].children["weapons"].children["laser"].files["fire.wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        auto file = root / "naga.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
} // namespace test_naga
