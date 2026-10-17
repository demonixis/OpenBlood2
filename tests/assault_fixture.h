#pragma once
#include "audio_fixture.h"
#include "camera_fixture.h"
#include "game/original/Grenade.h"
#include "game/original/HandWeaponModel.h"
#include "game/original/WeaponPickups.h"
#include "player_fixture.h"
namespace test_assault
{
    using namespace OpenBlood2;
    inline bool setup(GameSession& g, const std::filesystem::path& root)
    {
        if (!test_player::setup(g, root))
            return false;
        test_player::Directory dir;
        dir.children["models"].children["weapons"].files["m16_pv.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["weapons"].files["c_m16_pv_t.dtx"] = {1, 2, 3};
        dir.children["models"].children["powerups"].files["m16_pu.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["powerups"].files["m16_pu.dtx"] = {1, 2, 3};
        dir.children["models"].children["ammo"].files["grenade.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["ammo"].files["grenade.dtx"] = {1, 2, 3};
        for (auto name : {"fire.wav", "alt.wav", "projectile.wav", "altbounce.wav"})
            dir.children["sounds"].children["weapons"].children["assault"].files[name] = test_audio::wave();
        dir.children["sounds"].children["weapons"].children["c4"].files["explosion_1.wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        auto file = root / "assault.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        return g.resources.mount(file, error) && g.enter_player(error);
    }
    inline bool equip(GameSession& g)
    {
        auto* p = g.server_shell.Player(g.local_client);
        if (!p || p->m_InventoryMgr.ObtainWeapon(WEAP_ASSAULTRIFLE, 2) != InventoryResult::Ok)
            return false;
        p->m_InventoryMgr.SetAmmoCount(1, 200);
        p->m_InventoryMgr.SetAmmoCount(5, 100);
        std::string error;
        if (!g.select_weapon(2, error))
        {
            std::cerr << "M16 equip: " << error << "\n";
            return false;
        }
        return true;
    }
    inline CGrenade* grenade(GameSession& g, DVector position, DVector direction = {0, 0, 1}, float speed = 100,
                             int radius = 200)
    {
        ObjectCreateStruct state;
        state.m_Pos = position;
        SAFE_STRCPY(state.m_Name, "M16Grenade");
        auto* p = dynamic_cast<CGrenade*>(g.server.CreateObject(g.server.GetClass("CGrenade"), &state));
        if (!p || !p->Setup(direction, 9, 120, speed, radius, g.client_shell.player))
        {
            std::cerr << "Grenade creation/setup: " << (p ? p->error : "no instance") << " diagnostics=";
            for (const auto& d : g.objects.diagnostics())
                std::cerr << d << ';';
            std::cerr << '\n';
            return nullptr;
        }
        return p;
    }
    inline bool fire_modes(GameSession& g)
    {
        auto* p = g.server_shell.Player(g.local_client);
        auto& inv = p->m_InventoryMgr;
        auto* w = inv.CurrentWeapon();
        // Move away from authored actors/doors for this labelled input/animation check.
        // Separate tests prove combat, contacts and actual pickup creation.
        DVector position{0, 800, 0};
        if (g.server.TeleportObject(p->m_hObject, &position) != DE_OK)
            return false;
        p->m_damage.m_bGodMode = true;
        for (bool alt : {false, true})
        {
            g.client_shell.weapon_input = {};
            for (unsigned i = 0; i < 100; ++i)
                if (!g.step(.01))
                {
                    for (const auto& t : g.trace)
                        std::cerr << t << "\n";
                    for (const auto& d : g.objects.diagnostics())
                        std::cerr << d << "\n";
                    return false;
                }
            const auto shots = w->shots, keys = g.client_shell.weapons[0].fire_keys;
            const auto bullets = inv.GetAmmoCount(1), charges = inv.GetAmmoCount(5);
            g.client_shell.weapon_input = {!alt, alt, false};
            for (unsigned i = 0; i < 140; ++i)
                if (!g.step(.01))
                {
                    for (const auto& t : g.trace)
                        std::cerr << t << "\n";
                    for (const auto& d : g.objects.diagnostics())
                        std::cerr << d << "\n";
                    return false;
                }
            const auto count = w->shots - shots;
            std::cout << "M16 " << (alt ? "alternate" : "primary") << " shots=" << count
                      << " keys=" << g.client_shell.weapons[0].fire_keys - keys << '\n';
            if (!count || count != g.client_shell.weapons[0].fire_keys - keys ||
                inv.GetAmmoCount(1) != bullets - (alt ? 0 : count) ||
                inv.GetAmmoCount(5) != charges - (alt ? count * 5 : 0) || w->sounds_played != w->shots ||
                w->sound_failures)
                return false;
        }
        g.client_shell.weapon_input = {};
        for (unsigned i = 0; i < 450; ++i)
            if (!g.step(.01))
                return false;
        const auto shots = w->shots;
        for (unsigned i = 0; i < 100; ++i)
            if (!g.step(.01))
                return false;
        return w->shots == shots;
    }
} // namespace test_assault
