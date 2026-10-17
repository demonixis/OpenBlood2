#include "brush_fixture.h"
#include "game/original/CultistAI.h"
#include "game/original/SmellHint.h"
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
    struct RecoveryProbe : AI_Mgr
    {
        int decisions = 0, requested_stimulus = -2;
        bool recurse = false, remove = false, nested_ok = true;
        bool ComputeState(int stimulus = -1) override
        {
            ++decisions;
            requested_stimulus = stimulus;
            if (recurse)
                nested_ok = AI_STATE_WalkAroundObj();
            if (remove)
                GetServerDE()->RemoveObject(m_hObject);
            return true; // records the requested decision; never fakes perception or movement
        }
    };
    RecoveryProbe* actor(GameSession& g, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{"AIRecoveryProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<RecoveryProbe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = 0;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<RecoveryProbe*>(r->game.get()) : nullptr;
        if (!a)
            return nullptr;
        auto model = std::make_shared<Model>();
        std::string error;
        if (!read_model(test_model::abc().b, *model, error))
            return nullptr;
        auto clip = model->animations.front();
        model->animations.clear();
        for (unsigned i = 0; i < 77; ++i)
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
            table->m_nAnim_JUMP[i] = 41 + i;
            table->m_nAnim_CROUCH[i] = 50 + i;
            table->m_nAnim_CRAWL[i] = 59 + i;
            table->m_nAnim_FIRE_CRAWL[i] = 68 + i;
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

    bool armed(RecoveryProbe* a)
    {
        CHECK(a && a->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA, 0) == InventoryResult::Ok);
        a->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 100);
        return true;
    }
    bool jump(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        a->m_fJumpSpeed = 40;
        a->m_fRunSpeed = 20;
        a->Metacmd = 7;
        CHECK(a->MC_Jump() && r->animation.animation == 41 + TYPE_PISTOL && !r->animation.looping);
        CHECK(near(r->velocity.y, 40) && near(r->velocity.z, 20) && a->Metacmd == 7);
        CHECK(g.server.advance_models(2));
        r->velocity = {99, -3, 99};
        CHECK(a->MC_Jump() && near(r->velocity.y, -3) && near(r->velocity.x, 0) && near(r->velocity.z, 20));
        CHECK(a->m_bAnimating && a->Metacmd == 7); // completed clip is not landing
        r->state.m_Pos = {0, -8, 0};
        CHECK(a->MC_Jump() && !a->m_bAnimating && a->Metacmd == 8 && near(r->velocity.z, 0));
        CHECK(near(r->velocity.y, -3)); // StopVelocity preserves vertical velocity, even without gravity
        a->m_nTrapped = 1;
        CHECK(a->MC_Jump() && near(r->velocity.y, 0) && near(r->velocity.z, 0));
        g.server.collision_ready = false;
        CHECK(!a->MC_Jump() && !a->locomotion_error.empty());
        return true;
    }
    bool jump_physics(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g, {0, -8, 0});
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Flags = FLAG_SOLID | FLAG_GRAVITY;
        a->m_fJumpSpeed = 100;
        a->m_fRunSpeed = 20;
        CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->SetNewState(AI_Mgr::StateJumpOver));
        CHECK(a->DispatchState() && near(r->state.m_Pos.y, -8));
        bool rose = false;
        for (unsigned i = 0; i < 100 && a->Metacmd == 1; ++i)
        {
            CHECK(g.step(.01));
            rose |= r->state.m_Pos.y > -7;
            CHECK(a->DispatchState());
        }
        CHECK(rose && a->Metacmd == 2 && near(r->state.m_Pos.y, -8));
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateGuard);
        CHECK(r->state.m_Pos.z > 0 && r->state.m_Pos.z < 10 && near(r->velocity.z, 0));
        return true;
    }
    bool crouch(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        r->velocity = {1, 2, 3};
        a->Metacmd = 3;
        a->m_bAnimating = true;
        g.server.SetModelLooping(a->m_hObject, DTRUE);
        CHECK(a->MC_Crouch() && a->Metacmd == 4 && a->m_nCurMetacmd == 4 && a->m_bAnimating);
        CHECK(r->animation.animation == 50 + TYPE_PISTOL && r->animation.looping && near(r->velocity.z, 3));
        CHECK(a->MC_Crouch() && a->Metacmd == 5);
        a->Metacmd = std::numeric_limits<int>::max();
        CHECK(!a->MC_Crouch());
        a->Metacmd = 0;
        auto table = std::make_shared<CAnim_Sound>();
        table->m_nAnim_CROUCH[TYPE_PISTOL] = 9999;
        CHECK(a->InitStatics(table) && !a->MC_Crouch() && a->Metacmd == 0);
        return true;
    }
    bool crawling(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g, {0, -8, 0});
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        a->m_fWalkSpeed = 30;
        a->m_nInjuredLeg = 10;
        CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->SetNewState(AI_Mgr::StateCrawlUnder));
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 5 && r->animation.animation == 59 + TYPE_PISTOL &&
              near(r->velocity.z, 30));
        g.server.SetModelLooping(a->m_hObject, DFALSE);
        CHECK(g.server.advance_models(1));
        CHECK(a->DispatchState() && a->Metacmd == 2 && !a->m_bAnimating);
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateGuard);
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->SetNewState(AI_Mgr::StateCrawlUnder));
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 12 && r->animation.animation == 68 + TYPE_PISTOL);
        // Obstructed crawl overwrites the successful recovery selection with WalkAround.
        r->velocity = {};
        CHECK(a->m_MoveObj.Update() && a->m_MoveObj.Update());
        CHECK(a->MC_Fire_Crawl() && a->m_nState == AI_Mgr::StateWalkAround);
        // Missing shade cannot be interpreted as success then overwritten.
        CHECK(object(g, "block", {0, -8, 3}, {2, 2, .2f}));
        CHECK(a->MC_Crawl());
        r->velocity = {};
        CHECK(!a->MC_Crawl() && a->locomotion_error.find("GetPointShade") != std::string::npos);
        return true;
    }
    bool clearance(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g, {0, -8, 0});
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        CHECK(object(g, "ceiling", {0, -4, 0}, {10, .5f, 10}));
        auto m = std::make_shared<Model>(*r->model);
        m->animations[59 + TYPE_PISTOL].dimensions.y = 5;
        r->model = m;
        CHECK(a->MC_Crawl() && a->animation_blocked && near(r->dimensions.y, 2) &&
              near(r->velocity.z, a->m_fWalkSpeed));
        CHECK(a->m_bAnimating && near(a->m_vDims.y, 2) && near(a->animation_requested_dims.y, 5));
        CHECK(a->MC_Jump()); // separate valid short clip still works
        auto h = a->m_hObject;
        CHECK(g.objects.remove(h));
        CHECK(!a->MC_Jump() && !a->AI_STATE_JumpOverObj());
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        return true;
    }
    bool topos(const char*)
    {
        for (bool run : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            auto* a = actor(g, {0, -8, 0});
            CHECK(a);
            auto* r = g.objects.get(a->m_hObject);
            a->m_fWalkSpeed = 10;
            a->m_fRunSpeed = 20;
            CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->SetNewState(run ? AI_Mgr::StateRunTo : AI_Mgr::StateWalkTo));
            a->m_vDestPos = {0, 10, 5};
            CHECK(a->DispatchState() && a->Metacmd == 2 && a->m_MoveObj.path().size() == 1);
            CHECK(a->DispatchState() && a->Metacmd == 3 && near(r->velocity.z, 0));
            CHECK(a->m_MoveObj.Update() && a->DispatchState() && near(r->velocity.z, run ? 20 : 10));
            CHECK(near(r->state.m_Pos.z, 0));
            CHECK(g.step(.1) && near(r->state.m_Pos.z, run ? 2 : 1));
            // Arrival flattens Y and uses the previous aggregate sample, not current pose.
            r->state.m_Pos.z = 5 - a->navigation_threshold;
            CHECK(a->m_MoveObj.Update());
            r->state.m_Pos.z = 100;
            CHECK(a->DispatchState() && a->Metacmd == 4 && a->navigation_arrivals == 1 && a->m_MoveObj.path().empty());
            CHECK(a->ledge_queries == 0 &&
                  r->velocity.z > 0); // no ledge query on the last point, residual velocity retained
            CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateGuard && near(r->velocity.z, 0));
        }
        return true;
    }
    bool plans(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->SetNewState(AI_Mgr::StateWalkTo));
        a->m_vDestPos = {0, 10, 10};
        CHECK(a->DispatchState());
        CHECK(a->SetNewState(AI_Mgr::StateWalkAround));
        a->m_vDestPos = {}; // original preflight keeps the old path
        CHECK(!a->DispatchState() && a->navigation_plan_failed && a->Metacmd == 2 && !a->m_MoveObj.path().empty());
        CHECK(!a->DispatchState());
        a->Metacmd = 3;
        CHECK(!a->DispatchState() && a->velocity_commands == 0);
        a->Metacmd = 1;
        a->m_vDestPos = {0, 10, 20};
        CHECK(a->DispatchState() && !a->navigation_plan_failed);
        CHECK(a->SetNewState(AI_Mgr::StateRunTo));
        a->m_MoveObj.Term();
        a->Metacmd = 3;
        CHECK(!a->DispatchState() && a->navigation_error.find("Missing point") != std::string::npos);
        a->Metacmd = 1;
        g.server.collision_ready = false;
        CHECK(!a->DispatchState() && a->navigation_plan_failed && a->m_nState == AI_Mgr::StateRunTo);
        g.server.collision_ready = true;
        CHECK(a->DispatchState() && !a->navigation_plan_failed);
        CHECK(object(g, "detour obstacle", {0, 10, 25}, {4, 5, 5}));
        CHECK(a->SetNewState(AI_Mgr::StateWalkTo));
        a->m_vDestPos = {0, 10, 100};
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateIdle && a->navigation_plan_failed);
        CHECK(a->m_MoveObj.outcome == CMovement::Outcome::Blocked && a->m_MoveObj.path().size() == 1);
        a->Metacmd = 3;
        CHECK(!a->AI_STATE_WalkToPos() && a->velocity_commands == 0);
        return true;
    }
    bool around(const char*)
    {
        for (bool run : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            auto* a = actor(g);
            CHECK(a);
            CHECK(a->SetNewState(run ? AI_Mgr::StateRunAround : AI_Mgr::StateWalkAround));
            a->m_vDestPos = {0, 10, 10};
            CHECK(a->DispatchState() && a->Metacmd == 2 && a->decisions == 0);
            CHECK(a->DispatchState() && a->Metacmd == 3);
            CHECK(a->DispatchState() && a->m_nCurMetacmd == (run ? 2 : 1));
            auto* r = g.objects.get(a->m_hObject);
            r->state.m_Pos.z = 10;
            CHECK(a->m_MoveObj.Update());
            CHECK(a->DispatchState() && a->decisions == 1 && a->requested_stimulus == -1 &&
                  a->navigation_arrivals == 1);
            CHECK(a->ledge_queries == 0); // last-point short circuit
            a->Metacmd = 2;
            CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateSearchSmell);
            CHECK(a->DispatchState() && a->smell_exit == 1 && a->decisions == 2);
        }
        return true;
    }
    bool search(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->SetNewState(AI_Mgr::StateSearchVisual));
        CHECK(a->DispatchState() && a->decisions == 1 && a->requested_stimulus == -1); // no retained target
        auto* enemy = actor(g, {0, 10, 100});
        CHECK(enemy);
        a->m_bCabal = true;
        enemy->m_bCabal = false;
        CHECK(a->InsertMe() && enemy->InsertMe());
        g.objects.get(enemy->m_hObject)->state.m_Flags = FLAG_SOLID;
        a->m_fSeeingDist = 200;
        CHECK(a->DispatchState() && a->decisions == 2 && a->requested_stimulus == 1 &&
              a->GetTarget() == enemy->m_hObject);
        CHECK(near(a->m_vTargetPos.z, 100));
        // Center sight succeeds while the original right-side navigation ray is blocked.
        CHECK(object(g, "side blocker", {1.514214f, 10, 50}, {.2f, 1, 1}));
        a->Metacmd = 4;
        CHECK(a->DispatchState() && a->decisions == 3 && a->requested_stimulus == -1);
        CHECK(a->m_MoveObj.outcome == CMovement::Outcome::Obstructed);
        // Inject a valid helper-computed route and sample a supporting floor.
        CHECK(a->m_MoveObj.CalculatePath({0, 10, 20}));
        a->Metacmd = 3;
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 2 && a->ledge_queries == 0);
        CHECK(g.objects.advance(.051));
        CHECK(a->DispatchState() && a->decisions == 4 && a->ledge_queries == 1 && a->ledge);
        g.objects.get(a->m_hObject)->state.m_Pos.y = -8;
        g.objects.get(enemy->m_hObject)->state.m_Pos.y = -8;
        CHECK(object(g, "lower side blocker", {1.514214f, -8, 50}, {.2f, 1, 1}));
        CHECK(a->m_MoveObj.Update() && g.objects.advance(.051));
        const auto queries = a->ledge_queries;
        CHECK(a->DispatchState() && a->decisions == 4 && a->ledge_queries == queries + 1 && !a->ledge_sampled &&
              !a->ledge);
        // First support ray succeeds; the second source query is throttled false.
        // A failed query cannot produce a decision.
        g.server.collision_ready = false;
        auto d = a->decisions;
        CHECK(!a->DispatchState() && a->decisions == d);
        return true;
    }
    bool lifetime(const char*)
    {
        for (bool remove : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            auto* a = actor(g);
            CHECK(a);
            a->remove = remove;
            a->recurse = !remove;
            CHECK(a->SetNewState(AI_Mgr::StateWalkAround));
            a->Metacmd = 4;
            CHECK(!a->DispatchState() && a->navigation_failures > 0);
            if (remove)
                CHECK(!g.server.IsObjectAlive(a->m_hObject));
            else
                CHECK(!a->nested_ok && a->decisions == 1);
            CHECK(g.objects.clear() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
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
                    recursive_ok = dynamic_cast<RecoveryProbe*>(GetServerDE()->HandleToObject(sender))->MC_Fire_Crawl();
                if (remove)
                {
                    GetServerDE()->RemoveObject(sender);
                    GetServerDE()->RemoveObject(m_hObject);
                }
            }
            return result;
        }
    };
    Victim* victim(GameSession& g, RecoveryProbe* a)
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
        a->m_fWalkSpeed = 60;
        CHECK(a->MC_Fire_Crawl() && r->animation.animation == 68 + TYPE_PISTOL && a->m_fLoadTimeStart == 0 &&
              near(r->velocity.z, 60));
        CHECK(g.objects.advance(a->m_fAttackLoadTime) && a->MC_Fire_Crawl() && a->fire_shots == 0);
        CHECK(g.objects.advance(.001) && a->MC_Fire_Crawl() && a->fire_shots == 1 && t->damage.m_fHitPoints < 1000);
        CHECK(a->MC_Fire_Crawl() && a->fire_shots == 1);
        const auto meta = a->Metacmd;
        g.server.SetModelLooping(a->m_hObject, DFALSE);
        CHECK(g.server.advance_models(1));
        CHECK(a->MC_Fire_Crawl() && a->Metacmd == meta + 1 && !a->m_bAnimating && near(r->velocity.z, 0));
        CHECK(a->MC_Fire_Walk() && a->m_nCurMetacmd == 8 && r->animation.animation == 22 + TYPE_PISTOL);
        CHECK(object(g, "cover", {0, 10, 20}, {50, 50, 1}));
        CHECK(g.objects.advance(1));
        g.server.SetModelLooping(a->m_hObject, DFALSE);
        CHECK(g.server.advance_models(1));
        CHECK(a->MC_Fire_Walk() && a->Metacmd == meta + 3 && a->fire_result == FireResult::Blocked);
        // Unarmed moving fire selects pistol animation, then ordinary refusal advances once.
        auto* u = actor(g, {100, 10, 0});
        CHECK(u && u->MC_Fire_Crawl());
        CHECK(g.objects.get(u->m_hObject)->animation.animation == 68 + TYPE_PISTOL);
        CHECK(u->MC_Fire_Crawl() && !u->m_bAnimating);
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
        model->animations[68 + TYPE_PISTOL].keys = {{0, {}, {}, ""}, {50, {}, {}, "[fire_key:1]"}, {100, {}, {}, ""}};
        r->model = model;
        r->state.m_Flags = FLAG_MODELKEYS;
        CHECK(a->MC_Fire_Crawl() && g.objects.advance(1));
        CHECK(g.server.advance_models(.06) && a->fire_shots == 1 && a->m_fLoadTimeStart == 0);
        CHECK(a->MC_Fire_Crawl() && a->fire_shots == 2 && near(a->m_fLoadTimeStart, 1));
        return true;
    }
    bool fire_lifetime(const char*)
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
            CHECK(a->MC_Fire_Crawl() && g.objects.advance(1));
            CHECK(!a->MC_Fire_Crawl() && !a->locomotion_error.empty());
            if (removal)
                CHECK(!g.server.IsObjectAlive(h) && !g.server.IsObjectAlive(v) && g.objects.get(h));
            else
                CHECK(!t->recursive_ok && a->locomotion_failures == 1);
            CHECK(g.objects.clear() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
    HOBJECT scent(GameSession& g, DVector pos)
    {
        ObjectCreateStruct state;
        state.m_Pos = pos;
        return g.objects.create(g.objects.find_class("SmellHint"), state);
    }
    bool remember_dead_attacker(GameSession& g, RecoveryProbe* a)
    {
        // Exercise a source path that retains its prior target. No perception override.
        auto* dead = actor(g, {500, -8, 500});
        auto* live = actor(g, {500, -8, 600});
        CHECK(dead && live);
        auto msg = g.server.StartMessageToObject(dead, a->m_hObject, damage_message);
        CHECK(msg && g.server.WriteDamage(msg, {{}, {}, 1, DamageType::Normal, dead->m_hObject}) == DE_OK);
        g.server.EndMessage(msg);
        dead->m_damage.m_bDead = true;
        CHECK(a->SetNewTarget(live->m_hObject));
        CHECK(a->ComputeStimuli() == 0 && a->GetTarget() == live->m_hObject && a->from_damage);
        return true;
    }
    bool smell_guards(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && scent(g, {0, 10, 20}));
        a->m_fSeeingDist = a->m_fSensingDist = 0;
        a->m_fSmellingDist = 100;
        CHECK(a->SetNewState(AI_Mgr::StateSearchSmell) && a->DispatchState());
        CHECK(a->stimulus == 3 && !a->GetTarget() && !a->smell_cursor && a->smell_exit == 1);
        CHECK(a->decisions == 1 && a->requested_stimulus == -1 && a->velocity_commands == 0);
        a->m_fSmellingDist = std::numeric_limits<float>::quiet_NaN();
        CHECK(!a->DispatchState() && a->decisions == 1 && !a->navigation_error.empty());
        a->m_fSmellingDist = 100;
        CHECK(remember_dead_attacker(g, a));
        CHECK(a->DispatchState() && a->smell_exit == 3 && a->decisions == 2);
        auto* dead = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(a->m_damage.GetLastDamager()));
        CHECK(dead);
        dead->m_damage.m_bDead = false;
        CHECK(a->DispatchState() && a->smell_exit == 2 && a->requested_stimulus == 1);
        a->recurse = true;
        CHECK(!a->DispatchState() && !a->nested_ok);
        return true;
    }
    bool smell_route(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g, {0, -8, 0});
        CHECK(a && remember_dead_attacker(g, a));
        auto first = scent(g, {0, -8, 20});
        auto skipped = scent(g, {0, -8, 30});
        auto next = scent(g, {0, -8, 40});
        CHECK(first && skipped && next && a->SetSmellCursor(first));
        CHECK(a->SetNewState(AI_Mgr::StateSearchSmell));
        a->m_fSmellingDist = 100;
        // Ordinary hints are not ray-hit objects. Preserve the failed visible-search decision.
        CHECK(a->DispatchState() && a->smell_exit == 4 && a->decisions == 1);
        // Diagnostic-only ray-hit hint exercises successful original phase 1.
        g.objects.get(first)->state.m_Flags |= FLAG_RAYHIT;
        g.objects.get(first)->dimensions = {1, 1, 1};
        CHECK(a->DispatchState() && a->Metacmd == 2 && a->smell_cursor == first);
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 2 && a->velocity_commands == 1);
        CHECK(g.objects.get(a->m_hObject)->state.m_Pos.z == 0); // commands never integrate
        CHECK(g.objects.advance(.051));
        CHECK(a->DispatchState() && a->ledge_queries == 1 && !a->ledge_sampled);
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Pos.z = 20 - a->navigation_threshold;
        CHECK(a->m_MoveObj.Update() && g.objects.remove(skipped));
        CHECK(a->DispatchState() && a->smell_cursor == next && a->smell_advances == 1 && a->Metacmd == 3);
        CHECK(g.objects.remove(first) && g.objects.finish_frame());
        CHECK(a->smell_cursor == next && a->m_nState == AI_Mgr::StateSearchSmell); // old link released
        CHECK(a->DispatchState() && a->decisions == 2);
        a->Metacmd = 2;
        r->state.m_Pos.z = 40;
        CHECK(a->m_MoveObj.Update() && a->DispatchState() && a->Metacmd == 3 && a->smell_advances == 1);
        CHECK(!a->SetSmellCursor(a->m_hObject) && a->smell_cursor == next);
        CHECK(g.objects.remove(next) && g.objects.finish_frame());
        CHECK(!a->smell_cursor && a->smell_invalidations == 1 && a->m_nState == AI_Mgr::StateIdle);
        CHECK(SmellHint::m_dwNumSmells == 0);
        return true;
    }
    bool smell_lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto h = scent(g, {0, 10, 10});
        CHECK(a && h && a->SetSmellCursor(h) && a->SetSmellCursor(h));
        CHECK(g.objects.advance(20) && g.objects.finish_frame());
        CHECK(!a->smell_cursor && a->smell_invalidations == 1 && !g.objects.get(h));
        auto next = scent(g, {0, 10, 10});
        CHECK(next && next != h && !a->SetSmellCursor(h) && a->SetSmellCursor(next));
        CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame() && g.objects.get(next));
        CHECK(g.unload_world() && SmellHint::m_dwNumSmells == 0 && g.server.string_count() == 0 &&
              g.server.message_count() == 0 && g.server.list_count() == 0);
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
            for (unsigned cmd = 0; cmd < 4; ++cmd)
            {
                const auto clip = cmd == 0   ? table->m_nAnim_JUMP[TYPE_PISTOL]
                                  : cmd == 1 ? table->m_nAnim_CROUCH[TYPE_PISTOL]
                                  : cmd == 2 ? table->m_nAnim_CRAWL[TYPE_PISTOL]
                                             : table->m_nAnim_FIRE_CRAWL[TYPE_PISTOL];
                a->m_bAnimating = false;
                const bool ok = cmd == 0   ? a->MC_Jump()
                                : cmd == 1 ? a->MC_Crouch()
                                : cmd == 2 ? a->MC_Crawl()
                                           : a->MC_Fire_Crawl();
                CHECK(ok == (clip < r->model->animations.size()));
                CHECK(!ok || r->animation.animation == clip);
                std::cout << name << " command=" << cmd << " clip=" << clip << " available=" << ok << '\n';
            }
            CHECK(a->SetNewState(AI_Mgr::StateSearchSmell) && a->DispatchState() && a->smell_exit == 1);
            CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> cases{{"jump", jump},
                                                                        {"jump_physics", jump_physics},
                                                                        {"crouch", crouch},
                                                                        {"crawling", crawling},
                                                                        {"clearance", clearance},
                                                                        {"topos", topos},
                                                                        {"plans", plans},
                                                                        {"around", around},
                                                                        {"search", search},
                                                                        {"lifetime", lifetime},
                                                                        {"moving_fire", moving_fire},
                                                                        {"keys", keys},
                                                                        {"fire_lifetime", fire_lifetime},
                                                                        {"smell_guards", smell_guards},
                                                                        {"smell_route", smell_route},
                                                                        {"smell_lifetime", smell_lifetime},
                                                                        {"catalog", catalog}};
    auto t = cases.find(argv[1]);
    return t != cases.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
