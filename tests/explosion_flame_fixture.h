#pragma once
#include "explosion_ring_fixture.h"
namespace test_explosion_flame
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        if (!test_explosion_ring::mount(resources, root, error))
            return false;
        std::vector<std::uint8_t> skin;
        if (!resources.read("alpha.dtx", skin, error))
            return false;
        test_player::Directory dir;
        dir.children["spritetextures"].files["teslaprime_3.dtx"] = skin;
        dir.children["spritetextures"].files["drop32_1.dtx"] = skin;
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        auto file = root / "explosion-flames.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
    inline HOBJECT create(OpenBlood2::GameSession& g)
    {
        return g.objects.create(g.objects.find_class("CClientExplosionSFX"), {}, OpenBlood2::Creation::World,
                                {{"Name", std::string("diagnostic Tesla")},
                                 {"Pos", DVector{0, -30, 60}},
                                 {"Direction", DVector{0, 2, 0}},
                                 {"ExplosionType", 8}});
    }
    inline bool trigger(OpenBlood2::GameSession& g, HOBJECT h)
    {
        auto s = g.server.CreateString("TRIGGER");
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = m && g.server.WriteToMessageHString(m, s) == DE_OK && g.server.EndMessage(m) == DE_OK;
        g.server.FreeString(s);
        return ok;
    }
} // namespace test_explosion_flame
