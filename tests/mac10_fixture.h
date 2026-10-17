#pragma once
#include "audio_fixture.h"
#include "camera_fixture.h"
#include "game/original/HandWeaponModel.h"
#include "game/original/WeaponPickups.h"
#include <algorithm>
namespace test_mac10
{
    using namespace OpenBlood2;
    inline bool setup(GameSession& g, const std::filesystem::path& root)
    {
        if (!test_player::setup(g, root))
            return false;
        test_player::Directory dir;
        for (auto name : {"mac10_pv.abc", "lh_mac10_pv.abc"})
        {
            auto bytes = test_model::abc(true).b;
            if (std::string_view(name).starts_with("lh_"))
            {
                // Length-prefixed alt_fire name, distinct from start_alt_fire/end_alt_fire.
                const std::string needle("\x08\0alt_fire", 10), replacement("\x08\0no__fire", 10);
                auto it = std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end());
                if (it == bytes.end())
                    return false;
                std::copy(replacement.begin(), replacement.end(), it);
            }
            dir.children["models"].children["weapons"].files[name] = std::move(bytes);
        }
        dir.children["skins"].children["weapons"].files["c_mac10_pv_t.dtx"] = {1, 2, 3};
        dir.children["models"].children["powerups"].files["mac10_pu.abc"] = test_model::abc(true).b;
        dir.children["skins"].children["powerups"].files["mac10_pu.dtx"] = {1, 2, 3};
        for (auto name : {"fire.wav", "alt.wav"})
            dir.children["sounds"].children["weapons"].children["mac10"].files[name] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        const auto path = root / "mac10.rez";
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        f.close();
        std::string error;
        return g.resources.mount(path, error) && g.enter_player(error);
    }
    inline bool equip(GameSession& g, bool dual = true)
    {
        auto* p = g.server_shell.Player(g.local_client);
        if (!p)
            return false;
        auto& inv = p->m_InventoryMgr;
        std::string error;
        if (inv.ObtainWeapon(WEAP_SUBMACHINEGUN, 2) != InventoryResult::Ok ||
            (dual && inv.ObtainWeapon(WEAP_SUBMACHINEGUN, 2) != InventoryResult::Ok))
            return false;
        inv.SetAmmoCount(AMMO_BULLET, 200);
        return g.select_weapon(2, error);
    }
    inline bool fire_modes(GameSession& g)
    {
        auto* p = g.server_shell.Player(g.local_client);
        if (!p)
            return false;
        auto& inv = p->m_InventoryMgr;
        for (bool left : {false, true})
            for (bool alt : {false, true})
            {
                g.client_shell.weapon_input = {};
                for (unsigned i = 0; i < 100; ++i)
                    if (!g.step(.01))
                        return false;
                auto* w = inv.CurrentWeapon(left);
                if (!w)
                    return false;
                const auto before = w->shots, keys = g.client_shell.weapons[left ? 1 : 0].fire_keys;
                const auto ammo = inv.GetAmmoCount(1);
                g.client_shell.weapon_input = {!alt, alt, left};
                for (unsigned i = 0; i < 180; ++i)
                    if (!g.step(.01))
                        return false;
                const auto delta = w->shots - before;
                std::cout << "Mac10 " << (left ? "left" : "right") << ' ' << (alt ? "alternate" : "primary")
                          << " shots=" << delta << " keys=" << g.client_shell.weapons[left ? 1 : 0].fire_keys - keys
                          << " HP=" << p->m_damage.m_fHitPoints << " dead=" << p->m_damage.m_bDead
                          << " state=" << g.client_shell.weapons[left ? 1 : 0].state << '\n';
                if (delta < 2 || delta != g.client_shell.weapons[left ? 1 : 0].fire_keys - keys ||
                    inv.GetAmmoCount(1) != ammo - delta || w->sounds_played != w->shots || w->sound_failures)
                    return false;
                if (w->last_shot.request.alternate != alt || w->last_shot.request.left != left)
                    return false;
            }
        g.client_shell.weapon_input = {};
        for (unsigned i = 0; i < 100; ++i)
            if (!g.step(.01))
                return false;
        const auto right = inv.CurrentWeapon()->shots, left = inv.CurrentWeapon(true)->shots;
        for (unsigned i = 0; i < 100; ++i)
            if (!g.step(.01))
                return false;
        return inv.CurrentWeapon()->shots == right && inv.CurrentWeapon(true)->shots == left;
    }
} // namespace test_mac10
