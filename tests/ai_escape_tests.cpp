#include "brush_fixture.h"
#include "game/original/CivilianAI.h"
#include "game/original/CultistAI.h"
#include "game/original/ExitHint.h"
#include "game/server_adapter.h"
#include "model_fixture.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <type_traits>
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
    template <class Original> struct Probe : Original
    {
        int decisions = 0, requested_stimulus = -2;
        bool record = false, recurse = false, remove = false, nested_ok = true;
        bool ComputeState(int stimulus = -1) override
        {
            ++decisions;
            requested_stimulus = stimulus;
            if (recurse)
                nested_ok = this->AI_STATE_Escape_Hide();
            if (remove)
                this->GetServerDE()->RemoveObject(this->m_hObject);
            return record ? true : Original::ComputeState(stimulus);
        }
    };
    using Civilian = Probe<CivilianAI>;
    using Cultist = Probe<CultistAI>;
    template <class T = Civilian> T* actor(GameSession& g, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{std::is_base_of_v<CivilianAI, T> ? "CivilianEscapeProbe" : "CultistEscapeProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<T>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = 0;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<T*>(r->game.get()) : nullptr;
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
        table->m_nAnim_TAUNT[6] = 76;
        for (auto& clip : table->m_nAnim_IDLE)
            clip = 75;
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

    HOBJECT hint(GameSession& g, DVector pos, bool rayhit = true)
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        s.m_Flags = rayhit ? FLAG_RAYHIT : 0;
        const auto h = g.objects.create(g.objects.find_class("ExitHint"), s);
        if (h)
            g.objects.get(h)->dimensions = {1, 1, 1};
        return h;
    }
    bool beg(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* c = actor<Cultist>(g, {100, 10, 0});
        CHECK(a && c);
        CHECK(near(a->m_fRollSpeed, 3.5f) && near(c->m_fRollSpeed, 105));
        auto* r = g.objects.get(a->m_hObject);
        a->Metacmd = 4;
        CHECK(a->MC_Taunt_Beg() && a->m_nCurMetacmd == 17 && a->m_bAnimating && r->animation.animation == 76);
        CHECK(near(r->velocity.x, 3.5f) && near(r->velocity.z, 0) && !r->animation.looping);
        CHECK(g.step(.1) && near(r->state.m_Pos.x, .35f));
        CHECK(g.server.advance_models(1) && a->MC_Taunt_Beg() && a->Metacmd == 5 && !a->m_bAnimating &&
              near(r->velocity.x, 3.5f));
        r->state.m_Rotation = rotation_from_euler({0, 1.57079632679f, 0});
        CHECK(a->m_MoveObj.Update());
        CHECK(a->MC_Taunt_Beg() && near(r->velocity.x, 0) && near(r->velocity.z, -3.5f));
        a->m_bAnimating = false;
        a->m_nTrapped = 1;
        CHECK(a->MC_Taunt_Beg() && near(r->velocity.z, 0));
        a->m_fRollSpeed = std::numeric_limits<float>::infinity();
        CHECK(!a->MC_Taunt_Beg());
        return true;
    }
    bool selection(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* c = actor<Cultist>(g);
        CHECK(a && c);
        a->m_fSeeingDist = c->m_fSeeingDist = 100;
        auto plain = hint(g, {0, 10, 20}, false);
        CHECK(plain);
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState() && a->m_nState == AI_Mgr::StateEscapeHide);
        CHECK(c->SetNewState(AI_Mgr::StateEscapeRun) && c->DispatchState() && c->m_nState == AI_Mgr::StateAttackFar);
        // Nonsolid original hints do not become visible merely because a state needs one.
        auto first = hint(g, {-20, 10, 40}), second = hint(g, {20, 10, 40});
        CHECK(first && second);
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState() && a->GetTrackedObject() == second &&
              a->Metacmd == 2);
        auto enemy = object(g, "enemy", {0, 10, -30});
        CHECK(c->SetNewTarget(enemy) && c->RefreshTargetPosition());
        CHECK(c->SetNewState(AI_Mgr::StateEscapeRun) && c->DispatchState() && c->m_nState == AI_Mgr::StateAttackFar);
        CHECK(a->SetNewTarget(enemy) && a->RefreshTargetPosition());
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState() && a->GetTrackedObject() == second);
        return true;
    }
    bool facing(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        a->m_fSeeingDist = 100;
        auto h = hint(g, {30, 10, 0});
        CHECK(h);
        // Facing turns to +X; the previous cache remains +Z and hint forward is +Z.
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState() && a->Metacmd == 2 &&
              near(a->escape_hint_dot, 1));
        CHECK(a->m_MoveObj.GetForwardVector().z == 1 &&
              near(rotate_vector({0, 0, 1}, g.objects.get(a->m_hObject)->state.m_Rotation).x, 1));
        CHECK(a->m_MoveObj.Update()); // Now cached +X is exactly orthogonal to a +Y hint.
        g.objects.get(h)->state.m_Rotation = rotation_from_euler({-1.57079632679f, 0, 0});
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState() && a->m_nState == AI_Mgr::StateEscapeHide);
        CHECK(a->navigation_plan_failed);
        // Civilian uses current dimensions, unlike cached-dimension Cultist/Guard.
        g.objects.get(a->m_hObject)->dimensions = {100, 2, 100};
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState() && a->m_nState == AI_Mgr::StateEscapeHide);
        CHECK(a->navigation_threshold > 70 && a->navigation_distance == 30);
        return true;
    }
    template <class T> bool route_one()
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor<T>(g);
        CHECK(a);
        a->m_fSeeingDist = 100;
        a->m_fRunSpeed = 20;
        CHECK(hint(g, {0, 10, 20}));
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState() && a->Metacmd == 2);
        CHECK(a->DispatchState() && a->Metacmd == 3 && a->DispatchState());
        auto* r = g.objects.get(a->m_hObject);
        CHECK(near(r->velocity.z, 20));
        CHECK(g.step(.1) && near(r->state.m_Pos.z, 2));
        r->state.m_Pos.z = 20;
        CHECK(a->m_MoveObj.Update());
        a->m_vTargetPos = {0, 10, -10};
        CHECK(a->DispatchState() && a->Metacmd == 4 && a->navigation_arrivals == 1 && a->m_MoveObj.path().empty());
        CHECK(a->ledge_queries == 0 && near(r->velocity.z, 20));
        a->record = true;
        CHECK(a->DispatchState() && a->decisions == 1 && a->requested_stimulus == -1);
        a->Metacmd = 2;
        CHECK(a->DispatchState());
        CHECK(a->m_nState == (std::is_base_of_v<CivilianAI, T> ? AI_Mgr::StateEscapeRun : AI_Mgr::StateEscapeHide));
        a->Metacmd = 3;
        CHECK(!a->AI_STATE_Escape_RunAway());
        return true;
    }
    bool routes(const char*)
    {
        return route_one<Civilian>() && route_one<Cultist>();
    }
    bool perception(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* c = actor<Cultist>(g);
        auto* a = actor(g, {10, 10, 0});
        auto* enemy = actor(g, {0, 10, 50});
        CHECK(c && a && enemy);
        c->m_bCabal = a->m_bCabal = true;
        enemy->m_bCabal = false;
        CHECK(c->InsertMe() && a->InsertMe() && enemy->InsertMe());
        g.objects.get(enemy->m_hObject)->state.m_Flags = FLAG_SOLID;
        c->m_fSeeingDist = a->m_fSeeingDist = 100;
        CHECK(hint(g, {20, 10, 20}));
        CHECK(c->SetNewState(AI_Mgr::StateEscapeRun) && c->DispatchState());
        c->record = true;
        CHECK(c->DispatchState() && c->decisions == 1 && c->requested_stimulus == 1 &&
              c->GetTarget() == enemy->m_hObject);
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->DispatchState());
        auto calls = a->perception_calls;
        CHECK(a->DispatchState() && a->decisions == 0 && a->perception_calls == calls);
        // Guard uses actual smell precedence; the original search guard now requests a new decision.
        ObjectCreateStruct s;
        s.m_Pos = {100, 10, 0};
        auto smell = g.objects.create(g.objects.find_class("SmellHint"), s);
        CHECK(smell);
        a->m_fSeeingDist = 0;
        a->m_fSmellingDist = 200;
        CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->DispatchState() && a->m_nState == AI_Mgr::StateSearchSmell);
        CHECK(a->DispatchState() && a->smell_exit == 1 && !a->smell_cursor && a->decisions == 1);
        c->Metacmd = 2;
        g.server.collision_ready = false;
        auto decisions = c->decisions;
        CHECK(!c->AI_STATE_Escape_RunAway() && c->decisions == decisions);
        return true;
    }
    bool hiding(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* c = actor<Cultist>(g, {100, 10, 0});
        CHECK(a && c);
        for (AI_Mgr* ai : {static_cast<AI_Mgr*>(a), static_cast<AI_Mgr*>(c)})
        {
            auto target = object(g, "threat", {50, 10, 40});
            CHECK(target && ai->SetNewTarget(target) && ai->RefreshTargetPosition());
            CHECK(ai->SetNewState(AI_Mgr::StateEscapeHide));
            const auto turns = ai->face_commands;
            CHECK(ai->DispatchState() && ai->Metacmd == 1 && ai->m_nCurMetacmd == 17 && ai->face_commands == turns + 1);
            // Facing uses current pose, lateral Move deliberately uses the preceding cache.
            CHECK(g.objects.get(ai->m_hObject)->velocity.x > 0);
            CHECK(g.server.advance_models(1) && ai->DispatchState() && ai->Metacmd == 2 && !ai->m_bAnimating);
        }
        a->record = true;
        CHECK(a->DispatchState() && a->decisions == 1);
        CHECK(a->SetNewTarget(nullptr));
        auto face = a->face_commands;
        CHECK(a->DispatchState() && a->decisions == 2 && a->face_commands == face);
        return true;
    }
    bool guard(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g, {0, -8, 0});
        CHECK(a);
        a->record = true;
        a->m_vGuardPos = {0, -8, 0};
        CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->DispatchState() && a->Metacmd == 4 &&
              a->m_MoveObj.calculations == 0);
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 0);
        CHECK(g.server.advance_models(1) && a->DispatchState() && a->Metacmd == 5);
        CHECK(a->DispatchState() && a->decisions == 1);
        a->m_vGuardPos = {0, 10, 20};
        CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->DispatchState() && a->Metacmd == 2);
        CHECK(a->navigation_distance > 20 && a->DispatchState() && a->Metacmd == 3);
        a->m_fRunSpeed = 20;
        CHECK(a->DispatchState());
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Pos.z = 20;
        CHECK(a->m_MoveObj.Update());
        a->m_vTargetPos = {0, -8, -20};
        CHECK(g.objects.advance(.051));
        auto queries = a->ledge_queries;
        CHECK(a->DispatchState() && a->Metacmd == 4 && a->navigation_arrivals == 1 && a->ledge_queries == queries + 1 &&
              !a->ledge_sampled);
        CHECK(near(r->velocity.z, 20)); // Arrival faces stored target but does not stop.
        a->Metacmd = 2;
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateEscapeRun);
        return true;
    }
    bool failures(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        a->m_fSeeingDist = 100;
        CHECK(hint(g, {0, 10, 40}));
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun));
        g.server.collision_ready = false;
        CHECK(!a->DispatchState() && a->m_nState == AI_Mgr::StateEscapeRun && a->navigation_plan_failed);
        g.server.collision_ready = true;
        CHECK(a->DispatchState() && !a->navigation_plan_failed);
        CHECK(object(g, "path obstruction", {0, 10, 25}, {4, 5, 5}));
        a->m_vGuardPos = {0, 10, 100};
        CHECK(a->SetNewState(AI_Mgr::StateGuard) && a->DispatchState() && a->m_nState == AI_Mgr::StateIdle);
        CHECK(a->navigation_plan_failed && a->m_MoveObj.path().size() == 1);
        a->Metacmd = 3;
        CHECK(!a->AI_STATE_GuardLocation());
        auto table = std::make_shared<CAnim_Sound>();
        table->m_nAnim_TAUNT[6] = 9999;
        const auto playing = g.objects.get(a->m_hObject)->animation.animation;
        CHECK(a->InitStatics(table) && a->MC_Taunt_Beg() && a->animation_clip_retained);
        CHECK(g.objects.get(a->m_hObject)->animation.animation == playing);
        a->Metacmd = std::numeric_limits<int>::max();
        CHECK(!a->AI_STATE_Escape_Hide());
        return true;
    }
    bool tracking(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g, "tracked", {0, 10, 20}), other = object(g, "other", {0, 10, 30});
        CHECK(h && other);
        const auto owned_links = g.objects.link_count(a->m_hObject); // Inventory owns its muzzle-flash child.
        CHECK(a->SetTrackedObject(h) && a->RefreshTrackedPosition() && a->tracked_position_valid &&
              near(a->m_vTrackObjPos.z, 20));
        CHECK(g.objects.link_count(a->m_hObject) == owned_links); // Tracking adds no source engine link.
        a->m_dwFlags |= 8u;
        CHECK(a->SetNewState(AI_Mgr::StateIdle) && a->HandleTrigger(other, "SOUND_TRIGGER 1 1 2 3"));
        CHECK(!a->tracked_position_valid && a->GetTrackedObject() == h && near(a->m_vTrackObjPos.z, 3));
        CHECK(a->RefreshTrackedPosition() && a->tracked_position_valid && near(a->m_vTrackObjPos.z, 20));
        CHECK(a->SetNewTarget(h) && a->RefreshTargetPosition() && a->SetNewTarget(other));
        CHECK(a->GetTrackedObject() == h && g.objects.link_count(a->m_hObject) == owned_links + 1);
        CHECK(g.objects.remove(h) && !a->GetTrackedObject() && !a->RefreshTrackedPosition());
        CHECK(!a->tracked_position_valid && a->tracked_invalidations == 1 && near(a->m_vTrackObjPos.z, 20));
        CHECK(g.objects.finish_frame());
        auto replacement = object(g, "replacement", {0, 10, 50});
        CHECK(replacement && !a->GetTrackedObject());
        CHECK(a->SetTrackedObject(other) && a->RefreshTrackedPosition() && g.objects.remove(other) &&
              g.objects.finish_frame());
        CHECK(!a->GetTarget() && !a->GetTrackedObject() && a->tracked_invalidations == 2);
        CHECK(!a->SetTrackedObject(a->m_hObject) && !a->SetTrackedObject(h));
        CHECK(g.objects.clear() && ExitHint::m_dwNumExits == 0);
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
            a->record = true;
            a->remove = remove;
            a->recurse = !remove;
            CHECK(a->SetNewState(AI_Mgr::StateEscapeHide) && !a->DispatchState() && a->navigation_failures > 0);
            if (remove)
                CHECK(!g.server.IsObjectAlive(a->m_hObject));
            else
                CHECK(!a->nested_ok && a->decisions == 1);
            CHECK(g.objects.clear() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g));
        Config cfg;
        cfg.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(cfg, e));
        for (const char* name : {"m_cultist1", "f_cultist1", "m_civilian1", "f_civilian1", "labtech", "gideon"})
        {
            auto* a = actor(g);
            CHECK(a);
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load(std::string("models/enemies/") + name + ".abc", r->model, e));
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject) && a->InitStatics(table));
            const auto clip = table->m_nAnim_TAUNT[6];
            const bool ok = a->MC_Taunt_Beg();
            CHECK(ok == (clip < r->model->animations.size()));
            std::cout << name << " TAUNT[6]=" << clip << " available=" << ok
                      << " duration=" << (ok ? r->model->animations[clip].duration : 0) << '\n';
            CHECK(!ok || (r->animation.animation == clip && r->velocity.x > 0));
            CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> cases{
        {"beg", beg},           {"selection", selection},   {"facing", facing},
        {"routes", routes},     {"perception", perception}, {"hiding", hiding},
        {"guard", guard},       {"failures", failures},     {"tracking", tracking},
        {"lifetime", lifetime}, {"catalog", catalog}};
    auto t = cases.find(argv[1]);
    return t != cases.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
