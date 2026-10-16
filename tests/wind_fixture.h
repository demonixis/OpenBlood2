#pragma once
#include "energy_fixture.h"
#include "explosion_model_fixture.h"
namespace test_wind
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        if (!test_energy::mount(resources, root, error) || !test_explosion_model::mount(resources, root, error))
            return false;
        test_player::Directory dir;
        std::vector<std::uint8_t> skin;
        if (!resources.read("alpha.dtx", skin, error))
            return false;
        dir.children["skins"].children["explosions"].files["bmothball.dtx"] = skin;
        dir.children["sounds"].children["enemies"].children["gideon"].files["windfire.wav"] = test_audio::wave();
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
        auto file = root / "wind.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
    inline CGideonWind* equip(test_energy::Actor* actor)
    {
        if (!actor || !actor->inventory.Init(actor->m_hObject, nullptr) ||
            actor->inventory.ObtainWeapon(WEAP_GIDEON_WIND, 0) != InventoryResult::Ok)
            return nullptr;
        return dynamic_cast<CGideonWind*>(actor->inventory.CurrentWeapon());
    }
    inline CBehemothShockwaveProjectile* fire(OpenBlood2::GameSession& g, CGideonWind* weapon, bool alt = false)
    {
        if (!weapon || weapon->Fire({{0, 50, 0}, {0, 0, 1}, 42, WEAP_GIDEON_WIND, alt, false}) != FireResult::Fired)
            return nullptr;
        return dynamic_cast<CBehemothShockwaveProjectile*>(g.server.HandleToObject(weapon->projectile));
    }
} // namespace test_wind
