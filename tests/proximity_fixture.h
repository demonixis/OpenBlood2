#pragma once
#include "assault_fixture.h"
#include "game/original/ItemPickups.h"
#include "game/original/ProximityBomb.h"
namespace test_proximity
{
    using namespace OpenBlood2;
    inline bool setup(GameSession& g, const std::filesystem::path& root)
    {
        if (!test_assault::setup(g, root))
            return false;
        test_player::Directory dir;
        for (auto name : {"c4proximity_pv.abc"})
            dir.children["models"].children["weapons"].files[name] = test_model::abc(true).b;
        dir.children["skins"].children["weapons"].files["c_c4prox_pv_t.dtx"] = {1, 2, 3};
        dir.children["models"].children["powerups"].files["proximities_pu.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["powerups"].files["proximities_pu.dtx"] = {1, 2, 3};
        dir.children["models"].children["ammo"].files["c4.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["ammo"].files["c4_prox.dtx"] = {1, 2, 3};
        for (auto name : {"fire.wav", "alt.wav", "arm.wav", "detect.wav", "stick.wav", "explode.wav"})
            dir.children["sounds"].children["weapons"].children["proximities"].files[name] = test_audio::wave();
        dir.children["sounds"].children["weapons"].children["time"].files["explode.wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        auto file = root / "proximity.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        return g.resources.mount(file, error);
    }
    inline CProximityBomb* bomb(GameSession& g, DVector p, DVector direction = {0, 0, 1}, float speed = 0,
                                HOBJECT owner = nullptr)
    {
        ObjectCreateStruct s;
        s.m_Pos = p;
        legacy_copy(s.m_Name, "Proximity probe");
        auto* b = dynamic_cast<CProximityBomb*>(g.server.CreateObject(g.server.GetClass("CProximityBomb"), &s));
        if (!b || !b->Setup(direction, 49, 100, speed, 250, owner ? owner : g.client_shell.player))
            return nullptr;
        return b;
    }
    inline bool equip(GameSession& g, unsigned count = 3)
    {
        auto* p = g.server_shell.Player(g.local_client);
        if (!p)
            return false;
        if (p->m_InventoryMgr.AddInventoryWeapon(7, static_cast<DBYTE>(count)) != InventoryResult::Ok)
            return false;
        std::string error;
        return g.select_weapon(10, error);
    }
} // namespace test_proximity
