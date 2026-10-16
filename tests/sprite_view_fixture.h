#pragma once
#include "player_fixture.h"
namespace test_sprite_view
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        test_model::Writer d;
        d.u32(0);
        d.u32(0xfffffffeu);
        d.u16(4);
        d.u16(2);
        d.u16(1);
        d.u16(0);
        d.u32(2);
        d.u32(0);
        d.b.resize(1068);
        // Four columns: transparent red, partial green, opaque red, opaque black.
        const std::uint8_t palette[] = {255, 255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 0, 0};
        std::copy(std::begin(palette), std::end(palette), d.b.begin() + 44);
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 4; ++col)
                d.u8(col);
        d.b.insert(d.b.end(), {0x08, 0xff, 0x08, 0xff});
        test_player::Directory dir;
        dir.files["alpha.dtx"] = d.b;
        dir.files["bad.dtx"] = {1, 2, 3};
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        if (ec)
            return false;
        const auto path = root / "sprite-view.rez";
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        file.close();
        return resources.mount(path, error);
    }
    inline OpenBlood2::Sprite manifest()
    {
        return {10, true, false, 0, {"alpha.dtx", "bad.dtx", "missing.dtx", "ALPHA.DTX"}};
    }
} // namespace test_sprite_view
