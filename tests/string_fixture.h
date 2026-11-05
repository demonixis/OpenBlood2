#pragma once
#include "player_fixture.h"
#include "string_resource_fixture.h"
namespace test_strings
{
    inline bool mount(OpenBlood2::GameSession& g, const std::filesystem::path& root,
                      const std::vector<std::uint8_t>& bytes)
    {
        test_player::Directory dir;
        dir.files["cres.dll"] = bytes;
        test_player::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        std::ofstream file(root / "strings.rez", std::ios::binary);
        file.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
        file.close();
        std::string error;
        return file.good() && g.resources.mount(root / "strings.rez", error);
    }
} // namespace test_strings
