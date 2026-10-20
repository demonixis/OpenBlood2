#include "brush_fixture.h"
#include "game/original/EnhancedGideon.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
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
    bool setup(GameSession& g, const char* root)
    {
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        auto floor = test_brush::box("floor", {-10000, -20, -10000}, {10000, -10, 10000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    Gideon* create(GameSession& g, const char* name, DVector pos)
    {
        auto h = g.objects.create(g.objects.find_class(name), {}, Creation::World, {{"Pos", pos}});
        auto* a = dynamic_cast<Gideon*>(g.server.HandleToObject(h));
        if (a && !a->RuntimeReady())
            std::cerr << a->runtime_error << '\n';
        if (a && !a->m_MoveObj.Update())
            return nullptr;
        return a;
    }
    bool profiles(const char* root)
    {
        for (int difficulty : {0, 1, 2})
        {
            GameSession g;
            CHECK(setup(g, root));
            g.server.set_game_settings(difficulty, 0);
            auto* a = create(g, "EnhancedGideon", {-500, 200, 0});
            auto* b = create(g, "EnhancedGideon", {500, 200, 0});
            auto* old = create(g, "Gideon", {0, 200, 1000});
            CHECK(a && b && old && a->RuntimeReady() && b->RuntimeReady() && old->RuntimeReady());
            const float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1.f;
            CHECK(a->m_damage.m_fHitPoints >= 9000 * factor && a->m_damage.m_fHitPoints <= 9001 * factor);
            CHECK(a->m_damage.m_fArmorPoints == 0 && !a->m_damage.apply_damage_physics && !a->m_bCabal);
            CHECK(a->m_fWalkSpeed == 175 && a->m_fRunSpeed == 380 && a->m_fRollSpeed == 200 && a->m_fAIMass == 250);
            CHECK(a->m_fHearingDist == 0 && a->m_fSeeingDist == 10000 && a->m_fSensingDist == 10000 &&
                  a->m_dwFlags == 0x20);
            CHECK(a->AnimSound() == b->AnimSound() && a->AnimSound() != old->AnimSound());
            CHECK(a->AnimSound()->GetSoundRoot() == "sounds/enemies/EnhancedGideon");
            CHECK(std::string_view(g.objects.get(a->m_hObject)->state.m_SkinName) == "Skins/Enemies/GideonE.dtx");
            CHECK(!(g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY));
            CHECK(a->m_fFullHealth == 0 && old->m_fFullHealth == 1);
            CHECK(g.objects.advance(.002) && a->RuntimeReady());
            CHECK(!a->m_bCreateHealth && a->m_fFullHealth == a->m_damage.m_fHitPoints && a->boss_health_fraction == 1);
            const auto count = a->health_requests;
            CHECK(g.objects.engine_message(old->m_hObject, MID_UPDATE, nullptr));
            CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr) && a->health_requests == count &&
                  old->health_requests > count);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.string_count());
        }
        return true;
    }
    bool scripts(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "EnhancedGideon", {0, 200, 0});
        CHECK(a && a->RuntimeReady());
        CHECK(a->PrepareScript("SCRIPT; WAIT 1; SETMOVEMENT RUN"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && a->m_bNoFire &&
              (g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY));
        const auto shots = a->fire_shots;
        CHECK(!a->Fire() && a->fire_result == FireResult::Blocked && a->fire_error.empty() && a->suppressed_shots == 1);
        CHECK(a->MC_Extra("powerup") && a->powerup_effect == 0 && a->suppressed_powerups == 1);
        CHECK(a->MC_Extra("rr_gun") && a->fire_shots == shots && a->suppressed_shots == 2);
        CHECK(a->MC_Fire_Stand() && a->m_nState == AI_Mgr::StatePassive);
        // SetState leaves suppression latched; only DONE/default clears it in the source.
        CHECK(a->PrepareScript("SCRIPT; SETSTATE PASSIVE") && a->AI_STATE_Script(g.server_shell.paths) && a->m_bNoFire);
        CHECK(a->PrepareScript("SCRIPT") && a->AI_STATE_Script(g.server_shell.paths) && !a->m_bNoFire);
        CHECK(a->script_outcome == AI_Mgr::ScriptOutcome::Completed);
        CHECK(a->PrepareScript("SCRIPT; PLAYANIMATIONLOOPING talking_02"));
        CHECK(a->AI_STATE_Script(g.server_shell.paths) && !a->m_bNoFire &&
              a->script_outcome == AI_Mgr::ScriptOutcome::Completed);
        CHECK(g.unload_world());
        return true;
    }
    bool locomotion(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "EnhancedGideon", {0, 200, 0});
        CHECK(a && a->RuntimeReady());
        a->m_nState = AI_Mgr::StateScript;
        a->m_bAnimating = false;
        a->Metacmd = 7;
        CHECK(a->MC_Run() && !(g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY));
        CHECK(std::abs(g.objects.get(a->m_hObject)->velocity.z - 175) < .001);
        CHECK(a->MC_Run() && std::abs(g.objects.get(a->m_hObject)->velocity.z - 380) < .001);
        a->m_bAnimating = false;
        a->m_nInjuredLeg = 10;
        CHECK(a->MC_Walk() && (g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY));
        CHECK(std::abs(g.objects.get(a->m_hObject)->velocity.z - 175.f / 3) < .001);
        CHECK(a->MC_Walk() && std::abs(g.objects.get(a->m_hObject)->velocity.z - 175) < .001);
        a->m_bAnimating = false;
        CHECK(a->Script_Run() && (g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY));
        CHECK(std::abs(g.objects.get(a->m_hObject)->velocity.z - 175) < .001);
        CHECK(a->Script_Run() && std::abs(g.objects.get(a->m_hObject)->velocity.z - 380) < .001);
        auto* r = g.objects.get(a->m_hObject);
        r->animation.looping = false;
        r->animation.done = true;
        CHECK(a->Script_Run() && !a->m_bAnimating && a->Metacmd == 7 && r->velocity.z == 0);
        // The real FOLLOWPATH dispatcher must use the subclass override too.
        ObjectCreateStruct point;
        point.m_Pos = {0, 200, 2000};
        SAFE_STRCPY(point.m_Name, "enhanced1");
        CHECK(g.objects.create(g.objects.find_class("PathPoint"), point, Creation::World,
                               {{"Name", std::string("enhanced1")}, {"Pos", point.m_Pos}}));
        CHECK(g.server_shell.paths.BuildPathList());
        a->m_eScriptMovement = AI_Mgr::SM_RUN;
        a->m_bAnimating = false;
        CHECK(a->PrepareScript("SCRIPT; FOLLOWPATH enhanced") && a->AI_STATE_Script(g.server_shell.paths));
        CHECK(a->script_motion_outcome == AI_Mgr::MotionOutcome::Moving && std::abs(r->velocity.z - 175) < .001);
        CHECK(g.unload_world());
        return true;
    }
    bool combat(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "EnhancedGideon", {0, 200, 0});
        auto* target = create(g, "Gideon", {500, 200, 500});
        CHECK(a && target);
        g.server.SetObjectState(target->m_hObject, OBJSTATE_INACTIVE);
        target->m_damage.can_damage = false;
        CHECK(g.objects.advance(.002));
        CHECK(a->SetNewTarget(target->m_hObject) && a->RefreshTargetPosition());
        a->m_vTargetPos = target->m_MoveObj.GetPos();
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        a->Metacmd = 2;
        CHECK(a->AI_STATE_AttackClose() && a->m_bAnimating && a->m_nCurMetacmd == 7);
        CHECK(a->MC_Extra("powerup") && a->powerup_effect);
        CHECK(a->MC_Extra("rr_gun") && a->fire_shots == 1);
        a->m_bNoFire = true;
        CHECK(a->AI_STATE_AttackFar() && a->m_nState == AI_Mgr::StatePassive);
        a->m_bNoFire = false;
        const float hp = a->m_damage.m_fHitPoints;
        const auto requests = a->health_requests;
        auto m = g.server.StartMessageToObject(target, a->m_hObject, damage_message);
        CHECK(m &&
              g.server.WriteDamage(m, {{0, 0, 1}, {0, 200, 0}, 100, DamageType::Normal, target->m_hObject}) == DE_OK);
        CHECK(g.server.EndMessage(m) == DE_OK && a->m_damage.m_fHitPoints < hp && a->health_requests == requests + 1 &&
              a->damage_specials == 0);
        CHECK(a->RuntimeReady() && g.unload_world());
        return true;
    }
    bool keys(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "EnhancedGideon", {0, 200, 0});
        auto* target = create(g, "Gideon", {500, 200, 500});
        CHECK(a && target);
        g.server.SetObjectState(target->m_hObject, OBJSTATE_INACTIVE);
        target->m_damage.can_damage = false;
        a->m_bSetShutdown = true;
        g.server.SetObjectState(a->m_hObject, OBJSTATE_ACTIVE);
        CHECK(a->SetNewTarget(target->m_hObject) && a->SetNewState(AI_Mgr::StateAttackClose));
        a->m_vTargetPos = target->m_MoveObj.GetPos();
        a->Metacmd = 2;
        for (int i = 0; i < 240 && !a->fire_shots; ++i)
            CHECK(g.step(1. / 60));
        if (a->runtime_failed)
            std::cerr << a->runtime_error << '\n';
        CHECK(a->RuntimeReady() && a->fire_shots > 0 && a->model_keys > 0);
        std::cout << "EnhancedGideon real animation-key shots=" << a->fire_shots << '\n';
        CHECK(g.unload_world());
        return true;
    }
    bool worlds(const char* root)
    {
        GameSession g;
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        unsigned count = 0;
        for (const char* world : {"23_rooftop", "24_ancientcity"})
        {
            CHECK(g.request_world(world, error));
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                auto* a = dynamic_cast<EnhancedGideon*>(r->game.get());
                if (a)
                {
                    CHECK(a->RuntimeReady() && r->model);
                    ++count;
                }
            }
            CHECK(g.unload_world());
        }
        CHECK(count == 2);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "profiles"     ? profiles(argv[2])
            : mode == "scripts"    ? scripts(argv[2])
            : mode == "locomotion" ? locomotion(argv[2])
            : mode == "combat"     ? combat(argv[2])
            : mode == "keys"       ? keys(argv[2])
                                   : worlds(argv[2]))
               ? 0
               : 1;
}
