#pragma once
#include "energy_fixture.h"
#include "game/original/FlareProjectile.h"
#include "game/original/WeaponPickups.h"
namespace test_flare
{
    using namespace OpenBlood2;
    inline bool mount(GameSession& g, const std::filesystem::path& root)
    {
        test_player::Directory dir;
        test_model::Writer sprite;
        for (auto n : {1u, 15u, 1u, 1u, 0u})
            sprite.u32(n);
        std::string frame = "alpha.dtx";
        sprite.u16(frame.size());
        sprite.b.insert(sprite.b.end(), frame.begin(), frame.end());
        dir.children["sprites"].files["lensflare32.spr"] = sprite.b;
        for (auto name : {"flarepistol_pv.abc", "lh_flarepistol_pv.abc"})
            dir.children["models"].children["weapons"].files[name] = test_model::abc(true).b;
        dir.children["models"].children["powerups"].files["flarepistol_pu.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["weapons"].files["c_flare_pv_t.dtx"] = {1, 2, 3};
        dir.children["skins"].children["powerups"].files["flarepistol_pu.dtx"] = {1, 2, 3};
        for (auto name : {"fire.wav", "alt.wav", "projectile.wav", "impact.wav"})
            dir.children["sounds"].children["weapons"].children["flare"].files[name] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        auto file = root / "flare.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        return test_naga::mount(g.resources, root, error) && g.resources.mount(file, error);
    }
    inline bool setup(GameSession& g, const std::filesystem::path& root)
    {
        return test_player::setup(g, root) && mount(g, root);
    }
    template <class T = CFlareProjectile>
    T* projectile(GameSession& g, HOBJECT owner, DVector pos, DVector dir = {0, 0, 1}, float speed = 2000,
                  int radius = 100)
    {
        const char* name = std::is_same_v<T, CFlareProjectile>      ? "CFlareProjectile"
                           : std::is_same_v<T, CFlareAltProjectile> ? "CFlareAltProjectile"
                                                                    : "CFlareBurstProjectile";
        ObjectCreateStruct state;
        state.m_Pos = pos;
        legacy_copy(state.m_Name, name);
        auto* p = dynamic_cast<T*>(g.server.CreateObject(g.server.GetClass(name), &state));
        return p && p->Setup(dir, 3, 20, speed, radius, owner) ? p : nullptr;
    }
    inline bool equip(GameSession& g)
    {
        auto* p = g.server_shell.Player(g.local_client);
        if (!p)
            return false;
        auto& inv = p->m_InventoryMgr;
        if (inv.ObtainWeapon(3, 2) != InventoryResult::Ok || inv.ObtainWeapon(3, 2) != InventoryResult::Ok)
            return false;
        inv.SetAmmoCount(4, 200);
        std::string error;
        return g.select_weapon(2, error);
    }
} // namespace test_flare
