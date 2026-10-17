#include "brush_fixture.h"
#include "game/original/AI_Mgr.h"
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
    struct Probe : AI_Mgr
    {
        bool driving = false, ok = true, remove_on_touch = false, reenter_on_touch = false;
        unsigned recursive_touches = 0;
        unsigned self_actions = 0;
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_TOUCHNOTIFY && remove_on_touch)
                GetServerDE()->RemoveObject(m_hObject);
            if (m == MID_TOUCHNOTIFY && reenter_on_touch)
            {
                ++recursive_touches;
                UpdateMoveToObjectCmd();
            }
            if (m == MID_UPDATE && driving)
            {
                ok = script_motion_command == MotionCommand::FollowPath ? UpdateFollowPathCmd()
                                                                        : UpdateMoveToObjectCmd();
                driving = ok && !m_bUpdateScriptCmd;
                GetServerDE()->SetNextUpdate(m_hObject, driving ? .001f : 0);
            }
            return AI_Mgr::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD d) override
        {
            if (m == 1000)
                ++self_actions;
            return AI_Mgr::ObjectMessageFn(sender, m, d);
        }
    };
    Probe* actor(GameSession& g, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{"AIFollowProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<Probe*>(r->game.get()) : nullptr;
        if (!a)
            return nullptr;
        auto model = std::make_shared<Model>();
        std::string error;
        if (!read_model(test_model::abc().b, *model, error))
            return nullptr;
        auto clip = model->animations.front();
        model->animations.clear();
        for (unsigned i = 0; i < 22; ++i)
        {
            model->animations.push_back(clip);
            auto& c = model->animations.back();
            c.name = "clip" + std::to_string(i);
            c.dimensions = {1, 2, 1};
            c.keys.clear();
        }
        r->model = model;
        r->dimensions = {1, 2, 1};
        auto table = std::make_shared<CAnim_Sound>();
        for (unsigned i = 0; i < 9; ++i)
        {
            table->m_nAnim_WALK[i] = i;
            table->m_nAnim_RUN[i] = 9 + i;
        }
        table->m_nAnim_WALK_INJURED_LLEG_PISTOL = 18;
        table->m_nAnim_WALK_INJURED_RLEG_PISTOL = 19;
        table->m_nAnim_WALK_INJURED_LLEG_RIFLE = 20;
        table->m_nAnim_WALK_INJURED_RLEG_RIFLE = 21;
        if (!a->InitStatics(table) || !a->m_MoveObj.Init(h))
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
    HOBJECT point(GameSession& g, const char* name, DVector pos, Properties props = {})
    {
        ObjectCreateStruct s;
        SAFE_STRCPY(s.m_Name, name);
        s.m_Pos = pos;
        props["Name"] = std::string(name);
        props["Pos"] = pos;
        return g.objects.create(g.objects.find_class("PathPoint"), s, Creation::World, props);
    }
    bool select(GameSession& g, Probe* a, const char* name = "route")
    {
        CHECK(g.server_shell.paths.BuildPathList());
        CHECK(a->SetFollowPathCmd(g.server_shell.paths, name));
        return true;
    }
    bool velocity(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        CHECK(a->Move({2, 3, 4}, -2) && near(r->velocity.z, -8) && near(r->state.m_Pos.z, 0));
        CHECK(a->Move({2, 3, 4}, 1e-11f) && near(r->velocity.z, 4));
        CHECK(a->Move({2, 3, 4}, 2e-11f) && std::abs(r->velocity.z) < 1e-9f);
        a->m_nTrapped = 1;
        CHECK(a->Move({2, 3, 4}, 9) && near(r->velocity.y, 0));
        CHECK(!a->Move({1, 0, 0}, std::numeric_limits<float>::infinity()));
        a->m_nTrapped = 0;
        CHECK(!a->Move({1e7f, 0, 0}, 2));
        CHECK(a->velocity_commands == 4);
        return true;
    }
    bool locomotion(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        a->m_fWalkSpeed = 30;
        a->m_fRunSpeed = 60;
        a->m_nInjuredLeg = 10;
        a->Metacmd = 7;
        CHECK(a->Script_Walk() && g.server.GetModelAnimation(a->m_hObject) == TYPE_MELEE && near(r->velocity.z, 10));
        CHECK(a->Script_Walk() && near(r->velocity.z, 30) && a->Metacmd == 7);
        CHECK(a->m_InventoryMgr.Init(a->m_hObject, nullptr));
        CHECK(a->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA, 0) == InventoryResult::Ok);
        a->m_bAnimating = false;
        CHECK(a->Script_Walk() && g.server.GetModelAnimation(a->m_hObject) == 18 && near(r->velocity.z, 10));
        a->m_nInjuredLeg = 14;
        a->m_bAnimating = false;
        CHECK(a->Script_Walk() && g.server.GetModelAnimation(a->m_hObject) == 19);
        CHECK(a->Script_Run() && g.server.GetModelAnimation(a->m_hObject) == 9 + TYPE_PISTOL &&
              near(r->velocity.z, 60));
        g.server.SetModelLooping(a->m_hObject, DFALSE);
        CHECK(g.step(.5f));
        CHECK(g.step(.5f));
        CHECK(a->Script_Run() && !a->m_bAnimating && near(r->velocity.z, 0) && a->Metacmd == 7);
        CHECK(a->SetSetMovementCmd("rUn") && a->m_eScriptMovement == AI_Mgr::SM_RUN);
        CHECK(a->SetSetMovementCmd(" WALK") && a->m_eScriptMovement == AI_Mgr::SM_RUN);
        CHECK(a->SetSetMovementCmd("walk") && a->m_eScriptMovement == AI_Mgr::SM_WALK);
        auto missing = std::make_shared<CAnim_Sound>();
        missing->m_nAnim_WALK[TYPE_PISTOL] = 9999;
        CHECK(a->InitStatics(missing));
        a->m_nInjuredLeg = 0;
        CHECK(!a->Script_Walk() && !a->script_motion_error.empty());
        return true;
    }
    bool follow(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(g.objects.advance(.1f));
        CHECK(point(g, "route1", {0, 900, 2}));
        auto source = point(g, "route2", {0, -900, 1});
        CHECK(source && select(g, a));
        CHECK(a->ScriptPath().size() == 2 && a->ScriptPath()[0].name == "route2");
        CHECK(g.objects.remove(source) && g.objects.finish_frame());
        g.server_shell.paths.ClearPathList();
        CHECK(a->Move({0, 0, 1}, 3));
        CHECK(a->UpdateFollowPathCmd());
        auto* r = g.objects.get(a->m_hObject);
        CHECK(near(r->state.m_Pos.z, 1) && near(r->state.m_Pos.y, 10));
        CHECK(a->path_arrivals == 1 && !a->m_bUpdateScriptCmd && a->ScriptPath().size() == 1 && near(r->velocity.z, 3));
        CHECK(a->m_MoveObj.Update() && a->UpdateFollowPathCmd() && a->path_arrivals == 2 && !a->m_bUpdateScriptCmd);
        CHECK(a->UpdateFollowPathCmd() && a->m_bUpdateScriptCmd && near(r->velocity.z, 3));
        // Exact coincident point is defined to preserve heading and still consume the point.
        CHECK(point(g, "zero1", r->state.m_Pos) && select(g, a, "zero") && a->m_MoveObj.Update());
        CHECK(a->UpdateFollowPathCmd() && a->ScriptPath().empty());
        return true;
    }
    bool cached(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(g.objects.advance(.1f));
        CHECK(point(g, "route1", {100, 10, 0}) && select(g, a));
        CHECK(a->UpdateFollowPathCmd());
        auto* r = g.objects.get(a->m_hObject);
        CHECK(near(r->velocity.x, 0) &&
              near(r->velocity.z, 5)); // Facing changes current rotation; velocity reads preceding cache.
        CHECK(a->m_MoveObj.Update() && a->UpdateFollowPathCmd() && near(r->velocity.x, 5) && near(r->velocity.z, 0));
        a->m_nTrapped = 1;
        CHECK(a->UpdateFollowPathCmd() && near(r->velocity.x, 0));
        r->state.m_Pos = {99, 10, 0};
        CHECK(a->m_MoveObj.Update() && a->UpdateFollowPathCmd());
        CHECK(near(r->state.m_Pos.x, 99) && a->path_arrivals == 1);
        return true;
    }
    bool moveto(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(g.objects.advance(.1f));
        CHECK(object(g, "goal", {0, 700, 100}));
        auto first = object(g, "goal", {0, 700, 1});
        CHECK(first); // FindNamedObjects prepends: newest matching object is first.
        CHECK(a->SetMoveToObjectCmd("goal") && a->GetTarget() == first && a->Metacmd == 1);
        CHECK(a->UpdateMoveToObjectCmd() && a->Metacmd == 2);
        CHECK(a->UpdateMoveToObjectCmd() && a->Metacmd == 3);
        CHECK(a->UpdateMoveToObjectCmd() && a->Metacmd == 4 && a->path_arrivals == 1);
        g.objects.get(first)->state.m_Pos = {0, 800, 5};
        CHECK(a->UpdateMoveToObjectCmd() && a->m_bUpdateScriptCmd &&
              near(g.objects.get(a->m_hObject)->state.m_Pos.z, 5));
        CHECK(a->SetMoveToObjectCmd("absent") && a->m_bUpdateScriptCmd && a->GetTarget() == first);
        CHECK(a->UpdateMoveToObjectCmd() && near(g.objects.get(a->m_hObject)->state.m_Pos.z, 5));
        CHECK(a->SetMoveToObjectCmd("goal"));
        CHECK(g.objects.remove(first) && g.objects.finish_frame());
        CHECK(!a->GetTarget() && a->UpdateMoveToObjectCmd() && a->m_bUpdateScriptCmd);
        return true;
    }
    struct Receiver : BaseClass
    {
        AI_Mgr* caller = nullptr;
        HOBJECT remove = nullptr;
        bool recurse = false;
        unsigned calls = 0;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD m, HMESSAGEREAD) override
        {
            if (m == 1000)
            {
                ++calls;
                if (remove)
                    GetServerDE()->RemoveObject(remove);
                if (recurse)
                    caller->UpdateFollowPathCmd();
            }
            return 1;
        }
    };
    Receiver* receiver(GameSession& g, const char* name)
    {
        static const ClassInfo type{"AIFollowReceiver",
                                    g.objects.find_class("BaseClass"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Receiver>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        SAFE_STRCPY(s.m_Name, name);
        return dynamic_cast<Receiver*>(g.server.HandleToObject(g.objects.create(&type, s)));
    }
    bool actions(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* one = receiver(g, "group");
        auto* two = receiver(g, "group");
        CHECK(one && two);
        two->remove = one->m_hObject;
        CHECK(point(g, "route1", {0, 90, 0},
                    {{"ActionTarget", std::string("group")}, {"ActionMessage", std::string("ON")}}) &&
              select(g, a));
        auto strings = g.server.string_count();
        CHECK(a->UpdateFollowPathCmd() && one->calls == 0 && two->calls == 1 && a->path_deliveries == 1);
        CHECK(g.server.string_count() == strings && g.server.message_count() == 0);
        CHECK(point(g, "self1", {0, 0, 0}, {{"ActionMessage", std::string("REMOVE")}}) && select(g, a, "self"));
        CHECK(a->UpdateFollowPathCmd() && a->self_actions == 1 && g.server.IsObjectAlive(a->m_hObject) &&
              a->path_removals == 0);
        CHECK(point(g, "remove1", {0, 0, 0},
                    {{"ActionTarget", std::string("group")}, {"ActionMessage", std::string("rEmOvE")}}) &&
              select(g, a, "remove"));
        CHECK(a->UpdateFollowPathCmd() && a->path_removals == 1 && !g.server.IsObjectAlive(two->m_hObject));
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 4; ++cycle)
        {
            auto* a = actor(g);
            CHECK(a && !g.objects.get(old));
            old = a->m_hObject;
            auto* r = receiver(g, "callback");
            CHECK(r);
            r->caller = a;
            r->recurse = cycle % 2 == 0;
            r->remove = r->recurse ? nullptr : old;
            CHECK(point(g, "route1", {0, 0, 0},
                        {{"ActionTarget", std::string("callback")}, {"ActionMessage", std::string("ON")}}) &&
                  select(g, a));
            CHECK(!a->UpdateFollowPathCmd() && a->script_motion_failures > 0 && r->calls == 1 &&
                  g.server.message_count() == 0);
            CHECK(!a->UpdateFollowPathCmd() && r->calls == 1); // Latch prevents replaying an action after failure.
            CHECK(g.unload_world() && !g.objects.get(old) && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
            CHECK(setup(g));
        }
        auto* touching = actor(g);
        CHECK(touching);
        touching->remove_on_touch = true;
        touching->m_fWalkSpeed = 20;
        auto h = touching->m_hObject;
        g.objects.get(h)->state.m_Flags |= FLAG_TOUCH_NOTIFY;
        CHECK(object(g, "touch wall", {0, 10, 3}, {5, 5, 1}));
        CHECK(point(g, "touch1", {0, 10, 2}) && select(g, touching, "touch") && g.objects.advance(.1f));
        CHECK(!touching->UpdateFollowPathCmd() && !g.server.IsObjectAlive(h));
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        auto* recursive = actor(g, {100, 10, 0});
        CHECK(recursive);
        recursive->reenter_on_touch = true;
        recursive->m_fWalkSpeed = 20;
        g.objects.get(recursive->m_hObject)->state.m_Flags |= FLAG_TOUCH_NOTIFY;
        CHECK(object(g, "recursive wall", {100, 10, 3}, {5, 5, 1}));
        CHECK(object(g, "recursive goal", {100, 10, 2}) && recursive->SetMoveToObjectCmd("recursive goal"));
        recursive->Metacmd = 4; // Isolate the source final-snap callback boundary.
        CHECK(!recursive->UpdateMoveToObjectCmd() && recursive->script_motion_outcome == AI_Mgr::MotionOutcome::Failed);
        CHECK(!recursive->m_bUpdateScriptCmd && recursive->recursive_touches > 0 &&
              recursive->script_motion_failures == recursive->recursive_touches);
        CHECK(g.unload_world() && g.server.message_count() == 0 && g.server.string_count() == 0);
        return true;
    }
    bool failures(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(!a->UpdateFollowPathCmd());
        CHECK(!a->SetFollowPathCmd(g.server_shell.paths, "missing"));
        CHECK(point(g, "route1", {0, 10, 100}) && select(g, a));
        a->m_dwScriptFlags = 4;
        CHECK(a->UpdateFollowPathCmd() && !a->script_fire_engaged && a->velocity_commands == 1);
        a->m_dwScriptFlags = 0;
        CHECK(select(g, a));
        a->m_fWalkSpeed = std::numeric_limits<float>::quiet_NaN();
        CHECK(!a->UpdateFollowPathCmd());
        a->m_fWalkSpeed = 5;
        auto target = object(g, "goal", {0, 10, 100});
        CHECK(target && object(g, "wall", {0, 10, 25}, {4, 5, 5}));
        CHECK(a->SetMoveToObjectCmd("goal") && !a->UpdateMoveToObjectCmd() && a->m_MoveObj.path().size() == 1);
        CHECK(!a->UpdateMoveToObjectCmd() &&
              a->velocity_commands == 1); // Partial source route must not become success.
        CHECK(a->SetMoveToObjectCmd("goal") && g.objects.remove(target) && !a->UpdateMoveToObjectCmd());
        CHECK(!a->HandleTrigger(nullptr, "SCRIPT FOLLOWPATH route"));
        auto* b = actor(g, {100, 10, 0});
        CHECK(b);
        b->m_MoveObj.initialized = false;
        CHECK(!b->Script_Run());
        CHECK(b->m_MoveObj.Init(b->m_hObject));
        auto* body = g.objects.get(b->m_hObject);
        auto tall = std::make_shared<Model>(*body->model);
        tall->animations[9 + TYPE_MELEE].dimensions = {1, 20, 1};
        body->model = tall;
        CHECK(object(g, "ceiling", {100, 20, 0}, {5, 1, 5}));
        CHECK(!b->Script_Run() && b->animation_blocked && !b->m_bAnimating && near(body->velocity.z, 0));
        return true;
    }
    bool physics(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        a->m_fWalkSpeed = 10;
        CHECK(point(g, "route1", {0, 10, 100}) && select(g, a));
        a->driving = true;
        g.server.SetNextUpdate(a->m_hObject, .001f);
        CHECK(g.step(.1f) && a->ok && near(r->state.m_Pos.z, 1));
        CHECK(g.step(.1f) && near(r->state.m_Pos.z, 2));
        auto* b = actor(g, {100, 10, 0});
        CHECK(b);
        CHECK(object(g, "physical goal", {100, 10, 5}));
        b->m_fRunSpeed = 10;
        CHECK(b->SetSetMovementCmd("RUN") && b->SetMoveToObjectCmd("physical goal"));
        b->driving = true;
        g.server.SetNextUpdate(b->m_hObject, .001f);
        for (int i = 0; i < 30 && b->driving; ++i)
            CHECK(g.step(.1f) && b->ok);
        CHECK(b->m_bUpdateScriptCmd && b->path_arrivals == 1 && b->velocity_commands > 0);
        CHECK(g.objects.get(b->m_hObject)->state.m_Pos.z >= 5 && near(g.objects.get(b->m_hObject)->velocity.z, 10));
        DVector reset{0, 10, 2};
        CHECK(g.server.MoveObject(a->m_hObject, &reset) == DE_OK && a->m_MoveObj.Update());
        CHECK(object(g, "wall", {0, 10, 10}, {5, 5, 1}));
        for (int i = 0; i < 30; ++i)
            CHECK(g.step(.1f) && a->ok);
        CHECK(r->state.m_Pos.z < 8.01f && r->state.m_Pos.z > 7.8f && a->path_arrivals == 0);
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
            auto* a = actor(g, {0, 10000, 0});
            CHECK(a);
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load(std::string("models/enemies/") + name + ".abc", r->model, error));
            auto table = std::make_shared<CAnim_Sound>();
            CHECK(table->SetAnimationIndexes(a->m_hObject) && a->InitStatics(table));
            CHECK(a->m_InventoryMgr.Init(a->m_hObject, nullptr) &&
                  a->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA, 0) == InventoryResult::Ok);
            for (int mode = 0; mode < 4; ++mode)
            {
                a->m_bAnimating = false;
                a->m_nInjuredLeg = mode == 2 ? 10 : mode == 3 ? 14 : 0;
                unsigned clip = mode == 0   ? table->m_nAnim_WALK[TYPE_PISTOL]
                                : mode == 1 ? table->m_nAnim_RUN[TYPE_PISTOL]
                                : mode == 2 ? table->m_nAnim_WALK_INJURED_LLEG_PISTOL
                                            : table->m_nAnim_WALK_INJURED_RLEG_PISTOL;
                const bool present = clip < r->model->animations.size();
                CHECK((mode == 1 ? a->Script_Run() : a->Script_Walk()) == present);
                std::cout << name << " mode=" << mode << " clip=" << clip << " available=" << present << '\n';
            }
            CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        }
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p);
            auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
            CHECK(!p->m_MoveObj.initialized);
            CHECK(g.leave_player()); // Free the actual spawn volume before the diagnostic body enters it.
            auto* a = actor(g, pos);
            CHECK(a);
            CHECK(point(g, "diagnostic1", vector_add(pos, {0, 0, .05f})) && select(g, a, "diagnostic"));
            CHECK(g.objects.advance(.1f));
            const bool moved = a->UpdateFollowPathCmd();
            if (!moved)
                std::cerr << a->script_motion_error << " distance=" << a->script_motion_distance << "\n";
            CHECK(moved && a->path_arrivals == 1);
            CHECK(a->UpdateFollowPathCmd() && a->m_bUpdateScriptCmd);
            std::cout << "SubwayA cycle=" << cycle << " path arrival=" << a->path_arrivals << '\n';
            CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"velocity", velocity}, {"locomotion", locomotion}, {"follow", follow},     {"cached", cached},
        {"moveto", moveto},     {"actions", actions},       {"lifetime", lifetime}, {"failures", failures},
        {"physics", physics},   {"catalog", catalog},       {"subway", subway}};
    auto i = tests.find(argv[1]);
    return i != tests.end() && i->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
