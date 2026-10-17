#include "brush_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CameraObj.h"
#include "game/original/DestructableBrush.h"
#include "game/server_adapter.h"
#include "model_fixture.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool near(float a, float b)
    {
        return std::abs(a - b) < .001f;
    }
    bool setup(GameSession& g)
    {
        CHECK(g.objects.find_class("BaseClass") || g.initialize());
        auto floor = test_brush::box("floor", {-10000, -20, -10000}, {10000, -10, 10000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    struct Probe : AI_Mgr
    {
        bool scheduled = false;
        int scheduled_script = 0;
        bool scheduled_ok = true;
        int script_fault = 0; // Labelled branch/callback probes; ordinary tests use the real Fire service.
        bool Fire(bool alternate = false) override
        {
            if (!script_fault)
                return AI_Mgr::Fire(alternate);
            if (script_fault == 1 || script_fault == 2)
            {
                fire_result = script_fault == 1 ? FireResult::Blocked : FireResult::Unavailable;
                return false;
            }
            if (script_fault == 3)
                GetServerDE()->RemoveObject(m_hObject);
            else if (script_fault == 4)
                Script_Fire_Run(); // Deliberate reentry while the caller owns the motion guard.
            else
                PrepareScript("SCRIPT; WAIT 1"); // Replace a directly driven command during its shot.
            return true;
        }
        inline static unsigned destroyed = 0;
        inline static std::uint64_t final_calls = 0, final_shots = 0;
        ~Probe() override
        {
            final_calls = fire_calls;
            final_shots = fire_shots;
            ++destroyed;
        }
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_UPDATE && scheduled)
            {
                GetServerDE()->SetNextUpdate(m_hObject, .001f);
                bool engaged = false;
                scheduled_ok = scheduled_script == 1   ? Script_Fire_Stand(engaged)
                               : scheduled_script == 2 ? Script_Fire_Walk()
                               : scheduled_script == 3 ? Script_Fire_Run()
                                                       : MC_Fire_Stand();
                if (!scheduled_ok)
                    GetServerDE()->SetNextUpdate(m_hObject, 0);
            }
            return AI_Mgr::EngineMessageFn(m, d, f);
        }
    };
    Probe* actor(GameSession& g)
    {
        static const ClassInfo type{"AIFireProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = {0, 10, 0};
        s.m_Flags = FLAG_MODELKEYS;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<Probe*>(r->game.get()) : nullptr;
        if (!ai || !ai->m_InventoryMgr.Init(h, nullptr))
            return nullptr;
        auto model = std::make_shared<Model>();
        std::string error;
        if (!read_model(test_model::abc().b, *model, error))
            return nullptr;
        model->nodes[1].name = "r_gun";
        for (auto& a : model->animations)
        {
            for (auto& key : a.keys)
                key.text.clear();
            a.dimensions = {1, 1, 1};
            for (auto& n : a.nodes)
                for (auto& f : n.frames)
                    f.position = {};
        }
        r->model = model;
        g.server.SetModelAnimation(h, 0);
        r->dimensions = {1, 1, 1};
        auto table = std::make_shared<CAnim_Sound>();
        ai->InitStatics(table);
        ai->m_vTargetPos = {0, 10, 100};
        return ai;
    }
    struct Target : BaseClass
    {
        CDestructable damage;
        bool remove = false;
        Target()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject);
                damage.m_fHitPoints = damage.m_fMaxHitPoints = 1000;
            }
            return BaseClass::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD read) override
        {
            const auto result = BaseClass::ObjectMessageFn(sender, m, read);
            if (remove && m == damage_message)
            {
                g_pServerDE->RemoveObject(sender);
                g_pServerDE->RemoveObject(m_hObject);
            }
            return result;
        }
    };
    Target* target(GameSession& g, DVector pos = {0, 10, 100}, DVector dims = {5, 10, 5})
    {
        static const ClassInfo type{"AIFireTarget",
                                    g.objects.find_class("BaseClass"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Target>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, s);
        g.server.SetObjectDims(h, &dims);
        return dynamic_cast<Target*>(g.server.HandleToObject(h));
    }
    bool armed(Probe* a, DBYTE type = WEAP_BERETTA)
    {
        CHECK(a);
        CHECK(a->m_InventoryMgr.ObtainWeapon(type, 0) == InventoryResult::Ok);
        a->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 100);
        return true;
    }
    Probe* script_actor(GameSession& g, DVector pos = {0, 10, 0}, bool cabal = true)
    {
        auto* a = actor(g);
        if (!a)
            return nullptr;
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Pos = pos;
        r->state.m_Flags |= FLAG_SOLID;
        a->m_bCabal = cabal;
        a->m_damage.Init(a->m_hObject);
        a->m_damage.m_fHitPoints = a->m_damage.m_fMaxHitPoints = 1000;
        a->m_fSeeingDist = 1000;
        return a->InsertMe() && a->m_MoveObj.Init(a->m_hObject) ? a : nullptr;
    }
    bool script_stand(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = script_actor(g);
        auto* enemy = script_actor(g, {0, 10, 100}, false);
        CHECK(a && enemy);
        g.objects.get(enemy->m_hObject)->dimensions = {10, 10, 5};
        bool engaged = true;
        CHECK(a->Script_Fire_Stand(engaged) && !engaged && a->fire_calls == 0);
        CHECK(armed(a));
        a->Metacmd = 5;
        CHECK(a->Move({0, 2, 3}, 1e-11f));
        CHECK(a->Script_Fire_Stand(engaged) && engaged && a->Metacmd == 5 && a->m_nCurMetacmd == 7);
        CHECK(!g.server.GetModelLooping(a->m_hObject) && near(g.objects.get(a->m_hObject)->velocity.z, 3));
        CHECK(g.objects.advance(a->m_fAttackLoadTime) && a->Script_Fire_Stand(engaged) && a->fire_calls == 0);
        CHECK(g.objects.advance(.001) && a->Script_Fire_Stand(engaged) && a->fire_shots == 1 && a->Metacmd == 6);
        CHECK(enemy->m_damage.m_fHitPoints < 1000 && a->m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == 99);
        a->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 0);
        CHECK(g.objects.advance(1) && a->Script_Fire_Stand(engaged) && a->fire_dry == 1 && a->m_bAnimating);
        // No opportunity is a valid decision, including after a previous target.
        g.objects.get(enemy->m_hObject)->state.m_Pos = {0, 10, 2000};
        const auto phase = a->Metacmd;
        CHECK(a->Script_Fire_Stand(engaged) && !engaged && !a->GetTarget() && a->Metacmd == phase);
        CHECK(g.unload_world() && !AI_Mgr::m_dwNumCabal && !AI_Mgr::m_dwNumMonster);
        return true;
    }
    bool script_moving(const char*)
    {
        for (bool run : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            auto* a = script_actor(g);
            CHECK(a && script_actor(g, {0, 10, 100}, false) && armed(a));
            a->m_fWalkSpeed = 20;
            a->m_fRunSpeed = 40;
            a->m_nInjuredLeg = 10;
            a->m_vTargetPos = {10, 10, 100}; // Source moving fire keeps this old position after reacquisition.
            auto tick = [&]() { return run ? a->Script_Fire_Run() : a->Script_Fire_Walk(); };
            CHECK(tick() && !a->script_fire_engaged && !a->perception_calls && g.server.GetModelLooping(a->m_hObject));
            CHECK(near(g.objects.get(a->m_hObject)->velocity.z, run ? 40 : 20));
            CHECK(a->m_nCurMetacmd == (run ? 9 : 8));
            a->Metacmd = 5;
            CHECK(g.objects.advance(a->m_fAttackLoadTime) && tick() && !a->fire_calls);
            CHECK(g.objects.advance(.001) && tick() && a->fire_shots == 1 && a->Metacmd == 6);
            CHECK(a->script_fire_engaged && near(a->face_position.x, 10) && !a->target_position_valid);
            CHECK(near(a->script_motion_velocity.x, 0)); // Previous cached forward, before facing.
            const auto phase = a->Metacmd;
            g.server.SetModelLooping(a->m_hObject, false);
            CHECK(g.server.advance_models(1));
            a->m_fLoadTimeStart = g.server.GetTime();
            CHECK(tick() && !a->m_bAnimating && a->Metacmd == phase &&
                  near(g.objects.get(a->m_hObject)->velocity.z, 0));
        }
        return true;
    }
    bool script_refusal(const char*)
    {
        for (bool moving : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            auto* a = script_actor(g);
            CHECK(a && script_actor(g, {0, 10, 100}, false) && armed(a));
            bool engaged = false;
            auto tick = [&]() { return moving ? a->Script_Fire_Walk() : a->Script_Fire_Stand(engaged); };
            CHECK(tick() && g.objects.advance(1));
            a->script_fault = 1;
            CHECK(tick() && a->m_bAnimating == moving && near(a->m_fLoadTimeStart, moving ? 1 : 0));
            a->m_bAnimating = false;
            CHECK(tick() && g.objects.advance(1));
            a->script_fault = 2;
            CHECK(!tick() && a->script_motion_outcome == AI_Mgr::MotionOutcome::Failed);
        }
        return true;
    }
    bool script_lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        HOBJECT old = nullptr;
        for (int fault : {3, 4, 5})
        {
            auto* a = script_actor(g);
            CHECK(a && !g.objects.get(old) && script_actor(g, {0, 10, 100}, false) && armed(a));
            old = a->m_hObject;
            bool engaged = false;
            CHECK(a->Script_Fire_Stand(engaged) && g.objects.advance(1));
            a->script_fault = fault;
            CHECK(!a->Script_Fire_Stand(engaged) && a->script_motion_outcome == AI_Mgr::MotionOutcome::Failed);
            CHECK(g.unload_world() && !g.objects.get(old) && !g.server.string_count() && !g.server.message_count());
            CHECK(setup(g));
        }
        auto* a = script_actor(g);
        CHECK(a && script_actor(g, {0, 10, 100}, false) && armed(a));
        bool engaged = false;
        g.server.collision_ready = false;
        CHECK(!a->Script_Fire_Stand(engaged) && !a->m_bAnimating);
        g.server.collision_ready = true;
        a->Metacmd = std::numeric_limits<int>::max();
        CHECK(!a->Script_Fire_Stand(engaged));
        a->Metacmd = 1;
        auto bad = std::make_shared<CAnim_Sound>();
        bad->m_nAnim_FIRE_RUN[TYPE_PISTOL] = UINT32_MAX;
        CHECK(a->InitStatics(bad) && !a->Script_Fire_Run() && a->m_nState != AI_Mgr::StateCrawlUnder);
        CHECK(g.objects.remove(a->m_hObject) && !a->Script_Fire_Run() && g.objects.finish_frame());
        return true;
    }
    bool script_private(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        HOBJECT old = nullptr;
        for (int mode : {1, 2, 3})
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error) && !g.objects.get(old));
            auto* a = script_actor(g, {0, 10000, 0});
            auto* enemy = script_actor(g, {0, 10000, 400}, false);
            CHECK(a && enemy && armed(a));
            enemy->m_damage.m_fHitPoints = enemy->m_damage.m_fMaxHitPoints = 10000;
            g.objects.get(enemy->m_hObject)->dimensions = {60, 100, 30};
            auto* body = g.objects.get(a->m_hObject);
            CHECK(g.models.load("models/enemies/m_cultist1.abc", body->model, error));
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject) && table->SetSoundRoot("sounds/enemies/m_cultist") &&
                  a->InitStatics(table));
            a->m_vTargetPos = {0, 10000, 400};
            a->m_fWalkSpeed = 20;
            a->m_fRunSpeed = 40;
            a->scheduled = true;
            a->scheduled_script = mode;
            g.server.SetNextUpdate(a->m_hObject, .001f);
            for (unsigned i = 0; i < 240; ++i)
                CHECK(g.step(1. / 60));
            std::cout << "Script fire mode=" << mode << " shots=" << a->fire_shots
                      << " target HP=" << enemy->m_damage.m_fHitPoints << " position Z=" << body->state.m_Pos.z
                      << " failures=" << a->script_motion_failures << " error=" << a->script_motion_error << '\n';
            CHECK(a->scheduled_ok && a->fire_shots > 2 && enemy->m_damage.m_fHitPoints < 10000 &&
                  !a->script_motion_failures && !a->fire_failures);
            CHECK(mode == 1 ? near(body->state.m_Pos.z, 0) : body->state.m_Pos.z > 60);
            old = a->m_hObject;
            CHECK(g.unload_world() && !g.objects.get(old) && !g.server.string_count() && !g.server.message_count() &&
                  !g.server.client_count());
        }
        return true;
    }
    bool script_routes(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = script_actor(g);
        auto* enemy = script_actor(g, {0, 10, 100}, false);
        CHECK(a && enemy && armed(a));
        ObjectCreateStruct s;
        s.m_Pos = {0, 10, 200};
        CHECK(g.objects.create(g.objects.find_class("PathPoint"), s, Creation::World,
                               {{"Name", std::string("opportunity1")}, {"Pos", s.m_Pos}}));
        CHECK(g.server_shell.paths.BuildPathList() && a->PrepareScript("SCRIPT 4; FOLLOWPATH opportunity"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->script_fire_engaged && a->m_nCurMetacmd == 7 &&
              a->velocity_commands == 0);
        CHECK(g.objects.advance(.5) && a->AI_STATE_Script(g.server_shell.paths) && a->fire_shots == 1 &&
              !a->path_arrivals);
        g.objects.get(enemy->m_hObject)->state.m_Pos = {0, 10, 2000};
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && !a->script_fire_engaged && a->m_nCurMetacmd == 1 &&
              a->velocity_commands == 1);
        SAFE_STRCPY(g.objects.get(enemy->m_hObject)->state.m_Name, "moving goal");
        g.objects.get(enemy->m_hObject)->state.m_Pos = {0, 10, 100};
        CHECK(a->SetMoveToObjectCmd("moving goal") && a->SetSetMovementCmd("RUN"));
        CHECK(a->UpdateMoveToObjectCmd() && a->Metacmd == 2);
        CHECK(a->UpdateMoveToObjectCmd() && a->Metacmd == 3);
        CHECK(a->UpdateMoveToObjectCmd() && a->Metacmd == 2 && a->m_nCurMetacmd == 9);
        CHECK(g.objects.advance(.5) && a->UpdateMoveToObjectCmd() && a->UpdateMoveToObjectCmd());
        CHECK(a->Metacmd == 2 && a->script_fire_engaged && a->script_motion_failures == 0);
        return true;
    }
    bool selection(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto& i = a->m_InventoryMgr;
        CWeapon* w = nullptr;
        CHECK(i.FindBestWeapon(100, w) && !w);
        CHECK(i.ObtainWeapon(WEAP_BERETTA, 1) == InventoryResult::Ok);
        i.SetAmmoCount(AMMO_BULLET, 2);
        CHECK(i.FindBestWeapon(3500, w) && !w);
        CHECK(i.FindBestWeapon(0, w) && !w);
        CHECK(i.FindBestWeapon(3499, w) && w == i.GetWeapon(1));
        CHECK(!i.FindBestWeapon(-1, w) && !w);
        CHECK(a->MC_BestWeapon() && a->m_bAnimating && a->Metacmd == 1);
        CHECK(g.server.advance_models(1) && a->MC_BestWeapon() && i.CurrentWeapon() == i.GetWeapon(1) &&
              a->m_bAnimating);
        CHECK(g.server.advance_models(1) && a->MC_BestWeapon() && !a->m_bAnimating && a->Metacmd == 1);
        CHECK(a->MC_BestWeapon() && a->Metacmd == 2);
        CHECK(i.FindBestWeapon(8000, w) && !w); // Retains unsuitable current weapon.
        i.SetAmmoCount(AMMO_BULLET, 0);
        CHECK(a->MC_BestWeapon() && a->m_nState == AI_Mgr::StateFindAmmo);
        CHECK(i.ObtainWeapon(WEAP_MELEE, 0) == InventoryResult::Ok);
        CHECK(i.ChangeWeapon(WEAP_MELEE) == InventoryResult::Ok);
        CHECK(i.FindBestWeapon(50, w) && !w);
        CHECK(a->MC_BestWeapon() && a->m_nState == AI_Mgr::StateFindAmmo); // raw NONE pool
        i.SetAmmoCount(AMMO_BULLET, 5);
        CHECK(i.FindBestWeapon(100, w) && w == i.GetWeapon(1));
        CHECK(!a->MC_BestWeapon() && i.CurrentWeapon()->data.m_nType == WEAP_MELEE); // no switch column 4
        CHECK(i.ChangeWeapon(WEAP_BERETTA) == InventoryResult::Ok &&
              i.ChangeWeapon(WEAP_BERETTA) == InventoryResult::Current);
        CHECK(i.ChangeWeapon(255) == InventoryResult::Invalid);
        return true;
    }
    bool clearance(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* t = target(g);
        CHECK(t && a->SetNewTarget(t->m_hObject));
        DVector dir{0, 0, 1};
        CHECK(a->CheckClearShot({0, 10, 0}, dir) && a->clear_shot.outcome == AIClearOutcome::Target);
        g.server.SetObjectFlags(t->m_hObject, 0);
        CHECK(a->CheckClearShot({0, 10, 0}, dir) && a->clear_shot.outcome == AIClearOutcome::Clear);
        CHECK(!a->CanSee(t->m_hObject)); // Intentional source difference.
        g.server.SetObjectFlags(t->m_hObject, FLAG_RAYHIT);
        CHECK(a->CheckClearShot({0, 10, 0}, dir) && a->clear_shot.outcome == AIClearOutcome::Target);
        CHECK(target(g, {0, 10, 50}, {10, 20, 2}));
        CHECK(!a->CheckClearShot({0, 10, 0}, dir) && a->clear_shot.outcome == AIClearOutcome::Blocked);
        CHECK(!a->Fire() && a->fire_result == FireResult::Blocked && a->fire_blocked == 1 &&
              a->m_InventoryMgr.GetAmmoCount(1) == 100);
        g.server.collision_ready = false;
        CHECK(!a->Fire() && a->fire_result == FireResult::Unavailable && a->fire_failures == 1);
        return true;
    }
    bool filters(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* t = target(g);
        CHECK(t && a->SetNewTarget(t->m_hObject));
        auto model = test_brush::box("glass", {-20, 0, 45}, {20, 40, 55});
        g.world.models.push_back(std::move(model));
        auto h = g.objects.create(g.objects.find_class("CDestructableBrush"), {}, Creation::World,
                                  {{"Name", std::string("glass")},
                                   {"Flags", std::int32_t(FLAG_SOLID)},
                                   {"HitPoints", 100.f},
                                   {"SurfaceType", std::int32_t(SURFTYPE_GLASS)}});
        CHECK(h && a->Fire() && a->clear_shot.first_hit.object == t->m_hObject);
        const auto* weapon = a->m_InventoryMgr.CurrentWeapon();
        CHECK(weapon->impacts.size() == 2 && weapon->impacts[0].hit.object == h && weapon->impacts[0].continued &&
              weapon->impacts[0].damage_delivered && weapon->impacts[1].hit.object == t->m_hObject &&
              weapon->impacts[1].damage_delivered);
        auto* brush = dynamic_cast<CDestructableBrush*>(g.objects.get(h)->game.get());
        CHECK(brush && brush->m_damage.m_fHitPoints < 100 && t->damage.m_fHitPoints < 1000 &&
              a->m_InventoryMgr.GetAmmoCount(1) == 99); // One vector damages glass then target using one round.
        brush->destructable = false;
        CHECK(!a->Fire() && a->fire_result == FireResult::Blocked);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        auto volume = test_brush::box("liquid", {-20, 0, 45}, {20, 40, 55});
        g.world.models.push_back(std::move(volume));
        h = g.objects.create(g.objects.find_class("Water"), {}, Creation::World, {{"Name", std::string("liquid")}});
        CHECK(h);
        g.server.SetObjectFlags(h, FLAG_SOLID);
        DVector direction{0, 0, 1};
        CHECK(a->CheckClearShot({0, 10, 0}, direction) && a->clear_shot.first_hit.object == t->m_hObject);
        CHECK(g.objects.find_class(
            "CDestructableModel")); // Its filter behavior is covered by destructable_models.filters.
        return true;
    }
    bool head(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* t = target(g);
        CHECK(t && a->SetNewTarget(t->m_hObject));
        auto m = std::make_shared<Model>();
        std::string e;
        CHECK(read_model(test_model::abc().b, *m, e));
        m->nodes[1].name = "head";
        for (auto& clip : m->animations)
            for (unsigned n = 0; n < clip.nodes.size(); ++n)
                for (auto& f : clip.nodes[n].frames)
                    f.position = {0, n ? 8.f : 0, 0};
        auto* r = g.objects.get(t->m_hObject);
        r->model = m;
        r->state.m_ObjectType = OT_MODEL;
        CHECK(target(g, {0, 10, 50}, {10, 1, 2}));
        DVector dir{0, 0, 1};
        CHECK(a->CheckClearShot({0, 10, 0}, dir) && a->clear_shot.outcome == AIClearOutcome::Head &&
              a->clear_shot.rays == 2 && dir.y > 0);
        CHECK(a->m_vTargetPos.y == 18 && a->target_position_valid);
        g.server.SetObjectFlags(t->m_hObject, 0);
        dir = {0, 0, 1};
        a->m_vTargetPos = {};
        CHECK(!a->CheckClearShot({0, 10, 0}, dir) && a->clear_shot.head_result == SegmentResult::Miss &&
              a->m_vTargetPos.y == 18 && dir.y == 0);
        return true;
    }
    bool aim(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* t = target(g);
        CHECK(t && a->SetNewTarget(t->m_hObject));
        const bool fired = a->Fire();
        CHECK(fired && a->fire_gun_node && a->fire_shots == 1 && a->fire_origin.y == 10 &&
              t->damage.m_fHitPoints < 1000);
        auto* w = a->m_InventoryMgr.CurrentWeapon();
        CHECK(w->last_shot.damage_delivered && w->last_shot.hit.object == t->m_hObject);
        // A non-player shot remains close to the gun's requested forward, never a fake +36 eye ray.
        CHECK(std::abs(w->last_shot.direction.y) < .02f);
        a->m_InventoryMgr.SetAmmoCount(1, 0);
        CHECK(a->Fire(true) && a->fire_result == FireResult::Dry && a->fire_dry == 1 && a->fire_shots == 1);
        a->m_InventoryMgr.SetAmmoCount(1, 100);
        auto* r = g.objects.get(a->m_hObject);
        auto m = std::make_shared<Model>(*r->model);
        r->model = m;
        m->nodes[1].name = "not_gun";
        a->m_vTargetPos = {500, 80, 100};
        CHECK(a->Fire() && !a->fire_gun_node && a->fire_direction.x == 0 && a->fire_direction.y == 0 &&
              a->fire_direction.z == 1);
        CHECK(a->m_InventoryMgr.ObtainWeapon(WEAP_MELEE, 1) == InventoryResult::Ok &&
              a->m_InventoryMgr.ChangeWeapon(WEAP_MELEE) == InventoryResult::Ok);
        g.objects.get(t->m_hObject)->state.m_Pos = {0, 10, 50};
        CHECK(a->Fire() && a->fire_result == FireResult::Fired && a->m_InventoryMgr.GetAmmoCount(0) == 0);
        return true;
    }
    bool keys(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* t = target(g);
        CHECK(t && a->SetNewTarget(t->m_hObject));
        auto* r = g.objects.get(a->m_hObject);
        auto m = std::make_shared<Model>(*r->model);
        r->model = m;
        m->animations[0].keys = {{0, {}, {}, "[fire_key:1][UpVel:7]"}, {50, {}, {}, "[fire_key:1]"}, {100, {}, {}, ""}};
        g.server.SetModelLooping(a->m_hObject, false);
        CHECK(g.server.advance_models(.2));
        CHECK(a->fire_shots == 2 && a->model_keys == 2 && a->key_outcome == AIKeyOutcome::FireAttempt &&
              r->velocity.y == 7);
        CHECK(g.objects.remove(t->m_hObject));
        CHECK(g.server.ResetModelAnimation(a->m_hObject) == DE_OK);
        CHECK(g.server.advance_models(.2) && a->key_outcome == AIKeyOutcome::Invalid && a->fire_shots == 2);
        CHECK(g.objects.finish_frame() && !a->GetTarget());
        CHECK(g.server.ResetModelAnimation(a->m_hObject) == DE_OK);
        r->velocity.y = 0;
        CHECK(g.server.advance_models(.2) && a->no_target == 2 && r->velocity.y == 0);
        return true;
    }
    bool cadence(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* t = target(g);
        CHECK(t && a->SetNewTarget(t->m_hObject));
        CHECK(a->MC_Fire_Stand() && a->m_nCurMetacmd == 7 && a->m_bAnimating && a->fire_calls == 0);
        CHECK(g.objects.advance(a->m_fAttackLoadTime) && a->MC_Fire_Stand() && a->fire_calls == 0); // strict >
        CHECK(g.objects.advance(.001) && a->MC_Fire_Stand() && a->fire_shots == 1 &&
              near(a->m_fLoadTimeStart, g.server.GetTime()));
        CHECK(g.objects.advance(.1) && a->MC_Fire_Stand() && a->fire_shots == 1);
        CHECK(target(g, {0, 10, 50}, {10, 20, 2}));
        CHECK(g.objects.advance(1) && g.server.advance_models(1));
        const auto before = a->Metacmd;
        CHECK(a->MC_Fire_Stand() && !a->m_bAnimating && a->Metacmd == before + 2); // both branches
        CHECK(a->MC_Fire_Stand());
        g.server.collision_ready = false;
        CHECK(g.objects.advance(1) && !a->MC_Fire_Stand());
        CHECK(a->state_failures > 0);
        return true;
    }
    bool recovery(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* r = g.objects.get(a->m_hObject);
        auto model = std::make_shared<Model>(*r->model);
        r->model = model;
        model->animations[0].dimensions = {1, 5, 1};
        auto ceiling = test_brush::box("ceiling", {-100, 12, -100}, {100, 20, 100});
        for (auto& surface : ceiling.surfaces)
            surface.flags = 1;
        std::string error;
        CHECK(g.collision.build(ceiling, error));
        CHECK(a->MC_Fire_Stand() && a->animation_blocked && a->m_nState == AI_Mgr::StateCrawlUnder);
        CHECK(!a->m_bAnimating && r->dimensions.y == 1 && r->state.m_Pos.y == 10);
        CHECK(setup(g));
        auto table = std::make_shared<CAnim_Sound>();
        table->m_nAnim_FIRE_STAND[TYPE_PISTOL] = UINT32_MAX;
        CHECK(a->InitStatics(table));
        CHECK(!a->MC_Fire_Stand() && !a->animation_blocked && a->m_nState == AI_Mgr::StateCrawlUnder);
        table->m_nAnim_FIRE_STAND[TYPE_PISTOL] = 0;
        CHECK(a->MC_Fire_Stand());
        a->Metacmd = std::numeric_limits<int>::max();
        CHECK(!a->MC_Fire_Stand() && a->fire_calls == 0);
        a->Metacmd = 1;
        a->scheduled = true;
        g.server.SetNextUpdate(a->m_hObject, .001f);
        g.server.SetObjectState(a->m_hObject, OBJSTATE_INACTIVE);
        const auto time = r->animation.time;
        CHECK(g.step(.5) && r->animation.time == time && a->fire_calls == 0);
        g.server.SetObjectState(a->m_hObject, OBJSTATE_ACTIVE);
        CHECK(g.step(.1) && a->fire_calls > 0);
        r->model.reset();
        CHECK(!a->MC_Fire_Stand());
        return true;
    }
    bool failures(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && !a->Fire() && !a->fire_error.empty());
        CHECK(armed(a));
        a->m_vTargetPos = {0, 10, 0};
        CHECK(!a->Fire() && a->fire_result == FireResult::Invalid); // gun target zero length
        DVector direction{};
        CHECK(!a->CheckClearShot({0, 10, 0}, direction) && a->clear_shot.outcome == AIClearOutcome::Invalid);
        a->m_vTargetPos = {0, 10, 100};
        auto* r = g.objects.get(a->m_hObject);
        r->model.reset();
        CHECK(!a->MC_Fire_Stand());
        CHECK(g.objects.remove(a->m_hObject));
        CHECK(!a->Fire() && !a->MC_BestWeapon() && !a->MC_Fire_Stand());
        WeaponFire shot;
        shot.weapon = 1;
        shot.position = {0, 10, 0};
        shot.direction = {0, 0, 1};
        CHECK(a->m_InventoryMgr.CurrentWeapon()->Fire(shot) == FireResult::Invalid);
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        HOBJECT old = nullptr;
        for (int i = 0; i < 4; ++i)
        {
            CHECK(setup(g));
            auto* a = actor(g);
            CHECK(armed(a) && !g.objects.get(old));
            auto* t = target(g);
            CHECK(t && a->SetNewTarget(t->m_hObject));
            auto* r = g.objects.get(a->m_hObject);
            auto m = std::make_shared<Model>(*r->model);
            r->model = m;
            m->animations[0].keys = {{0, {}, {}, "[fire_key:1]"}, {50, {}, {}, "[fire_key:1]"}, {100, {}, {}, ""}};
            t->remove = true;
            Probe::destroyed = 0;
            old = a->m_hObject;
            CHECK(g.server.advance_models(.2));
            CHECK(Probe::destroyed == 1 && Probe::final_calls == 1 && Probe::final_shots == 1 && !g.objects.get(old));
            CHECK(g.objects.finish_frame() && Probe::destroyed == 1);
            CHECK(g.unload_world());
        }
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g));
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        for (const char* name : {"m_cultist1", "f_cultist1", "m_civilian1", "f_civilian1", "labtech", "gideon"})
        {
            auto* a = actor(g);
            CHECK(armed(a));
            auto* r = g.objects.get(a->m_hObject);
            r->state.m_Pos = {0, 10000, 0};
            CHECK(g.models.load(std::string("models/enemies/") + name + ".abc", r->model, error));
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject) && a->InitStatics(table));
            const auto clip = table->m_nAnim_FIRE_STAND[TYPE_PISTOL];
            const bool present = clip < r->model->animations.size();
            CHECK(a->MC_Fire_Stand() == present);
            std::cout << name << " pistol clip=" << clip << " available=" << present;
            if (present)
            {
                unsigned keys = 0;
                for (const auto& k : r->model->animations[clip].keys)
                    if (k.text.find("fire_key") != k.text.npos)
                        ++keys;
                std::cout << " duration=" << r->model->animations[clip].duration << " fire_keys=" << keys;
            }
            std::cout << "\n";
        }
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", e) && g.enter_player(e));
            CHECK(g.objects.find_class("CultistAI"));
            auto* a = actor(g);
            CHECK(armed(a) && !g.objects.get(old));
            auto* r = g.objects.get(a->m_hObject);
            r->state.m_Pos = {0, 10000, 0};
            CHECK(g.models.load("models/enemies/m_cultist1.abc", r->model, e));
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject) && table->SetSoundRoot("sounds/enemies/m_cultist") &&
                  a->InitStatics(table));
            auto* t = target(g, {0, 10000, 300}, {60, 100, 30});
            CHECK(t && a->SetNewTarget(t->m_hObject) && a->RefreshTargetPosition());
            a->scheduled = true;
            g.server.SetNextUpdate(a->m_hObject, .001f);
            for (int j = 0; j < 300; ++j)
                CHECK(g.step(1. / 60));
            std::cout << "cycle=" << cycle << " shots=" << a->fire_shots << " keys=" << a->model_keys
                      << " completed=" << a->animation_finishes << " audio=" << a->audio_starts
                      << " suppressed=" << a->camera_suppressed << " key_error=" << a->ai_key_error
                      << " fire_error=" << a->fire_error << " key_failures=" << a->key_failures
                      << " target_hp=" << t->damage.m_fHitPoints << "\n";
            CHECK(a->fire_shots > 4 && a->animation_finishes > 0 && a->fire_failures == 0 && a->state_failures == 0 &&
                  a->key_failures == 0 && a->audio_keys > 0 && a->camera_suppressed == a->audio_keys &&
                  a->audio_starts == 0 && t->damage.m_fHitPoints < 1000);
            const auto* camera = dynamic_cast<const CameraObj*>(g.server.HandleToObject(g_hActiveCamera));
            CHECK(camera && !camera->AllowPlayerMovement()); // Authored camera suppresses sounds, not firing.
            old = a->m_hObject;
            CHECK(g.unload_world() && !g.objects.get(old) && g.server.string_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{{"script_private", script_private},
                                                                        {"script_stand", script_stand},
                                                                        {"script_moving", script_moving},
                                                                        {"script_refusal", script_refusal},
                                                                        {"script_lifetime", script_lifetime},
                                                                        {"script_routes", script_routes},
                                                                        {"selection", selection},
                                                                        {"clearance", clearance},
                                                                        {"filters", filters},
                                                                        {"head", head},
                                                                        {"aim", aim},
                                                                        {"keys", keys},
                                                                        {"cadence", cadence},
                                                                        {"recovery", recovery},
                                                                        {"failures", failures},
                                                                        {"lifetime", lifetime},
                                                                        {"catalog", catalog},
                                                                        {"subway", subway}};
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
