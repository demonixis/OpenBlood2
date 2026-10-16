#include "brush_fixture.h"
#include "game/original/CultistAI.h"
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
        return std::abs(a - b) < .002f;
    }
    bool setup(GameSession& g)
    {
        CHECK(g.objects.find_class("BaseClass") || g.initialize());
        auto floor = test_brush::box("floor", {-1000, -20, -1000}, {1000, -10, 1000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    CultistAI* actor(GameSession& g, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{"AILocomotionProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<CultistAI>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = 0;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<CultistAI*>(r->game.get()) : nullptr;
        if (!a)
            return nullptr;
        auto model = std::make_shared<Model>();
        std::string error;
        if (!read_model(test_model::abc().b, *model, error))
            return nullptr;
        auto clip = model->animations.front();
        model->animations.clear();
        for (unsigned i = 0; i < 41; ++i)
        {
            model->animations.push_back(clip);
            auto& c = model->animations.back();
            c.name = "clip" + std::to_string(i);
            c.dimensions = {1, 2, 1};
            c.keys.clear();
        }
        for (auto& c : model->animations)
            for (auto& n : c.nodes)
                for (auto& f : n.frames)
                    f.position = {};
        r->model = model;
        r->dimensions = {1, 2, 1};
        auto table = std::make_shared<CAnim_Sound>();
        for (unsigned i = 0; i < 9; ++i)
        {
            table->m_nAnim_WALK[i] = i;
            table->m_nAnim_RUN[i] = 9 + i;
            table->m_nAnim_FIRE_WALK[i] = 22 + i;
            table->m_nAnim_FIRE_RUN[i] = 31 + i;
            table->m_nAnim_FIRE_STAND[i] = 40;
        }
        table->m_nAnim_WALK_INJURED_LLEG_PISTOL = 18;
        table->m_nAnim_WALK_INJURED_RLEG_PISTOL = 19;
        table->m_nAnim_WALK_INJURED_LLEG_RIFLE = 20;
        table->m_nAnim_WALK_INJURED_RLEG_RIFLE = 21;
        if (!a->InitStatics(table) || !a->m_MoveObj.Init(h))
            return nullptr;
        a->m_vDims = r->dimensions;
        if (!a->m_MoveObj.Update() || !a->m_InventoryMgr.Init(h, nullptr))
            return nullptr;
        a->m_damage.Init(h);
        a->m_damage.m_fHitPoints = a->m_damage.m_fMaxHitPoints = 100;
        return a;
    }
    HOBJECT object(GameSession& g, const char* name, DVector pos, DVector dims = {})
    {
        ObjectCreateStruct s;
        SAFE_STRCPY(s.m_Name, name);
        s.m_Pos = pos;
        s.m_Flags = dims.x ? FLAG_SOLID : 0;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), s);
        if (h)
            g.objects.get(h)->dimensions = dims;
        return h;
    }

    bool armed(CultistAI* a)
    {
        CHECK(a && a->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA, 0) == InventoryResult::Ok);
        a->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 100);
        return true;
    }
    bool obstruction(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        bool out;
        r->velocity = {3, 0, 0};
        CHECK(a->CheckObstructed({}, 30, out) && !out);
        r->velocity.x = 2.99f;
        CHECK(a->CheckObstructed({999, 0, 0}, 30, out) && out);
        r->velocity = {0, 3, 0};
        CHECK(a->CheckObstructed({}, 30, out) && !out);
        r->velocity = {};
        r->state.m_Pos.y += .02f;
        CHECK(a->m_MoveObj.Update() && a->CheckObstructed({}, 30, out) && !out);
        CHECK(a->m_MoveObj.Update() && a->CheckObstructed({}, 30, out) && out);
        CHECK(!a->CheckObstructed({}, -1, out) && !out);
        a->m_MoveObj.sampled = false;
        CHECK(!a->CheckObstructed({}, 30, out));
        return true;
    }
    bool rays(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto block = object(g, "upper right", {1, 11, 3}, {.2f, .2f, .2f});
        CHECK(block);
        CHECK(a->CalculateObstruction() == 0x11);
        CHECK(a->obstacle_rays[0].hit.object == block && a->obstacle_rays[1].result == SegmentResult::Miss);
        CHECK(near(a->obstacle_rays[0].from.x, 1) && near(a->obstacle_rays[0].from.y, 11));
        CHECK(near(a->obstacle_rays[8].from.x, -1) && near(a->obstacle_rays[8].from.y, 9));
        CHECK(near(a->obstacle_rays[4].to.z, (std::sqrt(2.f) + 1) * 2));
        auto* r = g.objects.get(block);
        r->state.m_Pos = {0, 10, 3};
        r->dimensions = {2, 2, .2f};
        CHECK(a->CalculateObstruction() == 0x77);
        // A first self hit is discarded; the original does not recast beyond it.
        g.objects.get(a->m_hObject)->state.m_Flags |= FLAG_SOLID;
        CHECK(a->CalculateObstruction() == 0 && a->obstacle_rays[4].hit.object == a->m_hObject);
        g.objects.get(a->m_hObject)->state.m_Flags = 0;
        CHECK(g.objects.remove(block) && g.objects.finish_frame());
        auto wall = test_brush::box("world wall", {-4, 6, 3}, {4, 14, 4});
        for (auto& s : wall.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(wall, error));
        CHECK(a->CalculateObstruction() == 0x77 && !a->obstacle_rays[4].hit.object &&
              a->obstacle_rays[4].hit.polygon != UINT32_MAX);
        g.server.collision_ready = false;
        CHECK(a->CalculateObstruction() == -1 && a->obstacle_rays[0].result == SegmentResult::Unavailable);
        return true;
    }
    bool navigate(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g, "obstacle", {0, 11, 3}, {2, .2f, .2f});
        CHECK(h);
        a->m_dwFlags = 3;
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        CHECK(a->NavigateObstacle() && a->m_nState == AI_Mgr::StateCrawlUnder);
        auto* b = g.objects.get(h);
        b->state.m_Pos.y = 9;
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->NavigateObstacle() &&
              a->m_nState == AI_Mgr::StateJumpOver);
        b->state.m_Pos.y = 10;
        b->dimensions.y = 2;
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->NavigateObstacle() &&
              a->m_nState == AI_Mgr::StateAttackClose);
        a->m_dwFlags = 0;
        CHECK(!a->NavigateObstacle() && a->locomotion_error.find("GetPointShade") != std::string::npos);
        CHECK(a->SetNewState(AI_Mgr::StateSearchVisual));
        a->Metacmd = 4;
        CHECK(a->NavigateObstacle() && a->Metacmd == 1 && a->m_nState == AI_Mgr::StateSearchVisual);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        a->m_nLastState = AI_Mgr::StateGuard;
        CHECK(a->NavigateObstacle() && a->m_nState == AI_Mgr::StateGuard);
        return true;
    }
    bool ledges(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        bool out;
        CHECK(a->IsLedge({0, 0, 2}, out) && !out && a->ledge_queries == 0);
        CHECK(g.objects.advance(.05));
        CHECK(a->IsLedge({0, 0, 2}, out) && out && a->ledge_sampled && a->ledge_queries == 1);
        CHECK(near(a->ledge_ray.from.z, std::sqrt(2.f) * 2.5f) && near(a->ledge_ray.to.y, 6));
        CHECK(a->IsLedge({0, 0, 2}, out) && !out && !a->ledge_sampled && a->ledge);
        CHECK(object(g, "support", {0, 7, 3.535534f}, {1, .5f, 1}));
        CHECK(g.objects.advance(.051) && a->IsLedge({0, 0, 2}, out) && !out);
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Pos = {0, -8, 0};
        CHECK(a->m_MoveObj.Update());
        CHECK(g.objects.advance(.051) && a->IsLedge({0, 0, 1}, out) && !out && !a->ledge_ray.hit.object);
        g.server.collision_ready = false;
        CHECK(g.objects.advance(.051) && !a->IsLedge({0, 0, 1}, out) && !a->ledge_sampled);
        const auto queries = a->ledge_queries;
        CHECK(!a->IsLedge({0, 0, 1}, out) && !a->ledge_sampled && a->ledge_queries == queries + 1);
        g.server.collision_ready = true;
        CHECK(a->IsLedge({0, 0, 1}, out) && !out && a->ledge_sampled && a->ledge_queries == queries + 2);
        return true;
    }
    bool walking(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g, {0, -8, 0});
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        a->m_fWalkSpeed = 30;
        a->m_fRunSpeed = 60;
        a->m_nInjuredLeg = 10;
        a->Metacmd = 7;
        CHECK(a->MC_Walk() && r->animation.animation == TYPE_MELEE && near(r->velocity.z, 10));
        CHECK(a->MC_Walk() && near(r->velocity.z, 10)); // no gravity => no support rewrite
        r->state.m_Flags = FLAG_GRAVITY;
        CHECK(a->MC_Walk() && near(r->velocity.z, 30));
        CHECK(armed(a));
        a->m_bAnimating = false;
        CHECK(a->MC_Walk() && r->animation.animation == 18 && near(r->velocity.z, 10));
        a->m_nInjuredLeg = 14;
        a->m_bAnimating = false;
        CHECK(a->MC_Walk() && r->animation.animation == 19);
        CHECK(a->MC_Run() && r->animation.animation == 9 + TYPE_PISTOL && near(r->velocity.z, 60));
        r->state.m_Flags = 0;
        g.server.SetModelLooping(a->m_hObject, DFALSE);
        CHECK(g.server.advance_models(1));
        CHECK(a->MC_Run() && !a->m_bAnimating && a->Metacmd == 8 && near(r->velocity.z, 0));
        auto table = std::make_shared<CAnim_Sound>();
        table->m_nAnim_RUN[TYPE_PISTOL] = 9999;
        CHECK(a->InitStatics(table) && !a->MC_Run() && !a->animation_blocked);
        return true;
    }
    bool physics(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g, {0, -8, 0});
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Flags = FLAG_SOLID | FLAG_GRAVITY;
        a->m_fRunSpeed = 60;
        CHECK(a->MC_Run());
        CHECK(g.step(.1) && near(r->state.m_Pos.z, 6)); // one generic integration, no command integration
        CHECK(a->MC_Run() && near(r->state.m_Pos.z, 6));
        CHECK(g.step(.1) && near(r->state.m_Pos.z, 12));
        // Foot-anchored clip growth blocked by an actual ceiling, not a missing clip.
        r->state.m_Flags = 0;
        CHECK(object(g, "ceiling", {0, -4, r->state.m_Pos.z}, {10, .5f, 10}));
        auto model = std::make_shared<Model>(*r->model);
        model->animations[TYPE_MELEE].dimensions.y = 5;
        r->model = model;
        CHECK(a->MC_Walk() && a->animation_blocked && a->m_nState == AI_Mgr::StateCrawlUnder &&
              near(r->dimensions.y, 2));
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 5 && r->animation.animation == 0 &&
              near(r->dimensions.y, 2)); // actual short crawl clip fits below the ceiling
        return true;
    }
    struct Victim : BaseClass
    {
        CDestructable damage;
        bool remove = false, recurse = false, recursive_ok = true;
        Victim()
        {
            AddAggregate(&damage);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD read) override
        {
            const auto result = BaseClass::ObjectMessageFn(sender, m, read);
            if (m == damage_message)
            {
                if (recurse)
                    recursive_ok = dynamic_cast<CultistAI*>(GetServerDE()->HandleToObject(sender))->MC_Fire_Run();
                if (remove)
                {
                    GetServerDE()->RemoveObject(sender);
                    GetServerDE()->RemoveObject(m_hObject);
                }
            }
            return result;
        }
    };
    Victim* victim(GameSession& g, CultistAI* a)
    {
        static const ClassInfo type{"LocomotionVictim",
                                    g.objects.find_class("BaseClass"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Victim>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_Pos = {0, 10, 100};
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* t = r ? dynamic_cast<Victim*>(r->game.get()) : nullptr;
        if (!t || !a->SetNewTarget(h) || !a->RefreshTargetPosition())
            return nullptr;
        r->dimensions = {20, 20, 5};
        t->damage.Init(h);
        t->damage.m_fHitPoints = t->damage.m_fMaxHitPoints = 1000;
        return t;
    }
    bool moving_fire(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        auto* t = victim(g, a);
        CHECK(t);
        auto* r = g.objects.get(a->m_hObject);
        a->m_fRunSpeed = 60;
        CHECK(a->MC_Fire_Run() && r->animation.animation == 31 + TYPE_PISTOL && a->m_fLoadTimeStart == 0 &&
              near(r->velocity.z, 60));
        CHECK(g.objects.advance(a->m_fAttackLoadTime) && a->MC_Fire_Run() && a->fire_shots == 0);
        CHECK(g.objects.advance(.001) && a->MC_Fire_Run() && a->fire_shots == 1 && t->damage.m_fHitPoints < 1000);
        CHECK(a->MC_Fire_Run() && a->fire_shots == 1);
        const auto meta = a->Metacmd;
        g.server.SetModelLooping(a->m_hObject, DFALSE);
        CHECK(g.server.advance_models(1));
        CHECK(a->MC_Fire_Run() && a->Metacmd == meta + 1 && !a->m_bAnimating && near(r->velocity.z, 0));
        CHECK(a->MC_Fire_Walk() && a->m_nCurMetacmd == 8 && r->animation.animation == 22 + TYPE_PISTOL);
        CHECK(object(g, "cover", {0, 10, 20}, {50, 50, 1}));
        CHECK(g.objects.advance(1));
        g.server.SetModelLooping(a->m_hObject, DFALSE);
        CHECK(g.server.advance_models(1));
        CHECK(a->MC_Fire_Walk() && a->Metacmd == meta + 3 && a->fire_result == FireResult::Blocked);
        // Unarmed moving fire selects pistol animation, then ordinary refusal advances once.
        auto* u = actor(g, {100, 10, 0});
        CHECK(u && u->MC_Fire_Run());
        CHECK(g.objects.get(u->m_hObject)->animation.animation == 31 + TYPE_PISTOL);
        CHECK(u->MC_Fire_Run() && !u->m_bAnimating);
        return true;
    }
    bool keys(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        CHECK(victim(g, a));
        auto* r = g.objects.get(a->m_hObject);
        auto model = std::make_shared<Model>(*r->model);
        model->animations[31 + TYPE_PISTOL].keys = {{0, {}, {}, ""}, {50, {}, {}, "[fire_key:1]"}, {100, {}, {}, ""}};
        r->model = model;
        r->state.m_Flags = FLAG_MODELKEYS;
        CHECK(a->MC_Fire_Run() && g.objects.advance(1));
        CHECK(g.server.advance_models(.06) && a->fire_shots == 1 && a->m_fLoadTimeStart == 0);
        CHECK(a->MC_Fire_Run() && a->fire_shots == 2 && near(a->m_fLoadTimeStart, 1));
        return true;
    }
    bool lifetime(const char*)
    {
        for (bool removal : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            auto* a = actor(g);
            CHECK(armed(a));
            auto* t = victim(g, a);
            CHECK(t);
            t->remove = removal;
            t->recurse = !removal;
            auto h = a->m_hObject, v = t->m_hObject;
            CHECK(a->MC_Fire_Run() && g.objects.advance(1));
            CHECK(!a->MC_Fire_Run() && !a->locomotion_error.empty());
            if (removal)
                CHECK(!g.server.IsObjectAlive(h) && !g.server.IsObjectAlive(v) && g.objects.get(h));
            else
                CHECK(!t->recursive_ok && a->locomotion_failures == 1);
            CHECK(g.objects.clear() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
    bool attacks(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(armed(a));
        CHECK(victim(g, a));
        a->m_fSeeingDist = 400;
        auto phase = [&](int state, int meta, float distance)
        {
            return a->SetNewState(state) &&
                   (a->Metacmd = meta, a->m_vTargetPos = DVector{0, 10, distance}, a->DispatchState());
        };
        CHECK(phase(AI_Mgr::StateAttackClose, 3, 100) && a->m_nCurMetacmd == 7);
        CHECK(phase(AI_Mgr::StateAttackClose, 3, 100.01f) && a->Metacmd == 4 && !a->m_bAnimating);
        CHECK(phase(AI_Mgr::StateAttackClose, 4, 199.99f) && a->m_nCurMetacmd == 2 && a->Metacmd == 4);
        CHECK(phase(AI_Mgr::StateAttackClose, 4, 200) && a->Metacmd == 5 && !a->m_bAnimating);
        CHECK(phase(AI_Mgr::StateAttackFar, 3, 300) && a->m_nCurMetacmd == 7);
        CHECK(phase(AI_Mgr::StateAttackFar, 4, 300.01f) && a->m_nCurMetacmd == 9 && a->Metacmd == 4);
        a->m_nInjuredLeg = 10;
        CHECK(phase(AI_Mgr::StateAttackFar, 4, 301) && a->m_nCurMetacmd == 1);
        a->m_nInjuredLeg = 0;
        CHECK(g.objects.advance(.051));
        CHECK(phase(AI_Mgr::StateAttackFar, 4, 301) && a->m_nCurMetacmd == 7 && a->ledge);
        // State uses preceding aggregate cache, not a fresh actor pose.
        g.objects.get(a->m_hObject)->state.m_Pos.z = 1000;
        CHECK(phase(AI_Mgr::StateAttackClose, 3, 100) && near(a->attack_distance, 100));
        CHECK(a->SetNewTarget(nullptr));
        a->m_vTargetPos = {0, 10, 100};
        CHECK(a->AI_STATE_AttackClose() && a->m_nState == AI_Mgr::StateSearchVisual);
        a->m_MoveObj.sampled = false;
        CHECK(!a->AI_STATE_AttackFar());
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g));
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        for (const char* name : {"m_cultist1", "f_cultist1", "m_civilian1", "f_civilian1", "labtech", "gideon"})
        {
            auto* a = actor(g);
            CHECK(a);
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load(std::string("models/enemies/") + name + ".abc", r->model, e));
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject) && a->InitStatics(table));
            for (unsigned command = 0; command < 4; ++command)
            {
                const auto clip = command == 0   ? table->m_nAnim_WALK[TYPE_MELEE]
                                  : command == 1 ? table->m_nAnim_RUN[TYPE_MELEE]
                                  : command == 2 ? table->m_nAnim_FIRE_WALK[TYPE_PISTOL]
                                                 : table->m_nAnim_FIRE_RUN[TYPE_PISTOL];
                a->m_bAnimating = false;
                const bool ok = command == 0   ? a->MC_Walk()
                                : command == 1 ? a->MC_Run()
                                : command == 2 ? a->MC_Fire_Walk()
                                               : a->MC_Fire_Run();
                CHECK(ok == (clip < r->model->animations.size()));
                CHECK(!ok || r->animation.animation == clip);
                std::cout << name << " command=" << command << " clip=" << clip << " available=" << ok << "\n";
            }
            CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"obstruction", obstruction}, {"rays", rays},       {"navigate", navigate},       {"ledges", ledges},
        {"walking", walking},         {"physics", physics}, {"moving_fire", moving_fire}, {"keys", keys},
        {"lifetime", lifetime},       {"attacks", attacks}, {"catalog", catalog}};
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
