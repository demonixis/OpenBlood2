#include "audio_fixture.h"
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
        PathMgr* paths = nullptr;
        bool driving = false, ok = true, reenter = false, remove_self = false, decision_ready = true;
        unsigned decisions = 0;
        bool reenter_decision = false;
        bool ComputeState(int = -1) override
        {
            ++decisions;
            if (reenter_decision)
                PrepareScript("SCRIPT; WAIT 0");
            if (!decision_ready)
                return AI_Mgr::ComputeState();
            return SetNewState(StateInactive);
        }
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_UPDATE && driving && paths)
            {
                ok = AI_STATE_Script(*paths);
                driving = ok && m_nState == StateScript;
                GetServerDE()->SetNextUpdate(m_hObject, driving ? .001f : 0);
            }
            return AI_Mgr::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD d) override
        {
            if (m == 1000 && reenter)
                PrepareScript("SCRIPT; WAIT 0");
            if (m == 1000 && remove_self)
                GetServerDE()->RemoveObject(m_hObject);
            return AI_Mgr::ObjectMessageFn(sender, m, d);
        }
    };
    Probe* actor(GameSession& g, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{"AIScriptProbe",
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
        a->paths = &g.server_shell.paths;
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
    bool parsing(const char*)
    {
        AI_Mgr::ScriptProgram p;
        std::string error;
        CHECK(AI_Mgr::ParseScript("sCrIpT INTERRUPTABLE; Wait \"  +2.8sec\"; BAD value", 9, p, error));
        CHECK(p.flags == 11 && p.commands.size() == 2 && p.commands[1].command == AI_Mgr::ScriptCommand::Done);
        CHECK(AI_Mgr::ParseScript("SCRIPT; WAIT nonsense", 15, p, error) && p.flags == 13);
        CHECK(AI_Mgr::ParseScript("SCRIPT -1; WAIT -2147483648", 0, p, error) && p.flags == UINT32_MAX);
        CHECK(AI_Mgr::ParseScript("SCRIPT 1junk; WAIT 0;", 0, p, error) && p.flags == 1);
        const auto old = p.commands.size();
        for (const std::string text : {"SCRIPT; WAIT", "SCRIPT; WAIT 0 1", "SCRIPT; WAIT 2147483648",
                                       "SCRIPT 999999999999999999999999", "SCRIPT; WAIT \"oops", "TARGET target"})
            CHECK(!AI_Mgr::ParseScript(text, 0, p, error) && !error.empty() && p.commands.size() == old);
        CHECK(!AI_Mgr::ParseScript("SCRIPT; WAIT " + std::string(50, '0'), 0, p, error));
        CHECK(!AI_Mgr::ParseScript(std::string_view("SCRIPT\0; WAIT 0", 15), 0, p, error));
        std::string many = "SCRIPT;";
        for (int i = 0; i < 64; ++i)
            many += "WAIT 0;";
        CHECK(!AI_Mgr::ParseScript(many, 0, p, error));
        CHECK(AI_Mgr::ParseScript("SCRIPT 0 ignored_header_argument", 0, p, error) && p.commands.empty());
        return true;
    }
    bool sequencing(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->PrepareScript("SCRIPT; SETMOVEMENT RUN; WAIT 0; SETMOVEMENT WALK;"));
        CHECK(a->script_selections == 0 && a->ScriptCommands().size() == 3); // No invented forced AI_Update.
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->m_eScriptMovement == AI_Mgr::SM_RUN);
        CHECK(a->script_selections == 1 && a->ScriptCommands().size() == 2);
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->script_selections == 2 && a->m_bUpdateScriptCmd);
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->m_eScriptMovement == AI_Mgr::SM_WALK);
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->decisions == 1 &&
              a->script_outcome == AI_Mgr::ScriptOutcome::Completed);
        CHECK(a->PrepareScript("SCRIPT; UNKNOWN anything; WAIT 1") && a->AI_STATE_Script(g.server_shell.paths));
        CHECK(a->decisions == 3 && a->ScriptCommands().size() == 1 &&
              a->m_dwScriptFlags == 0); // Original double decision.
        CHECK(a->PrepareScript("SCRIPT") && a->AI_STATE_Script(g.server_shell.paths) && a->decisions == 4);
        CHECK(!a->HandleTrigger(nullptr, "SCRIPT; WAIT 0")); // Full trigger remains gated.
        a->decision_ready = false;
        CHECK(a->PrepareScript("SCRIPT") && !a->AI_STATE_Script(g.server_shell.paths));
        CHECK(a->script_outcome == AI_Mgr::ScriptOutcome::Failed);
        return true;
    }
    bool waiting(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->PrepareScript("SCRIPT; WAIT \"+1.9seconds\"; SETMOVEMENT RUN"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->script_wait_end == 1 && !a->m_bUpdateScriptCmd);
        CHECK(g.step(.5f) && a->AI_STATE_Script(g.server_shell.paths) && !a->m_bUpdateScriptCmd);
        CHECK(g.step(.5f) && a->AI_STATE_Script(g.server_shell.paths) && a->m_bUpdateScriptCmd);
        CHECK(a->script_selections == 1 && a->AI_STATE_Script(g.server_shell.paths) && a->script_selections == 2);
        for (const char* arg : {"-1", "0.9", "no_number"})
        {
            CHECK(a->PrepareScript(std::string("SCRIPT; WAIT ") + arg));
            CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->m_bUpdateScriptCmd);
        }
        CHECK(a->PrepareScript("SCRIPT 1; SETMOVEMENT WALK; SETMOVEMENT RUN"));
        for (int i = 0; i < 100; ++i)
        {
            CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->ScriptCommands().size() == 2);
            CHECK(a->script_index == unsigned((i + 1) % 2) &&
                  a->m_eScriptMovement == (i % 2 ? AI_Mgr::SM_RUN : AI_Mgr::SM_WALK));
        }
        CHECK(a->PrepareScript("SCRIPT 1; WAIT 1"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && !a->m_bUpdateScriptCmd);
        CHECK(!a->PrepareScript("SCRIPT; WAIT 0; WAIT") && a->ScriptCommands().size() == 1);
        CHECK(!a->AI_STATE_Script(g.server_shell.paths)); // Failure latches; valid preparation recovers.
        return true;
    }
    bool animations(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        CHECK(a->PrepareScript("SCRIPT; PLAYANIMATION clip0; SETMOVEMENT RUN"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && !r->animation.looping && !a->m_bUpdateScriptCmd);
        CHECK(g.step(.5f) && g.step(.5f));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->m_bUpdateScriptCmd && a->script_selections == 1);
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->m_eScriptMovement == AI_Mgr::SM_RUN);
        CHECK(a->PrepareScript("SCRIPT 1; PLAYANIMATIONLOOPING clip1; WAIT 50"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && r->animation.looping && a->decisions == 1);
        CHECK(a->script_outcome == AI_Mgr::ScriptOutcome::Completed && a->m_dwScriptFlags == 0 &&
              a->ScriptCommands().size() == 2);
        CHECK(a->PrepareScript("SCRIPT; PLAYANIMATION missing") && !a->AI_STATE_Script(g.server_shell.paths));
        CHECK(r->animation.looping && r->animation.animation == 1);
        CHECK(a->PrepareScript("SCRIPT; PLAYANIMATION clip0") && a->AI_STATE_Script(g.server_shell.paths));
        r->model.reset();
        CHECK(!a->AI_STATE_Script(g.server_shell.paths));
        return true;
    }
    bool state_target(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        const auto h = object(g, "named target", {20, 10, 0});
        CHECK(h);
        CHECK(a->PrepareScript("SCRIPT; TARGET \"named target\"; WAIT 0"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->GetTarget() == h && a->m_nState == AI_Mgr::StateAttackFar);
        CHECK(a->script_outcome == AI_Mgr::ScriptOutcome::StateChanged && a->ScriptCommands().size() == 1);
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->script_selections == 1);
        // Source SetTargetCmd: an unmatched name keeps the target and still enters AttackFar.
        CHECK(a->PrepareScript("SCRIPT; TARGET absent") && a->AI_STATE_Script(g.server_shell.paths) &&
              a->GetTarget() == h && a->m_nState == AI_Mgr::StateAttackFar && a->target_misses == 1);
        CHECK(a->PrepareScript("SCRIPT; SETSTATE PASSIVE; WAIT 0") && a->AI_STATE_Script(g.server_shell.paths));
        CHECK(a->m_nState == AI_Mgr::StatePassive && a->ScriptCommands().size() == 1);
        CHECK(a->PrepareScript("SCRIPT 2; WAIT 50"));
        CHECK(a->HandleTrigger(h, "SETSTATE INACTIVE") && a->AI_STATE_Script(g.server_shell.paths));
        CHECK(a->script_outcome == AI_Mgr::ScriptOutcome::StateChanged);
        return true;
    }
    bool movement(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(point(g, "route1", {0, 10, 3}));
        CHECK(g.server_shell.paths.BuildPathList());
        CHECK(object(g, "destination", {0, 10, 7}));
        a->m_fRunSpeed = 10;
        CHECK(a->PrepareScript(
            "SCRIPT; SETMOVEMENT RUN; FOLLOWPATH route; WAIT 0; MOVETOOBJECT destination; SETSTATE INACTIVE"));
        a->driving = true;
        g.server.SetNextUpdate(a->m_hObject, .001f);
        for (int i = 0; i < 100 && a->driving; ++i)
            CHECK(g.step(.1f) && a->ok);
        CHECK(!a->driving && a->script_selections == 5 && a->path_arrivals == 2 &&
              a->m_nState == AI_Mgr::StateInactive);
        CHECK(g.objects.get(a->m_hObject)->state.m_Pos.z >= 7 && near(g.objects.get(a->m_hObject)->velocity.z, 0));
        CHECK(a->PrepareScript("SCRIPT; FOLLOWPATH absent") && !a->AI_STATE_Script(g.server_shell.paths));
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
            CHECK(point(g, "route1", {0, 10, 0}, {{"ActionMessage", std::string("SETSTATE INACTIVE")}}));
            CHECK(g.server_shell.paths.BuildPathList());
            a->remove_self = remove;
            a->reenter = !remove;
            CHECK(a->PrepareScript("SCRIPT; FOLLOWPATH route; WAIT 0"));
            CHECK(!a->AI_STATE_Script(g.server_shell.paths));
            CHECK(a->script_outcome == AI_Mgr::ScriptOutcome::Failed && a->script_selections == 1);
            CHECK(!a->AI_STATE_Script(g.server_shell.paths) && a->script_selections == 1);
            CHECK(g.objects.finish_frame() && g.server.message_count() == 0);
            CHECK(g.unload_world() && g.server.string_count() == 0);
        }
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && a->PrepareScript("SCRIPT"));
        a->reenter_decision = true;
        CHECK(!a->AI_STATE_Script(g.server_shell.paths));
        CHECK(a->script_outcome == AI_Mgr::ScriptOutcome::Failed && a->decisions == 1);
        return true;
    }
    bool audio(const char* root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->PrepareScript("SCRIPT; PLAYSOUND long.wav; WAIT 0"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->script_sounds == 1 && a->m_bUpdateScriptCmd);
        CHECK(g.server.audio.voices().size() == 1);
        const auto& v = g.server.audio.voices().begin()->second;
        CHECK(v.request.outer_radius == 5000 && v.request.inner_radius == 500 && v.request.priority == 2 &&
              v.auto_remove);
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->m_bUpdateScriptCmd); // Does not wait for voice completion.
        CHECK(a->PrepareScript("SCRIPT; PLAYSOUND absent.wav") && !a->AI_STATE_Script(g.server_shell.paths));
        CHECK(a->script_sounds == 1 && !a->AI_STATE_Script(g.server_shell.paths));
        CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.string_count() == 0);
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
        CHECK(g.request_world("01_SubwayA", error));
        unsigned scripts = 0, failed = 0, instructions = 0;
        for (const auto& o : g.world.objects)
            for (const auto& prop : o.properties)
                if (const auto* text = std::get_if<std::string>(&prop.value))
                {
                    auto upper = *text;
                    for (auto& ch : upper)
                        if (ch >= 'a' && ch <= 'z')
                            ch -= 'a' - 'A';
                    if (!upper.starts_with("SCRIPT"))
                        continue;
                    AI_Mgr::ScriptProgram p;
                    ++scripts;
                    const bool ok = AI_Mgr::ParseScript(*text, 0, p, error);
                    if (!ok)
                        ++failed;
                    else
                        instructions += p.commands.size();
                    std::cout << o.class_name << '.' << prop.name << " parsed=" << ok
                              << " commands=" << p.commands.size() << " error=" << error << '\n';
                }
        CHECK(scripts == 6 && failed == 0 && instructions == 12);
        std::cout << "SubwayA authored scripts=" << scripts << " rejected=" << failed
                  << " parsed instructions=" << instructions << '\n';
        CHECK(g.unload_world() && setup(g));
        for (const char* name : {"m_cultist1", "f_cultist1", "m_civilian1", "f_civilian1", "labtech", "gideon"})
        {
            auto* a = actor(g, {0, 10000, 0});
            CHECK(a);
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load(std::string("models/enemies/") + name + ".abc", r->model, error));
            const auto clip = r->model->animations.front().name;
            CHECK(a->PrepareScript("SCRIPT; PLAYANIMATION \"" + clip + "\"; SETSTATE INACTIVE"));
            CHECK(a->AI_STATE_Script(g.server_shell.paths));
            for (int i = 0; i < 100 && a->m_nState == AI_Mgr::StateScript; ++i)
                CHECK(g.step(.1f) && a->AI_STATE_Script(g.server_shell.paths));
            CHECK(a->m_nState == AI_Mgr::StateInactive && a->script_failures == 0);
            std::cout << name << " clip=" << clip << " completed=1\n";
            CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{{"parsing", parsing},
                                                                        {"sequencing", sequencing},
                                                                        {"waiting", waiting},
                                                                        {"animations", animations},
                                                                        {"state_target", state_target},
                                                                        {"movement", movement},
                                                                        {"lifetime", lifetime},
                                                                        {"audio", audio},
                                                                        {"catalog", catalog}};
    const auto i = tests.find(argv[1]);
    return i != tests.end() && i->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
