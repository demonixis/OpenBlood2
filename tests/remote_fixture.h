#pragma once
#include "assault_fixture.h"
#include "game/original/ItemPickups.h"
#include "game/original/RemoteBomb.h"
namespace test_remote
{
    using namespace OpenBlood2;
    inline bool setup(GameSession& g, const std::filesystem::path& root)
    {
        if (!test_assault::setup(g, root))
            return false;
        test_player::Directory dir;
        for (auto name : {"c4remote_pv.abc"})
            dir.children["models"].children["weapons"].files[name] = test_model::abc(true).b;
        dir.children["skins"].children["weapons"].files["c_c4remote_pv_t.dtx"] = {1, 2, 3};
        dir.children["models"].children["powerups"].files["remotes_pu.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["powerups"].files["remotes_pu.dtx"] = {1, 2, 3};
        dir.children["models"].children["ammo"].files["c4.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["ammo"].files["c4_remote.dtx"] = {1, 2, 3};
        for (auto name : {"fire.wav", "alt.wav", "arm.wav", "detect.wav", "stick.wav", "explode.wav"})
            dir.children["sounds"].children["weapons"].children["remotes"].files[name] = test_audio::wave();
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
        auto file = root / "remote.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        return g.resources.mount(file, error);
    }
    inline CRemoteBomb* bomb(GameSession& g, DVector p, DVector direction = {0, 0, 1}, float speed = 0,
                             HOBJECT owner = nullptr)
    {
        ObjectCreateStruct s;
        s.m_Pos = p;
        legacy_copy(s.m_Name, "Remote probe");
        auto* b = dynamic_cast<CRemoteBomb*>(g.server.CreateObject(g.server.GetClass("CRemoteBomb"), &s));
        if (!b || !b->Setup(direction, 50, 100, speed, 250, owner ? owner : g.client_shell.player))
            return nullptr;
        auto* player = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(owner ? owner : g.client_shell.player));
        if (player && !player->AddRemoteBomb(b->m_hObject))
            return nullptr;
        return b;
    }
    inline bool equip(GameSession& g, unsigned count = 3)
    {
        auto* p = g.server_shell.Player(g.local_client);
        if (!p)
            return false;
        if (p->m_InventoryMgr.AddInventoryWeapon(8, static_cast<DBYTE>(count)) != InventoryResult::Ok)
            return false;
        std::string error;
        return g.select_weapon(11, error);
    }
} // namespace test_remote
