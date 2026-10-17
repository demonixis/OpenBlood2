#pragma once
#include "energy_fixture.h"
namespace test_shield
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        if (!test_sprite_view::mount(resources, root, error))
            return false;
        std::vector<std::uint8_t> image;
        if (!resources.read("alpha.dtx", image, error))
            return false;
        test_player::Directory dir;
        dir.children["spritetextures"].files["lensflare_1.dtx"] = image;
        dir.children["spritetextures"].files["smoke64_2.dtx"] = image;
        dir.children["sounds"].children["enemies"].children["gideon"].files["shield.wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        auto file = root / "shield.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
    inline CGideonShield* equip(test_energy::Actor* a)
    {
        if (!a || !a->inventory.Init(a->m_hObject, nullptr) ||
            a->inventory.ObtainWeapon(WEAP_GIDEON_SHIELD, 0) != InventoryResult::Ok)
            return nullptr;
        return dynamic_cast<CGideonShield*>(a->inventory.CurrentWeapon());
    }
    inline OpenBlood2::WeaponFire shot(bool alt = false)
    {
        return {{0, 50, 0}, {0, 0, 1}, 42, WEAP_GIDEON_SHIELD, alt, false};
    }
} // namespace test_shield
