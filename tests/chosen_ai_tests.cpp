#include "brush_fixture.h"
#include "game/original/ChosenAI.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <cmath>
#include <iostream>
#include <set>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ":" << __LINE__ << ": " #x "\n";                                                  \
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
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    ChosenAI* create(GameSession& g, const char* name, DVector pos, Properties props = {})
    {
        props["Pos"] = pos;
        auto h = g.objects.create(g.objects.find_class(name), {}, Creation::World, std::move(props));
        if (!h)
            for (const auto& d : g.objects.diagnostics())
                std::cerr << d << '\n';
        auto* a = dynamic_cast<ChosenAI*>(g.server.HandleToObject(h));
        if (a && !a->RuntimeReady())
            std::cerr << name << ": " << a->runtime_error << '\n';
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
            const float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1.f;
            std::set<const CAnim_Sound*> tables;
            int i = 0;
            for (const auto* type : {"GabriellaAI", "GabriellaREV", "OpheliaAI", "OpheliaREV"})
            {
                auto* a = create(g, type, {float(i++ * 400 - 800), 100, 0});
                CHECK(a && a->RuntimeReady());
                CHECK(a->m_nState == AI_Mgr::StatePassive && a->m_bCabal && a->m_dwFlags == 0x0b);
                CHECK(a->m_fSeeingDist == 1500 && a->m_fHearingDist == 1500 && a->m_fSensingDist == 200);
                CHECK(a->m_fWalkSpeed == 75 && a->m_fRunSpeed == 180 && a->m_fRollSpeed == 105 &&
                      a->m_fJumpSpeed == 400);
                const float hp = a->Ophelia() ? 400 : 600;
                CHECK(a->m_damage.m_fHitPoints >= hp * factor && a->m_damage.m_fHitPoints <= (hp + 1) * factor);
                CHECK(a->m_damage.m_fArmorPoints == hp * factor && a->m_InventoryMgr.damage_multiplier == factor);
                CHECK(a->npc_ammo[1] == (a->Ophelia() ? 100 : 0) && a->npc_ammo[6] == (a->Ophelia() ? 500 : 400));
                CHECK(a->m_InventoryMgr.CurrentWeapon()->data.m_nType ==
                      (a->Ophelia() ? WEAP_TESLACANNON : WEAP_MINIGUN));
                CHECK(a->AnimSound()->GetSoundRoot() ==
                      std::string(a->Ophelia() ? "sounds/chosen/ophelia" : "sounds/chosen/gabriella"));
                CHECK(tables.insert(a->AnimSound()).second);
                const auto* r = g.objects.get(a->m_hObject);
                const std::string skin =
                    a->Revenant() ? (a->Ophelia() ? "ophrev" : "gabrev") : (a->Ophelia() ? "ophelia" : "gabriella");
                CHECK(std::string(r->state.m_SkinName) == "Skins/Characters/" + skin + ".dtx");
            }
            auto* first = create(g, "GabriellaAI", {1000, 100, 0});
            CHECK(first);
            CHECK(tables.contains(first->AnimSound()));
            CHECK(g.objects.advance(.01));
            for (auto id : g.objects.snapshot())
                if (auto* a = dynamic_cast<ChosenAI*>(g.objects.get(id)->game.get()))
                    CHECK(a->RuntimeReady() && a->runtime_updates > 0);
            CHECK(g.unload_world());
        }
        return true;
    }
    bool actions(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "GabriellaAI", {-400, 100, 0});
        auto* rev = create(g, "GabriellaREV", {0, 100, 0});
        auto* o = create(g, "OpheliaAI", {400, 100, 0});
        auto* orv = create(g, "OpheliaREV", {800, 100, 0});
        CHECK(a && rev && o && orv);
        for (auto* c : {a, rev, o, orv})
        {
            c->m_vTargetPos = c->m_MoveObj.GetPos();
            c->m_fStimuli[0] = .8f;
            c->m_fStimuli[5] = .14f;
            CHECK(c->ComputeState(1) && c->m_nState == AI_Mgr::StateFindHealth);
            c->m_fStimuli[5] = .15f;
            CHECK(c->ComputeState(1) && c->m_nState == AI_Mgr::StateAttackFar);
            c->m_fStimuli[5] = .5f;
            CHECK(c->ComputeState(1) && c->m_nState == AI_Mgr::StateAttackClose);
            CHECK(c->SetNewState(AI_Mgr::StateEscapeRun) && c->ComputeState(0));
            CHECK(c->m_nState == AI_Mgr::StateSearchVisual && c->m_nLastState == AI_Mgr::StateIdle);
            CHECK(c->SetNewState(AI_Mgr::StateAttackClose));
            c->Metacmd = 3;
            c->m_nCurMetacmd = 7;
            c->m_bAnimating = true;
            CHECK(c->AI_STATE_AttackClose());
            CHECK(c->m_nState == (c->Revenant() ? AI_Mgr::StateSearchVisual : AI_Mgr::StateAttackClose));
        }
        // Real sniper uses crouching fire even with full health and a level target.
        CHECK(o->SetNewTarget(a->m_hObject));
        CHECK(o->m_InventoryMgr.ChangeWeapon(WEAP_SNIPERRIFLE) == InventoryResult::Ok);
        CHECK(o->SetNewState(AI_Mgr::StateAttackClose));
        o->Metacmd = 3;
        o->m_fStimuli[5] = 1;
        o->m_vTargetPos = vector_add(o->m_MoveObj.GetPos(), {0, 0, 50});
        o->m_nDodgeFlags = 0;
        CHECK(o->AI_STATE_AttackClose() && o->m_nCurMetacmd == 11);
        // Actual stationary Minigun alternate, moving primary, and excluded Sniper alternate.
        CHECK(a->SetNewTarget(rev->m_hObject));
        CHECK(a->Fire(false) && a->fire_shots == 1 && a->alternate_choices == 1);
        CHECK(a->m_InventoryMgr.CurrentWeapon()->last_shot.request.alternate);
        DVector moving{6, 0, 0};
        CHECK(g.server.SetVelocity(a->m_hObject, &moving) == DE_OK);
        CHECK(a->Fire(true) && a->fire_shots == 2 && a->alternate_choices == 1);
        CHECK(!a->m_InventoryMgr.CurrentWeapon()->last_shot.request.alternate);
        DVector stopped{};
        CHECK(g.server.SetVelocity(a->m_hObject, &stopped) == DE_OK);
        CHECK(o->SetNewTarget(rev->m_hObject));
        o->m_vTargetPos = rev->m_MoveObj.GetPos();
        CHECK(o->Fire(true) && o->fire_shots == 1 && o->alternate_choices == 0);
        CHECK(!o->m_InventoryMgr.CurrentWeapon()->last_shot.request.alternate);
        // The inherited virtual dispatch must select original two-weapon switching.
        AI_Mgr* base = a;
        a->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 0);
        a->m_bAnimating = false;
        a->Metacmd = 2;
        CHECK(base->MC_BestWeapon() && a->m_bAnimating);
        auto* r = g.objects.get(a->m_hObject);
        r->animation.done = true;
        CHECK(base->MC_BestWeapon() && a->weapon_switches == 1 && a->m_bAnimating);
        CHECK(a->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_NAPALMCANNON);
        r->animation.done = true;
        CHECK(base->MC_BestWeapon() && !a->m_bAnimating && a->Metacmd == 3);
        // Non-ROLL left/right invokes the original one-second sidestep instead of skipping it.
        CHECK(rev->SetNewState(AI_Mgr::StateDodge));
        rev->Metacmd = 2;
        rev->m_nDodgeFlags = 0x24;
        CHECK(rev->AI_STATE_Dodge() && rev->m_nCurMetacmd == 22 && rev->sidesteps == 1);
        DVector velocity;
        CHECK(g.server.GetVelocity(rev->m_hObject, &velocity) == DE_OK);
        CHECK(std::abs(vector_dot(velocity, rev->m_MoveObj.GetRightVector()) - 105) < .001f);
        rev->m_fTimeStart = g.server.GetTime() - 1;
        CHECK(rev->AI_STATE_Dodge() && rev->Metacmd == 3 && !rev->m_bAnimating);
        CHECK(g.server.GetVelocity(rev->m_hObject, &velocity) == DE_OK && vector_dot(velocity, velocity) > 0);
        // No ExitHint: original escape/hide choices remain distinct.
        CHECK(a->SetNewState(AI_Mgr::StateEscapeRun) && a->AI_STATE_Escape_RunAway() &&
              a->m_nState == AI_Mgr::StateAttackFar);
        CHECK(a->SetNewState(AI_Mgr::StateEscapeHide) && a->AI_STATE_Escape_Hide() &&
              a->m_nState == AI_Mgr::StateAttackClose);
        return g.unload_world();
    }
    bool worlds(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        unsigned count = 0;
        for (const auto* world : {"23_rooftop", "24b_arena", "25_ancientone", "end"})
        {
            CHECK(g.request_world(world, error));
            for (std::size_t i = 0; i < g.entries.size(); ++i)
            {
                const auto& name = g.world.objects[i].class_name;
                if (name != "GabriellaAI" && name != "GabriellaREV" && name != "OpheliaAI" && name != "OpheliaREV")
                    continue;
                if (g.entries[i].state != EntryState::Created)
                    std::cerr << world << " " << name << " " << g.entries[i].detail << '\n';
                CHECK(g.entries[i].state == EntryState::Created);
                auto* a = dynamic_cast<ChosenAI*>(g.objects.get(g.entries[i].object)->game.get());
                CHECK(a && a->RuntimeReady());
                ++count;
                std::cout << world << " " << name << " HP=" << a->m_damage.m_fHitPoints
                          << " armor=" << a->m_damage.m_fArmorPoints << '\n';
            }
            CHECK(g.unload_world());
        }
        CHECK(count == 7);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    return (std::string_view(argv[1]) == "profiles" ? profiles(argv[2])
            : std::string_view(argv[1]) == "worlds" ? worlds(argv[2])
                                                    : actions(argv[2]))
               ? 0
               : 1;
}
