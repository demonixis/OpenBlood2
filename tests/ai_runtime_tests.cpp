#include "brush_fixture.h"
#include "game/original/CameraObj.h"
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
        return std::abs(a - b) < .003f;
    }
    bool setup(GameSession& g, const char* root = "")
    {
        CHECK(g.initialize());
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string e;
        CHECK(g.collision.build(floor, e));
        g.server.collision_ready = true;
        if (*root)
        {
            Config cfg;
            cfg.data_dir = root;
            CHECK(g.resources.mount_game(cfg, e));
        }
        return true;
    }
    HOBJECT object(GameSession& g, DVector pos = {0, 10, 40}, const char* name = "object",
                   const char* type = "BaseClass")
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        SAFE_STRCPY(s.m_Name, name);
        auto h = g.objects.create(g.objects.find_class(type), s);
        if (h)
            g.objects.get(h)->dimensions = {1, 2, 1};
        return h;
    }
    class Probe : public CivilianAI
    {
      public:
        bool remove = false, recurse = false, nested = true;
        bool ComputeState(int = -1) override
        {
            if (remove)
                GetServerDE()->RemoveObject(m_hObject);
            if (recurse)
                nested = AI_Update();
            return true;
        }
    };
    // A deliberately synthetic scheduler fixture: real AI_Update/state/engine services,
    // but injected animation data/initialized flag. Catalog tests cover actual initialization.
    Probe* actor(GameSession& g)
    {
        static const ClassInfo type{"RuntimeProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = {0, 10, 0};
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<Probe*>(r->game.get()) : nullptr;
        if (!a)
            return nullptr;
        auto model = std::make_shared<Model>();
        std::string e;
        if (!read_model(test_model::abc().b, *model, e))
            return nullptr;
        model->animations[0].keys.clear();
        model->animations[0].dimensions = {1, 2, 1};
        r->model = model;
        r->dimensions = {1, 2, 1};
        auto table = std::make_shared<CAnim_Sound>();
        for (auto& clip : table->m_nAnim_IDLE)
            clip = 0;
        if (!a->InitStatics(table) || !a->m_MoveObj.Init(h) || !a->m_InventoryMgr.Init(h, nullptr))
            return nullptr;
        a->m_damage.Init(h);
        a->m_damage.SetHitPoints(100);
        a->m_vDims = r->dimensions;
        a->npc_type = AI_Mgr::NPCType::Civilian;
        a->runtime_initialized = true;
        a->m_nState = AI_Mgr::StateInactive;
        g.server.SetNextUpdate(h, .001f);
        return a;
    }
    bool activation(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        auto observer = object(g, {0, 10, 50});
        CHECK(observer);
        CHECK(g.step(.01) && a->runtime_updates == 1 && a->m_bSetShutdown);
        auto* r = g.objects.get(h);
        CHECK(r->activity == OBJSTATE_AUTODEACTIVATE_NOW && r->scheduled);
        CHECK(g.step(.5) && a->runtime_updates == 1 && g.server.PingObjects(observer));
        CHECK(r->activity == OBJSTATE_ACTIVE && r->activation_pings == 1);
        CHECK(g.step(.01) && a->runtime_updates == 2 && a->runtime_desyncs == 0);
        CHECK(g.step(3) && r->activity == OBJSTATE_AUTODEACTIVATE_NOW && a->runtime_updates == 2);
        CHECK(g.server.PingObjects(observer) && g.step(.01) && a->runtime_desyncs == 1);
        CHECK(r->due > g.objects.time() && r->due <= g.objects.time() + .5);
        CHECK(g.step(.5) && a->runtime_updates == 4);
        g.server.SetObjectState(h, OBJSTATE_INACTIVE);
        CHECK(g.server.PingObjects(observer) && r->activity == OBJSTATE_INACTIVE);
        CHECK(!g.objects.engine_message(h, MID_TOUCHNOTIFY, observer));
        g.server.SetObjectState(h, OBJSTATE_INACTIVE_TOUCH);
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, observer));
        CHECK(g.server.SetDeactivationTime(h, .25f));
        g.server.SetObjectState(h, OBJSTATE_ACTIVE);
        CHECK(r->auto_deactivate && g.step(.25) && r->activity == OBJSTATE_AUTODEACTIVATE_NOW);
        CHECK(!g.objects.engine_message(h, MID_TOUCHNOTIFY, observer));
        CHECK(!g.server.SetDeactivationTime(h, -1) && !g.server.SetDeactivationTime(h, NAN));
        CHECK(g.server.SetDeactivationTime(h, 0) && !r->auto_deactivate);
        return true;
    }
    bool visibility(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto observer = object(g, {0, 10, 50});
        auto h = object(g, {0, 10, 0});
        auto blocker = object(g, {0, 10, 25});
        CHECK(observer && h && blocker);
        CHECK(g.server.SetDeactivationTime(h, 3));
        g.server.SetObjectState(h, OBJSTATE_AUTODEACTIVATE_NOW);
        CHECK(g.server.PingObjects(observer) && g.objects.get(h)->activation_pings == 0);
        CHECK(g.objects.remove(blocker) && g.server.PingObjects(observer) && g.objects.get(h)->activation_pings == 1);
        auto pings = g.objects.get(h)->activation_pings;
        g.server.collision_ready = false;
        CHECK(!g.server.PingObjects(observer) && g.objects.get(h)->activation_pings == pings);
        CHECK(g.objects.remove(observer) && !g.server.PingObjects(observer) && g.objects.finish_frame());
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
            a->m_nState = AI_Mgr::StateEscapeHide;
            a->remove = remove;
            a->recurse = !remove;
            auto h = a->m_hObject;
            CHECK(!a->AI_Update());
            if (remove)
                CHECK(!g.server.IsObjectAlive(h));
            else
                CHECK(a->runtime_failed && !a->nested && g.objects.get(h)->activity == OBJSTATE_INACTIVE &&
                      !g.objects.get(h)->scheduled);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
        }
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        a->m_bRemoveMe = true;
        CHECK(g.step(.01) && !g.objects.get(h));
        return true;
    }
    bool targets(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g, {0, 10, 50}, "target", "CBaseCharacter");
        CHECK(h);
        auto* c = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
        CHECK(c);
        c->m_damage.Init(h);
        c->m_damage.SetHitPoints(100);
        a->m_fSensingDist = 20;
        a->m_fSeeingDist = 100;
        CHECK(a->SetNewTarget(h) && a->AI_Update() && a->GetTarget() == h && a->target_position_valid);
        CHECK(a->m_bHasTargeted && a->m_fSensingDist == 100 && near(a->m_vTargetPos.z, 50));
        auto blocker = object(g, {0, 10, 25});
        CHECK(blocker);
        // ALWAYSCLEAR bypasses sight, but never the checked current pose/character type.
        a->m_dwFlags |= 0x100;
        CHECK(a->AI_Update() && a->GetTarget() == h);
        a->m_dwFlags &= ~0x100u;
        CHECK(a->AI_Update() && !a->GetTarget() && a->runtime_target_losses == 1);
        CHECK(g.objects.remove(blocker) && g.objects.finish_frame());
        a->m_dwFlags |= 0x100;
        CHECK(a->SetNewTarget(h));
        c->m_damage.m_bDead = DTRUE;
        CHECK(a->AI_Update() && !a->GetTarget() && a->runtime_target_losses == 2);
        a->m_nState = AI_Mgr::StateInactive;
        c->m_damage.m_bDead = DFALSE;
        CHECK(a->SetNewTarget(h) && g.objects.remove(h) && a->AI_Update() && !a->GetTarget());
        CHECK(g.objects.finish_frame());
        auto wrong = object(g, {10, 10, 20});
        CHECK(wrong && a->SetNewTarget(wrong));
        CHECK(!a->AI_Update() && a->runtime_failed && a->runtime_error.find("not a character") != std::string::npos);
        return true;
    }
    bool tracking(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g);
        CHECK(h && a->SetTrackedObject(h) && a->AI_Update() && a->tracked_position_valid);
        CHECK(near(a->m_vTrackObjPos.z, 40));
        g.objects.get(h)->state.m_Pos.z = 60;
        CHECK(a->AI_Update() && near(a->m_vTrackObjPos.z, 60));
        CHECK(g.objects.remove(h) && !a->AI_Update() && !a->GetTrackedObject() && a->runtime_failed);
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool camera(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = g.objects.create(g.objects.find_class("CameraObj"));
        CHECK(h);
        g_hActiveCamera = h;
        a->m_nState = AI_Mgr::StateAttackFar;
        auto* r = g.objects.get(a->m_hObject);
        r->velocity = {4, 5, 6};
        CHECK(a->AI_Update() && a->runtime_camera_pauses == 1 && !a->m_bSetShutdown);
        CHECK(near(r->velocity.x, 0) && near(r->velocity.y, 5) && near(r->velocity.z, 0));
        a->m_nState = AI_Mgr::StateIdle;
        CHECK(a->AI_Update() && a->runtime_camera_pauses == 1 && a->m_bSetShutdown);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        g_hActiveCamera = nullptr;
        a->m_nState = AI_Mgr::StateDying;
        CHECK(a->AI_Update() && a->death_starts == 1 && !a->runtime_failed);
        // This injected model has no resource filename; transfer must retain a failed visible actor.
        CHECK(!a->AI_Update() && a->runtime_error.find("Corpse class/model filenames") != std::string::npos &&
              !a->m_bRemoveMe);
        return true;
    }
    bool spatial(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        CHECK(g.server.ConfigureCharacterBody(h, 250, 10) && g.objects.get(h)->character_body);
        CHECK(!g.server.ConfigureCharacterBody(h, 0, 10) && !g.server.ConfigureCharacterBody(h, 250, NAN));
        CHECK(g.server.ScaleObject(h, {2, 3, 4}) && g.objects.get(h)->state.m_Scale.y == 3);
        CHECK(!g.server.ScaleObject(h, {0, 1, 1}) && !g.server.ScaleObject(h, {1, INFINITY, 1}));
        auto normal = object(g);
        CHECK(normal && !g.server.ScaleObject(normal, {1, 1, 1}));
        g.world.min = {-100, -100, -100};
        g.world.max = {100, 100, 100};
        g.world.models.emplace_back();
        auto* r = g.objects.get(h);
        r->state.m_Pos = {100, 10, 0};
        r->state.m_Flags |= FLAG_REMOVEIFOUTSIDE;
        CHECK(g.step(.01) && g.objects.get(h));
        r->state.m_Pos.x = 100.1f;
        CHECK(g.step(.01) && !g.objects.get(h));
        return true;
    }
    bool body(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        auto* r = g.objects.get(h);
        r->state.m_Pos = {0, -7.999f, 0};
        r->state.m_Flags |= FLAG_GRAVITY;
        CHECK(g.server.ConfigureCharacterBody(h, 250, 10));
        r->velocity = {100, 0, 0};
        CHECK(g.server.advance_physics(.1));
        CHECK(near(r->velocity.x, 100 * std::exp(-1.f)) && near(r->state.m_Pos.x, 10 * (1 - std::exp(-1.f))));
        CHECK(r->body_steps == 12);
        // Standing on a character changes follow policy; world support preserves it.
        auto support = object(g, {0, 10, 0}, "support", "CBaseCharacter");
        CHECK(support);
        r->state.m_Pos = {0, 14.001f, 0};
        r->velocity = {};
        CHECK(a->AI_Update() && (r->state.m_Flags & FLAG_DONTFOLLOWSTANDING));
        CHECK(g.objects.remove(support) && g.objects.finish_frame());
        support = object(g, {0, 10, 0});
        CHECK(support);
        CHECK(a->AI_Update() && !(r->state.m_Flags & FLAG_DONTFOLLOWSTANDING));
        auto samples = a->m_MoveObj.samples;
        g.server.SetDeactivationTime(h, 0);
        g.server.SetObjectState(h, OBJSTATE_ACTIVE);
        CHECK(g.step(.01) && a->m_MoveObj.samples == samples + 1);
        return true;
    }
    bool send(GameSession& g, HOBJECT from, HOBJECT to, const char* text)
    {
        auto s = g.server.CreateString(text);
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(from), to, 1000);
        CHECK(s && m && g.server.WriteToMessageHString(m, s) == DE_OK);
        g.server.FreeString(s);
        CHECK(g.server.EndMessage(m) == DE_OK);
        return true;
    }
    bool triggers(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g, {0, 10, 50}, "speaker", "CBaseCharacter");
        CHECK(h);
        a->m_nState = AI_Mgr::StateIdle;
        CHECK(send(g, h, a->m_hObject, "trigger") && a->m_nState == AI_Mgr::StateIdle);
        CHECK(send(g, h, a->m_hObject, "TRIGGER") && a->m_nState == AI_Mgr::StateSpecial1 && a->GetTarget() == h);
        a->npc_labtech = true;
        CHECK(send(g, h, a->m_hObject, "TRIGGER") && a->m_nState == AI_Mgr::StateSpecial1);
        a->m_nState = AI_Mgr::StateIdle;
        CHECK(send(g, h, a->m_hObject, "TRIGGER") && a->m_nState == AI_Mgr::StateEscapeHide);
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        // Forced original SCRIPT update runs immediately, including shutdown selection.
        CHECK(send(g, h, a->m_hObject, "SCRIPT 0; WAIT 1;") && a->m_nState == AI_Mgr::StateScript);
        CHECK(a->runtime_updates == 1 && !a->runtime_failed && a->m_bSetShutdown);
        return true;
    }
    AI_Mgr* npc(GameSession& g, const char* type, Properties p = {}, Creation reason = Creation::World)
    {
        p.insert_or_assign("Pos", DVector{0, 100, 0});
        auto h = g.objects.create(g.objects.find_class(type), {}, reason, std::move(p));
        return g.server.IsObjectAlive(h) ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
    }
    bool missing(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        for (int i = 0; i < 2; ++i)
        {
            CHECK(!npc(g, "CultistAI") && !npc(g, "CivilianAI"));
            CHECK(g.objects.finish_frame() && g.objects.snapshot().empty());
            CHECK(AI_Mgr::m_dwNumCabal == 0 && CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (auto type : {"CultistAI", "CivilianAI"})
            for (bool male : {true, false})
                for (int difficulty : {0, 1, 2, 3})
                {
                    g.server.set_game_settings(difficulty, 0);
                    auto* a =
                        npc(g, type, {{"Male", male}, {"AIState", std::string("GUARD")}, {"RandomHitPoints", 1.f}});
                    if (!a)
                    {
                        for (const auto& d : g.objects.diagnostics())
                            std::cerr << d << '\n';
                        return false;
                    }
                    auto h = a->m_hObject;
                    auto* r = g.objects.get(h);
                    float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1.f;
                    CHECK(a->RuntimeReady() && r->model && a->npc_male == male && a->m_nState == AI_Mgr::StateGuard);
                    CHECK(near(a->m_damage.m_fHitPoints, 31 * factor) &&
                          near(a->m_InventoryMgr.damage_multiplier,
                               (type == std::string_view("CultistAI") ? .5f : 1) * factor));
                    CHECK(a->m_vGuardPos.y == 100 && r->state.m_Pos.y < 100 && r->auto_deactivate == false);
                    CHECK(r->state.m_Scale.y >= .95f && r->state.m_Scale.y <= 1.1f &&
                          near(r->state.m_Scale.x, 2 - r->state.m_Scale.y));
                    CHECK((r->state.m_Flags & FLAG_REMOVEIFOUTSIDE) && r->character_body &&
                          r->body_properties.mass == 250 && r->body_properties.friction == 10);
                    CHECK(a->m_MoveObj.initialized && r->scheduled && a->AnimSound());
                    std::cout << type << " male=" << male << " difficulty=" << difficulty
                              << " hp=" << a->m_damage.m_fHitPoints << " model=" << r->state.m_Filename << '\n';
                    CHECK(g.objects.remove(h) && g.objects.finish_frame());
                }
        g.server.set_game_settings(1, 0);
        auto* a = npc(g, "CivilianAI", {{"Male", false}, {"LabTech", true}, {"Scared", true}});
        CHECK(a && a->npc_labtech && !a->npc_male && dynamic_cast<CivilianAI*>(a)->m_bScared);
        CHECK(a->AnimSound()->GetSoundRoot() == "sounds/enemies/civilian");
        CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        a = npc(g, "CultistAI", {{"Male", false}}, Creation::String);
        CHECK(a && a->npc_male); // source world-only subclass properties
        auto* shared_table = a->AnimSound();
        DVector separated{300, 100, 0};
        CHECK(g.server.TeleportObject(a->m_hObject, &separated) == DE_OK);
        {
            GameSession rejected;
            CHECK(!rejected.initialize());
        }
        auto* other = npc(g, "CultistAI");
        CHECK(other && other->AnimSound() == shared_table);
        CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0);
        return true;
    }
    bool failures(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (auto props : {Properties{{"AIWeapon1", std::string("SHOTGUN")}}, Properties{{"RobeSkin", true}},
                           Properties{{"Proximity_Bombs", 1.f}}, Properties{{"RandomHitPoints", -1.f}}})
        {
            CHECK(!npc(g, "CultistAI", props));
            CHECK(g.objects.finish_frame() && g.objects.snapshot().empty());
        }
        CHECK(!npc(g, "CivilianAI", {}, Creation::Restore) && g.objects.finish_frame() && g.objects.snapshot().empty());
        CHECK(AI_Mgr::m_dwNumCabal == 0 && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool arena(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string e;
        g.world_path = "synthetic NPC integration arena";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 200}}}));
        auto* a = dynamic_cast<CultistAI*>(npc(g, "CultistAI"));
        CHECK(a);
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        if (!g.enter_player(e))
        {
            std::cerr << e << '\n';
            return false;
        }
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player);
        for (int tick = 0; tick < 600; ++tick)
            CHECK(g.step(1.0 / 60));
        std::cout << "Arena state=" << a->m_nState << " phase=" << a->Metacmd << " updates=" << a->runtime_updates
                  << " shots=" << a->fire_shots << " error=" << a->runtime_error
                  << " player HP=" << player->m_damage.m_fHitPoints << '\n';
        CHECK(a->RuntimeReady() && a->runtime_updates > 15 && a->attack_updates > 0 && a->fire_shots > 0);
        CHECK(player->m_damage.m_fHitPoints < 100);
        CHECK(a->m_InventoryMgr.CurrentWeapon()->m_hHandModel);
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string e;
        ObjectId old{};
        for (int load = 0; load < 2; ++load)
        {
            CHECK(g.request_world("01_SubwayA", e) && !g.objects.get(old));
            unsigned cultists = 0, civilians = 0, failed = 0, unsupported = 0, created = 0;
            for (auto& entry : g.entries)
            {
                failed += entry.state == EntryState::Failed;
                unsupported += entry.state == EntryState::Unsupported;
                created += entry.state == EntryState::Created;
            }
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                auto* a = dynamic_cast<AI_Mgr*>(r->game.get());
                if (!a)
                    continue;
                CHECK(a->RuntimeReady());
                old = id;
                cultists += a->npc_type == AI_Mgr::NPCType::Cultist;
                civilians += a->npc_type == AI_Mgr::NPCType::Civilian;
            }
            CHECK(cultists == 10 && civilians == 5 && failed == 0 && unsupported == 1 && created == 203);
            CHECK(g.objects.snapshot().size() == 225 && g.enter_player(e) && g.objects.snapshot().size() == 228);
            for (int tick = 0; tick < 600; ++tick)
            {
                CHECK(g.step(1.0 / 60));
                if (!tick)
                    CHECK(g.objects.snapshot().size() == 230 && g.objects.attachments().size() == 11);
            }
            std::uint64_t updates = 0, pings = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                auto* a = dynamic_cast<AI_Mgr*>(r->game.get());
                if (!a)
                    continue;
                if (a->runtime_failed)
                    std::cerr << a->runtime_error << '\n';
                CHECK(a->RuntimeReady() && a->runtime_updates > 0);
                updates += a->runtime_updates;
                pings += r->activation_pings;
            }
            CHECK(updates > 15 && pings > 0);
            std::cout << "SubwayA load=" << load << " created=" << created << " unsupported=" << unsupported
                      << " AI_Update=" << updates << " pings=" << pings << '\n';
            CHECK(g.unload_world() && !g.objects.get(old) && g.objects.snapshot().empty());
            CHECK(AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0 && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"body", body},         {"missing", missing},   {"activation", activation}, {"visibility", visibility},
        {"lifetime", lifetime}, {"targets", targets},   {"tracking", tracking},     {"camera", camera},
        {"spatial", spatial},   {"triggers", triggers}, {"arena", arena},           {"catalog", catalog},
        {"failures", failures}, {"subway", subway}};
    if (argc < 2)
        return 2;
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
