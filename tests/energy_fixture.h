#pragma once
#include "game/original/Projectile.h"
#include "naga_fixture.h"
namespace test_energy
{
    inline bool mount(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        if (!test_naga::mount(resources, root, error))
            return false;
        test_player::Directory dir;
        test_model::Writer sprite;
        for (auto n : {1u, 15u, 1u, 1u, 0u})
            sprite.u32(n);
        std::string frame = "alpha.dtx";
        sprite.u16(frame.size());
        sprite.b.insert(sprite.b.end(), frame.begin(), frame.end());
        dir.children["sprites"].files["yellowflare2.spr"] = sprite.b;
        dir.children["sounds"].children["enemies"].children["zealot"].files["distfire.wav"] = test_audio::wave();
        dir.children["sounds"].children["enemies"].children["gideon"].files["distfire.wav"] = test_audio::wave();
        dir.children["sounds"].children["weapons"].children["tesla"].files["impact.wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        auto file = root / "energy.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return resources.mount(file, error);
    }
    struct Actor : BaseClass
    {
        CDestructable damage;
        CInventoryMgr inventory;
        HOBJECT remove_on_damage = nullptr, remove_on_touch = nullptr;
        unsigned touches = 0;
        HOBJECT last_sender = nullptr;
        Actor()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD m, void* d, float f) override
        {
            if (m == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject);
                damage.m_fHitPoints = damage.m_fMaxHitPoints = 1000;
            }
            if (m == MID_TOUCHNOTIFY)
            {
                ++touches;
                if (remove_on_touch)
                    GetServerDE()->RemoveObject(remove_on_touch);
            }
            return BaseClass::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD r) override
        {
            const auto result = BaseClass::ObjectMessageFn(sender, m, r);
            if (m == OpenBlood2::damage_message)
            {
                last_sender = sender;
                if (remove_on_damage)
                    GetServerDE()->RemoveObject(remove_on_damage);
            }
            return result;
        }
    };
    inline Actor* actor(OpenBlood2::GameSession& g, DVector pos, bool gideon = false, DDWORD flags = FLAG_SOLID)
    {
        static OpenBlood2::ClassInfo owner{
            "Gideon", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Actor>(); }, {}};
        static OpenBlood2::ClassInfo target{
            "EnergyTarget", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Actor>(); }, {}};
        owner.parent = target.parent = g.objects.find_class("BaseClass");
        auto& type = gideon ? owner : target;
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct state;
        state.m_Pos = pos;
        state.m_Flags = flags;
        auto h = g.objects.create(&type, state);
        if (auto* r = g.objects.get(h))
            r->dimensions = {2, 2, 2};
        return dynamic_cast<Actor*>(g.server.HandleToObject(h));
    }
    inline CEnergyBlast* equip(Actor* a)
    {
        if (!a || !a->inventory.Init(a->m_hObject, nullptr) ||
            a->inventory.ObtainWeapon(WEAP_ZEALOT_ENERGYBLAST, 0) != InventoryResult::Ok)
            return nullptr;
        return dynamic_cast<CEnergyBlast*>(a->inventory.CurrentWeapon());
    }
    inline CEnergyBlastProjectile* projectile(OpenBlood2::GameSession& g, HOBJECT owner, DVector pos,
                                              DVector dir = {0, 0, 1}, float speed = 1000, int radius = 175)
    {
        ObjectCreateStruct state;
        state.m_Pos = pos;
        auto h = g.objects.create(g.objects.find_class("CEnergyBlastProjectile"), state);
        auto* p = dynamic_cast<CEnergyBlastProjectile*>(g.server.HandleToObject(h));
        return p && p->Setup(dir, 27, 40, speed, radius, owner) ? p : nullptr;
    }
} // namespace test_energy
