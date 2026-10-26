#include "brush_fixture.h"
#include "game/original/CivilianAI.h"
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
        return std::abs(a - b) < .001f;
    }
    bool setup(GameSession& g)
    {
        if (!g.objects.find_class("BaseClass"))
            CHECK(g.initialize());
        auto floor = test_brush::box("floor", {-10000, -20, -10000}, {10000, -10, 10000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    struct ScheduledCivilian : CivilianAI
    {
        bool running = true;
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_UPDATE && running)
            {
                GetServerDE()->SetNextUpdate(m_hObject, .001f);
                running = DispatchState();
                if (!running)
                    GetServerDE()->SetNextUpdate(m_hObject, 0);
            }
            return AI_Mgr::EngineMessageFn(m, d, f);
        }
    };
    struct RemovingCivilian : ScheduledCivilian
    {
        inline static unsigned destroyed = 0, removed_keys = 0;
        ~RemovingCivilian() override
        {
            ++destroyed;
        }
        bool OnStringKey(ArgList* args) override
        {
            if (args && args->argc && std::string_view(args->argv[0]) == "remove")
            {
                ++removed_keys;
                GetServerDE()->RemoveObject(m_hObject);
                // Destruction is deferred until the callback/animation batch ends.
                return destroyed == 0;
            }
            return AI_Mgr::OnStringKey(args);
        }
    };
    struct ResizeOnTouch : AI_Mgr
    {
        unsigned touches = 0;
        OpenBlood2::ResizeResult unchanged = OpenBlood2::ResizeResult::Invalid;
        OpenBlood2::ResizeResult changed = OpenBlood2::ResizeResult::Resized;
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_TOUCHNOTIFY)
            {
                ++touches;
                DVector dims;
                auto* server = GetServerDE();
                if (server->GetObjectDims(m_hObject, &dims) != DE_OK)
                    return 0;
                unchanged = server->ResizeObject(m_hObject, dims);
                dims.y += 1;
                changed = server->ResizeObject(m_hObject, dims);
                return 1;
            }
            return AI_Mgr::EngineMessageFn(m, d, f);
        }
    };
    template <class T = AI_Mgr> T* actor(GameSession& g, const char* name = "AIStateProbe", DVector pos = {0, 5, 0})
    {
        if (!g.objects.find_class(name))
        {
            static const ClassInfo type{name,
                                        g.objects.find_class("CBaseCharacter"),
                                        []() -> std::unique_ptr<BaseClass> { return std::make_unique<T>(); },
                                        {}};
            if (!g.objects.register_class(type))
                return nullptr;
        }
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = FLAG_MODELKEYS;
        SAFE_STRCPY(s.m_Name, "state/action diagnostic (not production NPC)");
        auto h = g.objects.create(g.objects.find_class(name), s);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<T*>(r->game.get()) : nullptr;
        if (!ai)
            return nullptr;
        auto model = std::make_shared<Model>();
        std::string error;
        if (!read_model(test_model::abc().b, *model, error))
            return nullptr;
        const auto clip = model->animations.front();
        model->animations.clear();
        for (unsigned i = 0; i < 18; ++i)
        {
            model->animations.push_back(clip);
            auto& a = model->animations.back();
            a.name = "clip" + std::to_string(i);
            a.dimensions = {2, 4, 2};
            a.keys.clear();
        }
        r->model = model;
        r->dimensions = {1, 1, 1};
        auto table = std::make_shared<CAnim_Sound>();
        for (unsigned i = 0; i < 4; ++i)
            table->m_nAnim_IDLE[i] = i;
        for (unsigned i = 0; i < 5; ++i)
            table->m_nAnim_TALK[i] = 4 + i;
        for (unsigned i = 0; i < 9; ++i)
            table->m_nAnim_SPECIAL[i] = 9 + i;
        ai->InitStatics(table);
        ai->m_damage.Init(h);
        ai->m_damage.m_fHitPoints = ai->m_damage.m_fMaxHitPoints = 100;
        return ai;
    }
    bool symbols(const char*)
    {
        int state = -50;
        CHECK(AI_Mgr::StateStrToInt("idle_extra", state) && state == AI_Mgr::StateIdle);
        CHECK(AI_Mgr::StateStrToInt("sEaR-anything", state) && state == AI_Mgr::StateSearchVisual);
        CHECK(AI_Mgr::StateStrToInt("SPECIAL10", state) && state == AI_Mgr::StateSpecial1);
        CHECK(AI_Mgr::StateStrToInt(" passIVE", state) && state == AI_Mgr::StateInactive);
        CHECK(AI_Mgr::StateStrToInt("", state) && state == AI_Mgr::StateInactive);
        CHECK(!AI_Mgr::StateStrToInt(std::string(33, 'a'), state) && state == AI_Mgr::StateInactive);
        CHECK(!AI_Mgr::StateStrToInt(std::string_view("IDLE\0", 5), state));
        return true;
    }
    bool transitions(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        r->velocity = {4, 8, 9};
        a->m_nState = AI_Mgr::StateAttackClose;
        a->Metacmd = 7;
        a->m_bAnimating = true;
        CHECK(a->SetNewState(AI_Mgr::StateGuard));
        CHECK(a->m_nLastState == AI_Mgr::StateAttackClose && a->Metacmd == 1 && !a->m_bAnimating &&
              a->m_nCurMetacmd == 999);
        CHECK(r->velocity.x == 0 && r->velocity.z == 0 && r->velocity.y == 8 && near(a->m_fWaitForIdleTime, 120));
        a->m_bHasTargeted = true;
        CHECK(a->SetNewState(AI_Mgr::StateIdle) && a->m_nState == AI_Mgr::StateSearchVisual &&
              a->m_nLastState == AI_Mgr::StateGuard);
        CHECK(g.objects.advance(60) && g.objects.advance(60));
        CHECK(a->SetNewState(AI_Mgr::StateIdle) && a->m_nState == AI_Mgr::StateSearchVisual);
        CHECK(g.objects.advance(.01));
        CHECK(a->SetNewState(AI_Mgr::StateIdle) && a->m_nState == AI_Mgr::StateIdle &&
              a->m_nLastState == AI_Mgr::StateGuard);
        for (int state : {AI_Mgr::StateWalkAround, AI_Mgr::StateRunAround, AI_Mgr::StateJumpOver,
                          AI_Mgr::StateCrawlUnder, AI_Mgr::StateSearchVisual, AI_Mgr::StateSearchSmell})
        {
            a->m_nState = state;
            a->m_nLastState = AI_Mgr::StateAttackFar;
            CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->m_nLastState == AI_Mgr::StateAttackFar);
        }
        a->m_nTrapped = 1;
        r->velocity = {1, 2, 3};
        CHECK(a->SetNewState(AI_Mgr::StateGuard));
        CHECK(r->velocity.x == 0 && r->velocity.y == 0 && r->velocity.z == 0);
        CHECK(a->state_changes == 11);
        return true;
    }
    bool commands(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        CHECK(a->MC_Idle() && a->m_bAnimating && a->m_nCurMetacmd == 0 && !r->animation.looping &&
              a->animation_choice < 4);
        g.server.trace_enabled = true;
        const auto clip = r->animation.animation;
        const auto revision = r->animation.revision;
        CHECK(a->MC_Talk() && r->animation.animation == clip && r->animation.revision == revision); // shared MC_IDLE
        CHECK(g.server.service_calls["RandomInt"] == 0);
        CHECK(g.server.advance_models(1) && a->MC_Talk() && !a->m_bAnimating && a->Metacmd == 2);
        CHECK(a->MC_Talk() && r->animation.animation >= 4 && r->animation.animation <= 8);
        CHECK(a->MC_Special(8) && r->animation.animation == 17 && a->m_nCurMetacmd == 25);
        const auto special_revision = r->animation.revision;
        CHECK(a->MC_Special(-99) && r->animation.revision == special_revision); // ignored during continuation
        CHECK(g.server.advance_models(1) && a->MC_Special(0) && a->Metacmd == 3);
        CHECK(a->animation_starts == 3 && a->animation_finishes == 2);
        CHECK(!a->MC_Special(-1) && !a->MC_Special(9));
        CHECK(a->MC_Idle());
        CHECK(g.server.advance_models(1));
        a->Metacmd = std::numeric_limits<int>::max();
        CHECK(!a->MC_Idle() && a->m_bAnimating);
        return true;
    }
    bool resize(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Flags |= FLAG_GRAVITY; // Grounded actors keep their feet.
        a->m_vScale = {2, 2, 2};
        CHECK(a->SetAnimation(1));
        CHECK(near(r->dimensions.y, 8) && near(r->state.m_Pos.y, 12) && near(a->m_vDims.y, 8));
        CHECK(g.server.advance_models(.2));
        const auto rev = r->animation.revision;
        g.server.collision_ready = false;
        CHECK(a->SetAnimation(1) && r->animation.revision > rev && r->animation.time == 0); // reset needs no resize
        CHECK(!a->SetAnimation(2) && r->animation.animation == 1 && a->m_nCurAnimation == 1);
        g.server.collision_ready = true;
        auto ceiling = test_brush::box("ceiling", {-100, 20, -100}, {100, 25, 100});
        for (auto& s : ceiling.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(ceiling, error));
        a->m_vScale = {3, 3, 3};
        const auto before = r->state.m_Pos;
        // AI_Mgr::SetAnimation in the original ignores SetObjectDims2 refusal.
        // The physical body remains safe, but the clip advances and the AI caches the retained physical dimensions.
        CHECK(a->SetAnimation(2) && r->animation.animation == 2 && r->state.m_Pos.y == before.y &&
              r->dimensions.y == 8 && near(a->m_vDims.y, 8) && near(a->animation_requested_dims.y, 12));
        CHECK(a->animation_blocked && a->animation_resize_blocks == 1 && a->state_error.empty());
        CHECK(g.server.advance_models(.025) && near(r->animation.time, 25.f));
        CHECK(a->SetAnimation(2) && !a->animation_blocked && a->animation_resize_blocks == 1);
        CHECK(r->animation.time == 0 && r->dimensions.y == 8); // Same-clip reset does not retry the resize.
        a->m_vScale = {-1, 1, 1};
        CHECK(!a->SetAnimation(3));
        CHECK(g.server.advance_models(.025));
        CHECK(a->SetAnimation(UINT32_MAX) && a->m_nCurAnimation == UINT32_MAX && a->animation_clip_retained);
        CHECK(r->animation.animation == 2 && near(r->animation.time, 25.f) && near(r->dimensions.y, 8));
        CHECK(a->SetAnimation(UINT32_MAX) && r->animation.time == 0 && a->retained_clip_requests == 2);
        const auto saved_model = r->model;
        r->model.reset();
        CHECK(!a->SetAnimation(2) && !a->animation_clip_retained);
        r->model = saved_model;
        CHECK(setup(g)); // Remove ceiling; a different valid clip can resize normally again.
        a->m_vScale = {3, 3, 3};
        CHECK(a->SetAnimation(3) && !a->animation_blocked && near(r->dimensions.y, 12));
        CHECK(near(r->state.m_Pos.y, before.y + 4) && a->animation_resize_blocks == 1);
        // Without FLAG_GRAVITY (Gideon hovers) the original SetObjectDims2 keeps the centre.
        r->state.m_Flags &= ~FLAG_GRAVITY;
        const auto centre = r->state.m_Pos;
        a->m_vScale = {2, 2, 2};
        CHECK(a->SetAnimation(1) && near(r->dimensions.y, 8) && near(r->state.m_Pos.y, centre.y));
        return true;
    }
    bool resize_callback(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor<ResizeOnTouch>(g, "ResizeOnTouchProbe");
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Flags |= FLAG_TOUCH_NOTIFY;
        const auto before = r->state.m_Pos;
        const auto dims = r->dimensions;
        ObjectCreateStruct state;
        state.m_Pos = {-20, before.y, 0};
        state.m_Flags = FLAG_SOLID;
        const auto mover = g.objects.create(g.objects.find_class("BaseClass"), state);
        CHECK(mover);
        g.objects.get(mover)->dimensions = {1, 1, 1};
        DVector destination{20, before.y, 0};
        CHECK(g.server.MoveObject(mover, &destination) == DE_OK);
        CHECK(a->touches == 1 && a->unchanged == ResizeResult::Resized && a->changed == ResizeResult::InTransaction);
        CHECK(r->state.m_Pos.x == before.x && r->state.m_Pos.y == before.y && r->state.m_Pos.z == before.z);
        CHECK(r->dimensions.x == dims.x && r->dimensions.y == dims.y && r->dimensions.z == dims.z);
        // Missing collision and invalid extents still fail, even for a no-op.
        g.server.collision_ready = false;
        CHECK(g.server.ResizeObject(a->m_hObject, dims) == ResizeResult::Unavailable);
        g.server.collision_ready = true;
        r->dimensions.x = 0;
        CHECK(g.server.ResizeObject(a->m_hObject, r->dimensions) == ResizeResult::Invalid);
        return true;
    }
    bool decisions(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor<CultistAI>(g, "CultistDecisionProbe");
        CHECK(a);
        a->m_vDims = {2, 4, 2};
        a->m_vTargetPos = {0, 5, 100};
        struct Row
        {
            float health, sight, height;
            int expected;
        };
        for (auto row : {Row{.249f, .9f, 0, 4}, Row{.25f, .9f, 0, 1}, Row{.5f, .5f, 0, 1}, Row{.5f, .501f, 4, 0},
                         Row{1, .9f, 4.01f, 1}})
        {
            a->m_fStimuli[5] = row.health;
            a->m_fStimuli[0] = row.sight;
            a->m_vTargetPos.y = 5 + row.height;
            CHECK(a->ComputeState(1) && a->m_nState == row.expected);
        }
        a->m_nState = AI_Mgr::StateFindAmmo;
        a->m_nLastState = AI_Mgr::StateGuard;
        CHECK(a->ComputeState(0) && a->m_nState == AI_Mgr::StateGuard && a->m_nLastState == AI_Mgr::StateFindAmmo);
        a->m_nState = AI_Mgr::StateEscapeRun;
        a->m_nLastState = AI_Mgr::StateGuard;
        a->m_fStimuli[5] = 1;
        a->m_vTargetPos.y = 5;
        const auto changes = a->state_changes;
        CHECK(a->ComputeState(0) && a->m_nState == AI_Mgr::StateSearchVisual && a->state_changes == changes + 2);
        CHECK(a->m_nLastState == AI_Mgr::StateIdle); // fallthrough observes first transition
        a->m_fSeeingDist = std::numeric_limits<float>::quiet_NaN();
        const auto state = a->m_nState;
        CHECK(!a->ComputeState() && a->m_nState == state);
        CHECK(!a->ComputeState(-2) && !a->ComputeState(5));
        return true;
    }
    bool civilian(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor<CivilianAI>(g, "CivilianDecisionProbe");
        CHECK(a);
        a->m_szAIState = "SPECIAL6";
        a->m_fStimuli[5] = 1;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateSpecial6 && !a->m_bScared);
        a->m_fStimuli[5] = .99f;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateEscapeRun && a->m_bScared);
        CHECK(!a->DispatchState()); // No successful placeholder for fleeing.
        a->m_nState = AI_Mgr::StateEscapeHide;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateEscapeHide);
        CHECK(a->ComputeState(0) && a->m_nState == AI_Mgr::StateSpecial6);
        for (int special = 2; special <= 8; ++special)
        {
            CHECK(a->SetNewState(AI_Mgr::StateSpecial1 + special - 1) && a->DispatchState());
            const auto clip = g.objects.get(a->m_hObject)->animation.animation;
            const int expected[]{0, 0, 9, 10, 12, 13, 14, 16, 17};
            CHECK(clip == unsigned(expected[special]) ||
                  ((special == 3 || special == 6) && clip == unsigned(expected[special] + 1)));
            const auto revision = g.objects.get(a->m_hObject)->animation.revision;
            g.server.trace_enabled = true;
            const auto draws = g.server.service_calls["RandomInt"];
            CHECK(a->DispatchState() && g.objects.get(a->m_hObject)->animation.revision == revision);
            CHECK(g.server.service_calls["RandomInt"] == draws + ((special == 3 || special == 6) ? 1 : 0));
            CHECK(g.server.advance_models(1) && a->DispatchState() && a->Metacmd == 2);
        }
        ObjectCreateStruct s;
        s.m_Pos = {50, 5, 0};
        const auto target = g.objects.create(g.objects.find_class("BaseClass"), s);
        CHECK(a->SetNewTarget(target) && a->RefreshTargetPosition() && a->SetNewState(AI_Mgr::StateSpecial1));
        CHECK(a->DispatchState() && a->Metacmd == 1 && a->m_nCurMetacmd == 0);
        CHECK(a->face_commands == 1);
        return true;
    }
    bool dispatch(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor<ScheduledCivilian>(g, "ScheduledCivilianProbe");
        CHECK(a);
        a->m_bScared = false;
        g.server.SetNextUpdate(a->m_hObject, .001f);
        CHECK(g.step(.01) && a->state_dispatches == 1 && a->animation_starts == 1 && a->perception_calls == 0);
        CHECK(g.step(.05) && a->state_dispatches == 1);
        CHECK(g.step(.06) && a->state_dispatches == 2);
        for (int i = 0; i < 30; ++i)
            CHECK(g.step(.1));
        CHECK(a->animation_finishes >= 2 && a->state_changes >= 2 && a->running);
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar));
        CHECK(g.step(.2) && !a->running && a->state_failures == 1);
        const auto calls = a->state_dispatches;
        CHECK(g.step(1) && a->state_dispatches == calls);
        auto* base = actor(g);
        CHECK(base);
        base->m_bCabal = false;
        base->Metacmd = -4;
        base->m_nState = AI_Mgr::StatePassive;
        CHECK(base->DispatchState() && base->Metacmd == 2 && base->perception_calls == 0);
        CHECK(base->DispatchState() && g.server.advance_models(1) && base->DispatchState() && base->Metacmd == 3);
        CHECK(base->DispatchState() && base->Metacmd == 1);
        base->m_bCabal = true;
        CHECK(!base->DispatchState() && base->state_error.find("inventory query failed") != std::string::npos);
        CHECK(base->m_InventoryMgr.Init(base->m_hObject, nullptr));
        CHECK(base->DispatchState() && base->Metacmd == 2); // Connected empty-inventory selection.
        base->m_nState = AI_Mgr::StateInactive;
        CHECK(base->DispatchState());
        return true;
    }
    bool failures(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor<CivilianAI>(g, "CivilianFailureProbe");
        CHECK(a);
        const auto original = a->m_nState;
        CHECK(!a->SetNewState(-1) && a->m_nState == original);
        auto* r = g.objects.get(a->m_hObject);
        r->velocity.y = std::numeric_limits<float>::infinity();
        CHECK(!a->SetNewState(AI_Mgr::StateGuard) && a->state_changes == 0);
        r->velocity = {};
        a->m_fWaitForIdleTime = std::numeric_limits<float>::quiet_NaN();
        CHECK(!a->SetNewState(AI_Mgr::StateGuard));
        a->m_fWaitForIdleTime = 0;
        a->m_szAIState = std::string(33, 'a');
        CHECK(!a->ComputeState(0));
        a->m_bScared = true;
        a->m_fSeeingDist = std::numeric_limits<float>::quiet_NaN();
        CHECK(!a->DispatchState() && a->animation_starts == 0 && a->m_nState == original);
        r->model.reset();
        CHECK(!a->MC_Idle());
        CHECK(g.objects.remove(a->m_hObject));
        CHECK(!a->SetNewState(AI_Mgr::StateIdle) && !a->AI_STATE_Idle() && !a->AI_STATE_Passive() &&
              !a->AI_STATE_Special(3) && !a->MC_Special(0) && !a->DispatchState());
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 4; ++cycle)
        {
            CHECK(setup(g));
            auto* a = actor<ScheduledCivilian>(g, "ScheduledCivilianProbe");
            CHECK(a && !g.objects.get(old));
            auto* r = g.objects.get(a->m_hObject);
            g.server.SetNextUpdate(a->m_hObject, .001f);
            CHECK(g.step(.1));
            const auto time = r->animation.time;
            const auto calls = a->state_dispatches;
            g.server.SetObjectState(a->m_hObject, OBJSTATE_INACTIVE);
            CHECK(g.step(.5));
            CHECK(r->animation.time == time && a->state_dispatches == calls);
            g.server.SetObjectState(a->m_hObject, OBJSTATE_ACTIVE);
            CHECK(g.step(.1) && a->state_dispatches > calls);
            old = a->m_hObject;
            CHECK(g.objects.remove(old));
            CHECK(!a->DispatchState());
            CHECK(g.objects.finish_frame() && !g.objects.get(old));
            CHECK(g.unload_world() && g.server.string_count() == 0);
        }
        CHECK(setup(g));
        auto* a = actor<RemovingCivilian>(g, "RemovingCivilianProbe");
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        auto model = std::make_shared<Model>(*r->model);
        for (auto& clip : model->animations)
            clip.keys = {{0, {}, {}, "remove"}, {1, {}, {}, "remove"}};
        r->model = model;
        auto h = a->m_hObject;
        g.server.SetNextUpdate(h, .001f);
        CHECK(g.step(.1) && !g.objects.get(h));
        CHECK(RemovingCivilian::destroyed == 1 && RemovingCivilian::removed_keys == 1);
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g));
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        auto* a = actor(g);
        CHECK(a);
        unsigned clips = 0, tables = 0;
        for (const char* name : {"m_civilian1", "f_civilian1", "labtech", "m_cultist1", "f_cultist1", "gideon"})
        {
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load("models/enemies/" + std::string(name) + ".abc", r->model, error));
            a->m_nCurAnimation = 999;
            r->animation = {};
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject) && a->InitStatics(table));
            for (unsigned clip = 0; clip < r->model->animations.size(); ++clip)
            {
                CHECK(a->SetAnimation(clip));
                const auto rev = r->animation.revision;
                CHECK(g.server.advance_models(.05));
                CHECK(a->SetAnimation(clip) && r->animation.time == 0 && r->animation.revision > rev);
                ++clips;
            }
            for (unsigned i = 0; i < 9; ++i)
            {
                a->m_bAnimating = false;
                const bool present = table->m_nAnim_SPECIAL[i] < r->model->animations.size();
                const auto playing = r->animation.animation;
                CHECK(a->MC_Special(i) && a->animation_clip_retained == !present);
                CHECK(r->animation.animation == (present ? table->m_nAnim_SPECIAL[i] : playing));
                if (present)
                {
                    ++tables;
                    std::cout << name << " special" << i + 1
                              << " duration=" << r->model->animations[r->animation.animation].duration << "\n";
                }
            }
        }
        std::cout << "Private NPC animation changes/resets=" << clips << "; available special slots=" << tables << "\n";
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            CHECK(g.objects.find_class("CivilianAI") && g.objects.find_class("CultistAI"));
            auto* a = actor<ScheduledCivilian>(g, "ScheduledCivilianProbe", {0, 10000, 0});
            CHECK(a && !g.objects.get(old));
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load("models/enemies/m_civilian1.abc", r->model, error));
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject));
            CHECK(table->SetSoundRoot("sounds/enemies/civilian") && a->InitStatics(table));
            a->m_szAIState = "SPECIAL6";
            CHECK(a->ComputeState(0));
            g.server.SetNextUpdate(a->m_hObject, .001f);
            for (int i = 0; i < 960; ++i)
                CHECK(g.step(1. / 60));
            CHECK(a->running && a->state_failures == 0 && a->animation_finishes > 0 && a->animation_starts > 1);
            std::cout << "Subway diagnostic cycle=" << cycle << " starts=" << a->animation_starts
                      << " finishes=" << a->animation_finishes << " dispatches=" << a->state_dispatches << "\n";
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
    const std::map<std::string, std::function<bool(const char*)>> tests{{"symbols", symbols},
                                                                        {"transitions", transitions},
                                                                        {"commands", commands},
                                                                        {"resize", resize},
                                                                        {"resize_callback", resize_callback},
                                                                        {"decisions", decisions},
                                                                        {"civilian", civilian},
                                                                        {"dispatch", dispatch},
                                                                        {"failures", failures},
                                                                        {"lifetime", lifetime},
                                                                        {"catalog", catalog},
                                                                        {"subway", subway}};
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
