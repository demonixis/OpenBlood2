#pragma once
#include "brush_fixture.h"
#include "game/original/Gideon.h"
#include "shield_fixture.h"

namespace test_gideon
{
    struct Actor : Gideon
    {
        std::vector<std::string> fired_nodes;
        bool Fire(bool alternate = false) override
        {
            fired_nodes.push_back(m_szFireNode);
            return Gideon::Fire(alternate);
        }
        inline static unsigned destroyed = 0;
        inline static std::uint64_t final_extra = 0, final_powerup = 0;
        ~Actor() override
        {
            ++destroyed;
            final_extra = extra_calls;
            final_powerup = powerup_calls;
        }
    };
    // Test-only class registration. This is not the production Gideon lifecycle.
    inline Actor* actor(OpenBlood2::GameSession& g, bool retail = false, DVector position = {0, 10, 0})
    {
        using namespace OpenBlood2;
        static ClassInfo type{
            "DiagnosticGideon", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Actor>(); }, {}};
        type.parent = g.objects.find_class("Gideon");
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = position;
        state.m_Flags = FLAG_VISIBLE | FLAG_MODELKEYS;
        if (retail)
        {
            SAFE_STRCPY(state.m_Filename, "Models/Enemies/gideon.abc");
            SAFE_STRCPY(state.m_SkinName, "Skins/Enemies/gideon.dtx");
        }
        auto h = g.objects.create(&type, state);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<Actor*>(r->game.get()) : nullptr;
        if (!a || !a->m_InventoryMgr.Init(h, nullptr) || !a->m_damage.Init(h, &a->m_InventoryMgr) ||
            !a->m_MoveObj.Init(h))
            return nullptr;
        auto table = std::make_shared<CAnim_Sound>();
        if (retail)
        {
            if (!r->model || !table->SetAnimationIndexes(h) || !table->GenerateHitSpheres(h))
                return nullptr;
        }
        else
        {
            auto model = std::make_shared<Model>();
            std::string error;
            if (!read_model(test_model::abc().b, *model, error))
                return nullptr;
            model->nodes[1].name = "rr_gun";
            for (auto& clip : model->animations)
            {
                clip.keys = {{0, {}, {}, ""}, {100, {}, {}, ""}};
                clip.dimensions = {1, 1, 1};
                for (auto& n : clip.nodes)
                    for (auto& f : n.frames)
                        f.position = {};
            }
            r->model = model;
            r->dimensions = {1, 1, 1};
        }
        if (!table->SetSoundRoot("sounds/enemies/Gideon") || !a->InitStatics(table))
            return nullptr;
        g.server.SetModelAnimation(h, 0);
        a->m_damage.SetHitPoints(4000);
        a->m_damage.SetArmorPoints(200);
        a->m_vTargetPos = {0, position.y, position.z + 500};
        for (auto id : {WEAP_GIDEON_SHIELD, WEAP_ZEALOT_ENERGYBLAST, WEAP_NAGA_EYEBEAM, WEAP_GIDEON_WIND})
            if (a->m_InventoryMgr.ObtainWeapon(id) != InventoryResult::Ok)
                return nullptr;
        return a;
    }
    inline bool key(OpenBlood2::GameSession& g, Gideon& a, const char* text)
    {
        char* argv[]{const_cast<char*>(text)};
        ArgList args{argv, 1};
        return g.objects.engine_message(a.m_hObject, MID_MODELSTRINGKEY, &args);
    }
    inline bool floor(OpenBlood2::GameSession& g)
    {
        auto world = test_brush::box("floor", {-10000, -20, -10000}, {10000, -10, 10000});
        for (auto& s : world.surfaces)
            s.flags = 1;
        std::string error;
        g.server.collision_ready = g.collision.build(world, error);
        return g.server.collision_ready;
    }
} // namespace test_gideon
