#include "brush_fixture.h"
#include "game/original/Fanatic.h"
#include "game/original/Projectile.h"
#include "game/original/Prophet.h"
#include "game/original/Spawner.h"
#include "game/server_adapter.h"
#include "model_fixture.h"
#include <cmath>
#include <iostream>
#include <tuple>
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
    // Labelled synthetic actor: the real class code with injected model/table data;
    // the private cases cover actual initialization from the retail resources.
    template <class T> T* actor(GameSession& g, const char* name, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{name,
                                    g.objects.find_class("AI_Mgr"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<T>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<T*>(r->game.get()) : nullptr;
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
        if (!a->InitStatics(std::make_shared<CAnim_Sound>()) || !a->m_MoveObj.Init(h) ||
            !a->m_InventoryMgr.Init(h, nullptr) || !a->m_damage.Init(h) || !a->m_MoveObj.Update())
            return nullptr;
        a->m_damage.SetHitPoints(100);
        a->m_vDims = r->dimensions;
        return a;
    }
    bool weapons()
    {
        DBYTE id = 99;
        // Names matching no compiled (non-_ADD_ON) branch take the source default Beretta.
        for (auto name : {"COMBAT", "FLAYER", "ROCK", "NIGHTMARE_FIREBALL", "BERE", ""})
            CHECK(AI_Mgr::WeaponStrToInt(name, id) && id == WEAP_BERETTA);
        const std::pair<const char*, DBYTE> supported[]{{"BERETTA", WEAP_BERETTA},
                                                        {"SUBMA", WEAP_SUBMACHINEGUN},
                                                        {"SUBMACHINEGUN", WEAP_SUBMACHINEGUN},
                                                        {"ASSAULTRIFLE", 9},
                                                        {"FLAREGUN", WEAP_FLAREGUN},
                                                        {"MELEE", WEAP_MELEE},
                                                        {"NONE", 0},
                                                        {"ENERGY_BLAST", 27},
                                                        {"NAGA_BLAST", 38},
                                                        {"GIDEON_SHIELD", 41},
                                                        {"GIDEON_WIND", 42},
                                                        {"MELEEX", WEAP_MELEE},
                                                        {"TESLACANNON", WEAP_TESLACANNON}};
        for (auto [name, expected] : supported)
            CHECK(AI_Mgr::WeaponStrToInt(name, id) && id == expected);
        // A matched branch whose weapon is not integrated is never replaced by Beretta.
        for (auto name : {"SHOTGUN", "SHOTGXX", "SNIPER", "HOWITZER", "PROX_BOMB", "HEAL", "SHIELD", "SKULL",
                          "GIDEON_GOO", "ORB", "SINGULARITY"})
            CHECK(!AI_Mgr::WeaponStrToInt(name, id));
        return true;
    }
    bool decisions()
    {
        GameSession g;
        CHECK(setup(g));
        auto* f = actor<Fanatic>(g, "FanaticProbe");
        auto* p = actor<Prophet>(g, "ProphetProbe", {200, 10, 0});
        CHECK(f && p);
        auto stimuli = [](AI_Mgr& a, float sight, float health)
        {
            a.m_fStimuli[0] = sight;
            a.m_fStimuli[5] = health;
            a.m_vTargetPos = {0, 10, 0};
        };
        // Fanatic with a stimulus: health < .25 FindHealth, < .5 or SIGHT <= .5 far, else close.
        stimuli(*f, 1, .2f);
        CHECK(f->ComputeState(1) && f->m_nState == AI_Mgr::StateFindHealth);
        stimuli(*f, 1, .4f);
        CHECK(f->ComputeState(1) && f->m_nState == AI_Mgr::StateAttackFar);
        stimuli(*f, .5f, 1);
        CHECK(f->ComputeState(1) && f->m_nState == AI_Mgr::StateAttackFar);
        stimuli(*f, .6f, 1);
        CHECK(f->ComputeState(1) && f->m_nState == AI_Mgr::StateAttackClose);
        // A lit Fanatic resumes its sprint at command 3.
        f->m_fStartTime = 1;
        CHECK(f->ComputeState(2) && f->m_nState == AI_Mgr::StateSpecial1 && f->Metacmd == 3);
        f->m_fStartTime = 0;
        // Without a stimulus: a healthy Fanatic searches, a hurt one idles.
        CHECK(f->SetNewState(AI_Mgr::StateAttackClose));
        stimuli(*f, 0, .5f);
        CHECK(f->ComputeState(0) && f->m_nState == AI_Mgr::StateSearchVisual);
        CHECK(f->SetNewState(AI_Mgr::StateAttackClose));
        stimuli(*f, 0, .4f);
        CHECK(f->ComputeState(0) && f->m_nState == AI_Mgr::StateIdle);
        // Prophet: < .15 FindHealth, < .5 far, an escaping Prophet looks for ammo.
        stimuli(*p, 1, .1f);
        CHECK(p->ComputeState(1) && p->m_nState == AI_Mgr::StateFindHealth);
        stimuli(*p, 1, .2f);
        CHECK(p->ComputeState(1) && p->m_nState == AI_Mgr::StateAttackFar);
        CHECK(p->SetNewState(AI_Mgr::StateEscapeRun));
        stimuli(*p, 1, 1);
        CHECK(p->ComputeState(1) && p->m_nState == AI_Mgr::StateFindAmmo);
        stimuli(*p, .6f, 1);
        CHECK(p->ComputeState(1) && p->m_nState == AI_Mgr::StateAttackClose);
        // Source quirk: only a badly hurt Prophet searches without a stimulus.
        CHECK(p->SetNewState(AI_Mgr::StateAttackClose));
        stimuli(*p, 0, .2f);
        CHECK(p->ComputeState(0) && p->m_nState == AI_Mgr::StateSearchVisual);
        CHECK(p->SetNewState(AI_Mgr::StateAttackClose));
        stimuli(*p, 0, 1);
        CHECK(p->ComputeState(0) && p->m_nState == AI_Mgr::StateIdle);
        // FindHealth never finds a powerup (no candidate list) and escapes immediately.
        CHECK(f->SetNewState(AI_Mgr::StateFindHealth) && f->DispatchState());
        CHECK(f->m_nState == AI_Mgr::StateEscapeRun && f->health_searches == 1);
        // The original base Escape_Hide/Special handlers are empty for the Prophet.
        CHECK(p->SetNewState(AI_Mgr::StateEscapeHide) && p->DispatchState() && p->m_nState == AI_Mgr::StateEscapeHide);
        CHECK(p->SetNewState(AI_Mgr::StateSpecial3) && p->DispatchState() && p->state_error.empty());
        CHECK(f->SetNewState(AI_Mgr::StateSpecial2) && f->DispatchState() && f->state_error.empty());
        // Special1 needs the dynamite node; a model without it fails explicitly.
        CHECK(f->SetNewState(AI_Mgr::StateSpecial1));
        f->Metacmd = 2;
        CHECK(!f->DispatchState() && f->state_error.find("torso_extra1") != std::string::npos);
        CHECK(f->m_fStartTime == 0 && !f->m_hSmokeSource);
        return true;
    }
    // Labelled synthetic radius-damage target: a plain object with the original aggregate.
    struct Target : BaseClass
    {
        CDestructable damage;
        Target()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override
        {
            const auto result = BaseClass::EngineMessageFn(message, data, reason);
            if (message == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject);
                damage.SetHitPoints(100);
                damage.apply_damage_physics = false;
            }
            return result;
        }
    };
    bool howitzer()
    {
        GameSession g;
        CHECK(setup(g));
        auto box = [&](DVector pos, DVector dims, const char* name)
        {
            ObjectCreateStruct s;
            s.m_Pos = pos;
            s.m_Flags = FLAG_SOLID;
            SAFE_STRCPY(s.m_Name, name);
            auto h = g.objects.create(g.objects.find_class("BaseClass"), s);
            if (auto* r = g.objects.get(h))
                r->dimensions = dims;
            return h;
        };
        static const ClassInfo target{"HowitzerTarget",
                                      g.objects.find_class("BaseClass"),
                                      []() -> std::unique_ptr<BaseClass> { return std::make_unique<Target>(); },
                                      {}};
        CHECK(g.objects.register_class(target));
        auto victim_at = [&](DVector pos)
        {
            ObjectCreateStruct s;
            s.m_Pos = pos;
            auto h = g.objects.create(&target, s);
            if (auto* r = g.objects.get(h))
                r->dimensions = {10, 10, 10};
            return h;
        };
        auto shooter = box({0, 50, 0}, {10, 40, 10}, "shooter");
        auto victim = victim_at({120, 0, 0}), far_away = victim_at({600, 0, 0});
        CHECK(shooter && victim && far_away);
        ObjectCreateStruct state;
        state.m_Pos = {0, 50, 0};
        auto* shell = dynamic_cast<CHowitzerShell*>(g.server.CreateObject(g.server.GetClass("CHowitzerShell"), &state));
        CHECK(shell && shell->Setup({0, -1, 0}, 6, 50, 250, 200, shooter));
        auto handle = shell->m_hObject;
        CHECK(g.step(.01));
        // Hitscan: it skipped its shooter, hit the floor top and was removed after one update.
        CHECK(!g.objects.get(handle) || g.objects.get(handle)->life != Life::Alive);
        CHECK(!g.server.scene_explosions.empty());
        const auto& effect = g.server.scene_explosions.back();
        CHECK(effect.type == 9 && effect.damage == 50 && effect.radius == 200);
        CHECK(std::abs(effect.position.y + 10) < .01f && effect.normal.y > .99f);
        CHECK(effect.sound == "Sounds\\Weapons\\c4\\explosion_1.wav" && !effect.sound_error.empty());
        CHECK(!g.server.projectile_reports.empty());
        const auto& report = g.server.projectile_reports.back();
        // Candidates are destructibles: the shell itself, the victim and the far target.
        CHECK(report.outcome == "exploded" && report.candidates == 3 && report.in_radius == 2 && report.delivered == 2);
        CHECK(dynamic_cast<Target*>(g.server.HandleToObject(victim))->damage.m_fHitPoints < 100);
        CHECK(dynamic_cast<Target*>(g.server.HandleToObject(far_away))->damage.m_fHitPoints == 100);
        // A shell with nothing below it expires without an explosion.
        const auto explosions = g.server.scene_explosions.size();
        state.m_Pos = {0, 50, 0};
        shell = dynamic_cast<CHowitzerShell*>(g.server.CreateObject(g.server.GetClass("CHowitzerShell"), &state));
        CHECK(shell && shell->Setup({0, 1, 0}, 6, 50, 250, 200, shooter));
        CHECK(g.step(.01) && g.server.scene_explosions.size() == explosions);
        CHECK(g.server.projectile_reports.back().outcome == "expired");
        CHECK(g.unload_world() && !g.server.string_count());
        return true;
    }
    AI_Mgr* npc(GameSession& g, const char* type, Properties p = {}, DVector pos = {0, 100, 0})
    {
        p.insert_or_assign("Pos", pos);
        auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World, std::move(p));
        if (!g.server.IsObjectAlive(h))
            for (const auto& d : g.objects.diagnostics())
                std::cerr << d << '\n';
        return g.server.IsObjectAlive(h) ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (int difficulty : {0, 1, 2})
        {
            g.server.set_game_settings(difficulty, 0);
            const float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1.f;
            auto* f = npc(g, "Fanatic");
            auto* f2 = npc(g, "Fanatic", {}, {300, 100, 0});
            auto* p = npc(g, "Prophet", {}, {-300, 100, 0});
            CHECK(f && f2 && p && f->RuntimeReady() && p->RuntimeReady());
            // Shared class tables and fixed original models.
            CHECK(f->AnimSound() == f2->AnimSound() && f->AnimSound() != p->AnimSound());
            CHECK(f->AnimSound()->GetSoundRoot() == "sounds/enemies/fanatic");
            CHECK(p->AnimSound()->GetSoundRoot() == "sounds/enemies/Prophet");
            CHECK(std::string(g.objects.get(f->m_hObject)->state.m_Filename) == "Models/Enemies/Fanatic.abc");
            CHECK(std::string(g.objects.get(p->m_hObject)->state.m_Filename) == "Models/Enemies/Prophet.abc");
            // HP 50/200 (+ the reversed default random draw), armor 0/100, halved damage.
            CHECK(f->m_damage.m_fHitPoints >= 50 * factor && f->m_damage.m_fHitPoints <= 51 * factor + .01f);
            CHECK(p->m_damage.m_fHitPoints >= 200 * factor && p->m_damage.m_fArmorPoints == 100 * factor);
            CHECK(std::abs(f->m_InventoryMgr.damage_multiplier - .5f * factor) < 1e-5f &&
                  std::abs(p->m_InventoryMgr.damage_multiplier - .5f * factor) < 1e-5f);
            CHECK(f->m_fSeeingDist == 1500 && f->m_fRollSpeed == 105 && f->m_fJumpSpeed == 400 && f->m_dwFlags == 15);
            // Defaults BERETTA/COMBAT/ASSAULTRIFLE: COMBAT resolves to the Beretta already held.
            CHECK(f->m_InventoryMgr.GetWeapon(0)->data.m_nType == WEAP_BERETTA &&
                  f->m_InventoryMgr.GetWeapon(1)->data.m_nType == 9 && !f->m_InventoryMgr.GetWeapon(2));
            // The Prophet obtains the melee weapon after base initialization.
            bool melee = false;
            for (unsigned i = 0; i < 10; ++i)
                melee |= p->m_InventoryMgr.GetWeapon(i) && p->m_InventoryMgr.GetWeapon(i)->data.m_nType == WEAP_MELEE;
            CHECK(melee && p->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_BERETTA);
            // Proximity_Bombs 25: kept unused in slot 10; AddAmmo caps proximity bombs at 10 for every strength.
            CHECK(f->m_InventoryMgr.GetWeapon(10) && f->m_InventoryMgr.GetAmmoCount(AMMO_PROXIMITYBOMB) == 10);
            CHECK(!p->m_InventoryMgr.GetWeapon(10));
            std::cout << "retail difficulty=" << difficulty << " Fanatic hp=" << f->m_damage.m_fHitPoints
                      << " Prophet hp=" << p->m_damage.m_fHitPoints << " armor=" << p->m_damage.m_fArmorPoints << '\n';
            for (auto* a : {f, f2, p})
                CHECK(g.objects.remove(a->m_hObject));
            CHECK(g.objects.finish_frame());
        }
        CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0);
        return true;
    }
    bool kamikaze(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string e;
        g.world_path = "synthetic Fanatic kamikaze arena (retail resources)";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 600}}}));
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld() && g.enter_player(e));
        auto* player = g.server_shell.Player(g.local_client);
        auto* f = dynamic_cast<Fanatic*>(npc(g, "Fanatic", {{"AIState", std::string("PASSIVE")}}));
        CHECK(player && f && f->RuntimeReady());
        const auto handle = f->m_hObject;
        const float health = player->m_damage.m_fHitPoints;
        CHECK(f->SetNewTarget(player->m_hObject) && f->SetNewState(AI_Mgr::StateSpecial1));
        bool lit = false, smoking = false;
        for (int tick = 0; tick < 1200 && g.server.IsObjectAlive(handle); ++tick)
        {
            if (!g.step(1.0 / 60))
            {
                for (const auto& x : g.step_errors())
                    std::cerr << x << '\n';
                return false;
            }
            if (!g.server.IsObjectAlive(handle))
                break;
            lit |= f->m_fStartTime > 0;
            smoking |= f->m_hAttach != 0 && g.server.IsObjectAlive(f->m_hSmokeSource);
            CHECK(f->runtime_error.empty());
        }
        const auto& shell = g.server.projectile_reports.back();
        CHECK(shell.outcome == "exploded" && !shell.cancelled && shell.delivered >= 1);
        std::cout << "Kamikaze lit=" << lit << " smoke=" << smoking << " player hp " << health << " -> "
                  << player->m_damage.m_fHitPoints << '\n';
        CHECK(lit && smoking && !g.server.IsObjectAlive(handle));
        CHECK(!g.server.scene_explosions.empty() && g.server.scene_explosions.back().type == 9);
        CHECK(player->m_damage.m_fHitPoints < health); // Radius damage reached the target.
        return true;
    }
    bool actions(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* f = dynamic_cast<Fanatic*>(npc(g, "Fanatic", {{"AIState", std::string("PASSIVE")}}));
        auto* p = dynamic_cast<Prophet*>(npc(g, "Prophet", {{"AIState", std::string("PASSIVE")}}, {400, 100, 0}));
        CHECK(f && p && f->m_MoveObj.Update() && p->m_MoveObj.Update());
        // Unlike CultistAI, a clear ROLL with no side rolls forward (13) or backward (14).
        for (auto [flags, command, clip] : {std::tuple{0x11, 13, f->AnimSound()->m_nAnim_ROLL_FORWARD},
                                            std::tuple{0x12, 14, f->AnimSound()->m_nAnim_ROLL_BACK},
                                            std::tuple{0x14, 16, f->AnimSound()->m_nAnim_ROLL_RIGHT}})
        {
            CHECK(f->SetNewState(AI_Mgr::StateDodge));
            f->m_nDodgeFlags = flags;
            f->Metacmd = 2;
            CHECK(f->DispatchState() && f->locomotion_command == command && f->m_nCurAnimation == clip);
        }
        // A blocked dodge (no ROLL bit) advances to ComputeState.
        CHECK(f->SetNewState(AI_Mgr::StateDodge));
        f->m_nDodgeFlags = 0x21;
        f->Metacmd = 2;
        CHECK(f->DispatchState() && f->Metacmd == 3);
        // Crouching fire selects the weapon's crouch clip and the original command id.
        CHECK(f->MC_Fire_Crouch() && f->m_nCurMetacmd == 11 && f->m_bAnimating);
        CHECK(f->m_nCurAnimation == f->AnimSound()->m_nAnim_FIRE_CROUCH[TYPE_PISTOL]); // Beretta.
        // The Prophet's melee swing picks one of FIRE_STAND[4..6] and does not shoot.
        CHECK(p->m_InventoryMgr.ChangeWeapon(WEAP_MELEE) == InventoryResult::Ok);
        const auto shots = p->fire_calls;
        CHECK(p->MC_Fire_Stand() && p->melee_roll >= 4 && p->melee_roll <= 6 && p->m_nCurMetacmd == 7);
        CHECK(p->m_nCurAnimation == p->AnimSound()->m_nAnim_FIRE_STAND[p->melee_roll] && p->fire_calls == shots);
        CHECK(g.unload_world());
        return true;
    }
    bool world(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        CHECK(g.request_world("05_cdmb", e));
        unsigned fanatics = 0, prophets = 0;
        HOBJECT spawner = nullptr;
        for (std::size_t i = 0; i < g.entries.size(); ++i)
        {
            const auto& name = g.world.objects[i].class_name;
            if (name != "Fanatic" && name != "Prophet" && name != "Spawner")
                continue;
            CHECK(g.entries[i].state == EntryState::Created);
            const auto* r = g.objects.get(g.entries[i].object);
            if (name == "Spawner")
            {
                spawner = r->handle;
                continue;
            }
            const auto* a = dynamic_cast<const AI_Mgr*>(r->game.get());
            CHECK(a && a->RuntimeReady());
            fanatics += name == "Fanatic";
            prophets += name == "Prophet";
        }
        CHECK(fanatics == 4 && prophets == 1 && spawner);
        // A Spawner trigger now creates a Fanatic with the class defaults.
        auto text = g.server.CreateString("TRIGGER");
        auto message = g.server.StartMessageToObject(nullptr, spawner, 1000);
        CHECK(text && message && g.server.WriteToMessageHString(message, text) == DE_OK);
        g.server.FreeString(text);
        CHECK(g.server.EndMessage(message) == DE_OK);
        const auto& report = g.server.spawn_reports.back();
        CHECK(report.request == "Fanatic" && report.child && report.outcome == "Created");
        CHECK(dynamic_cast<Fanatic*>(g.server.HandleToObject(report.child))->RuntimeReady());
        CHECK(g.run_world(e));
        std::cout << "05_CDMB: " << fanatics << " Fanatics, " << prophets
                  << " Prophet, spawned Fanatic, run_world passed\n";
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string mode = argv[1];
    if (argc == 2)
        return (mode == "weapons"     ? weapons()
                : mode == "decisions" ? decisions()
                                      : mode == "howitzer" && howitzer())
                   ? 0
                   : 1;
    return (mode == "catalog"    ? catalog(argv[2])
            : mode == "kamikaze" ? kamikaze(argv[2])
            : mode == "actions"  ? actions(argv[2])
                                 : mode == "world" && world(argv[2]))
               ? 0
               : 1;
}
