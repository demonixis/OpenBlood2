#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "gideon_fixture.h"
#include <cmath>
#include <functional>
#include <limits>
#include <map>
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
    bool setup(GameSession& g, const char* root)
    {
        CHECK(g.initialize() && test_gideon::floor(g));
        std::string error;
        std::filesystem::create_directories(root);
        CHECK(test_shield::mount(g.resources, root, error));
        return true;
    }
    bool decisions(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        a->m_fStimuli = {.95f, 0, 0, 0, .75f, .25f};
        for (auto [from, to] : {std::pair{AI_Mgr::StateIdle, AI_Mgr::StateSpecial1},
                                {AI_Mgr::StateTeleport, AI_Mgr::StateSearchVisual},
                                {AI_Mgr::StateSearchVisual, AI_Mgr::StateIdle},
                                {AI_Mgr::StateAttackClose, AI_Mgr::StateTeleport}})
        {
            a->m_nState = from;
            CHECK(a->ComputeState(0) && a->m_nState == to);
        }
        for (auto [sight, to] : {std::pair{.95f, AI_Mgr::StateAttackClose},
                                 {.94f, AI_Mgr::StateAttackFar},
                                 {.7501f, AI_Mgr::StateAttackFar},
                                 {.75f, AI_Mgr::StateSpecial1}})
        {
            a->m_nState = AI_Mgr::StateIdle;
            a->m_fStimuli[0] = sight;
            CHECK(a->ComputeState(1) && a->m_nState == to);
        }
        a->m_nState = AI_Mgr::StateSearchVisual;
        a->m_fStimuli[0] = 1;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateSpecial1);
        a->m_fStimuli[5] = .05f;
        CHECK(g.step(15));
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackClose);
        a->m_fStimuli[5] = .049f;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateTeleport && a->m_fLastPanic == 15);
        CHECK(g.step(15));
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateTeleport && a->m_fLastPanic == 30);
        CHECK(g.step(15.01));
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackClose && a->m_fLastPanic == 30);
        a->m_fLastPanic = 60;
        CHECK(g.step(14.99)); // exact shield deadline: panic still allowed
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateTeleport);
        CHECK(g.step(.01));
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackClose);
        a->m_fStimuli[5] = 1;
        a->m_fStimuli[4] = .7501f;
        a->m_fStimuli[0] = 0;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackClose);
        const auto old = a->m_nState;
        a->m_fStimuli[4] = NAN;
        CHECK(!a->ComputeState(1) && a->m_nState == old);
        a->m_fStimuli[4] = 0;
        CHECK(!a->ComputeState(5));
        // Recomputed perception failure must not become a usable stimulus.
        a->m_fSeeingDist = NAN;
        CHECK(!a->ComputeState() && a->m_nState == old);
        return true;
    }
    bool shield(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        a->m_damage.SetArmorPoints(25);
        CHECK(g.step(60));
        CHECK(a->MC_Fire_Stand() && a->shield_grants == 0 && a->attack_roll >= 0);
        a->m_bAnimating = false;
        CHECK(g.step(.001));
        CHECK(a->MC_Fire_Stand() && a->shield_grants == 1 && a->attack_roll == -1 && a->selected_weapon == 41);
        CHECK(a->m_damage.m_fArmorPoints == 125 && a->m_fLastShield > 60 && a->m_bAnimating);
        CHECK(a->fire_calls == 0 && g.server.object_particles.active.empty());
        // The boss command does not borrow base timed-fire behavior.
        CHECK(a->MC_Fire_Stand() && a->fire_calls == 0 && a->shield_grants == 1);
        CHECK(g.server.advance_models(.2) && a->MC_Fire_Stand());
        CHECK(!a->m_bAnimating && a->Metacmd == 2 && a->animation_finishes == 1 && a->fire_calls == 0);
        CHECK(a->m_InventoryMgr.CurrentWeapon()->data.m_nType == 41 &&
              a->completion_selection == InventoryResult::Invalid);
        CHECK(a->Fire() && a->m_damage.m_fArmorPoints == 125 && g.server.object_particles.active.size() == 1);
        a->m_damage.SetArmorPoints(25.001f);
        a->m_fLastShield = -100;
        CHECK(a->MC_Fire_Stand() && a->shield_grants == 1);
        return true;
    }
    bool choices(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        auto table = std::make_shared<CAnim_Sound>();
        table->m_nAnim_FIRE_STAND[TYPE_MAGIC] = 1;
        CHECK(a->InitStatics(table));
        bool energy = false, naga = false, wind = false;
        for (int i = 0; i < 80; ++i)
        {
            a->m_nState = i < 40 ? AI_Mgr::StateAttackFar : AI_Mgr::StateAttackClose;
            a->m_bAnimating = false;
            CHECK(a->MC_Fire_Stand());
            const auto expected = i < 40 ? (a->attack_roll < 70 ? 27 : 38) : (a->attack_roll < 50 ? 42 : 38);
            CHECK(a->selected_weapon == expected && a->m_InventoryMgr.CurrentWeapon()->data.m_nType == expected);
            CHECK(a->requested_animation == unsigned(expected == 27 ? 1 : 0));
            energy |= expected == 27;
            naga |= expected == 38;
            wind |= expected == 42;
        }
        CHECK(energy && naga && wind && a->fire_calls == 0);
        return true;
    }
    bool aim(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        CHECK(a->m_InventoryMgr.ChangeWeapon(41) == InventoryResult::Current);
        g.server.collision_ready = false;
        AI_Mgr* base = a;
        CHECK(base->Fire() && a->fire_gun_node && a->clear_shot.rays == 0 && a->fire_shots == 1);
        DVector expected;
        DRotation rot;
        CHECK(g.server.GetModelNodeTransform(a->m_hObject, "rr_gun", &expected, &rot));
        CHECK(near(a->fire_origin.x, expected.x) && near(a->fire_origin.y, expected.y));
        // No node -> CMovement cached forward, deliberately different from current pose and target.
        a->m_szFireNode = "missing";
        DRotation turned{{0, .70710678f, 0}, .70710678f};
        CHECK(g.server.SetObjectRotation(a->m_hObject, &turned) == DE_OK);
        a->m_vTargetPos = {2000, 10, 0};
        CHECK(a->Fire(true) && !a->fire_gun_node && near(a->fire_direction.z, 1) && near(a->fire_direction.x, 0));
        CHECK(a->m_MoveObj.Update());
        CHECK(a->Fire() && near(a->fire_direction.x, 1));
        a->m_MoveObj.sampled = false; // Deliberately unavailable cached sample.
        CHECK(!a->Fire() && a->fire_result == FireResult::Unavailable);
        a->m_szFireNode = "rr_gun";
        CHECK(a->Fire()); // Valid node does not need the movement fallback.
        CHECK(g.server.GetModelNodeTransform(a->m_hObject, "rr_gun", &a->m_vTargetPos, &rot));
        CHECK(!a->Fire() && a->fire_result == FireResult::Invalid);
        return true;
    }
    bool keys(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        // extra_key does not have fire_key's target guard.
        CHECK(test_gideon::key(g, *a, "[extra_key:powerup_suffix][UpVel:7]"));
        CHECK(a->key_outcome == AIKeyOutcome::ExtraAction && a->powerup_calls == 1 && a->powerup_effect &&
              g.objects.get(a->m_hObject)->velocity.y == 7 && a->fire_calls == 0);
        CHECK(test_gideon::key(g, *a, "[extra_key:rr_gun]"));
        CHECK(a->fire_shots == 1 && a->key_outcome == AIKeyOutcome::ExtraAction);
        CHECK(test_gideon::key(g, *a, "[fire_key:1][extra_key:powerup]"));
        CHECK(a->no_target == 1 && a->extra_calls == 2 && a->powerup_calls == 1);
        auto* t = test_energy::actor(g, {0, 10, 500});
        CHECK(t && a->SetNewTarget(t->m_hObject));
        CHECK(test_gideon::key(g, *a, "[fire_key:1]"));
        CHECK(a->fire_shots == 2 && a->key_outcome == AIKeyOutcome::FireAttempt);
        CHECK(a->MC_Extra("POWERUP")); // case-sensitive: this is a node name, with body fallback.
        CHECK(a->m_szFireNode == "POWERUP" && a->powerup_calls == 1 && a->fire_shots == 3);
        const auto node = a->m_szFireNode;
        CHECK(!a->MC_Extra(std::string(64, 'x')) && a->m_szFireNode == node);
        CHECK(!a->MC_Extra(std::string_view("x\0y", 3)) && !a->MC_Extra(""));
        CHECK(g.objects.remove(t->m_hObject));
        const auto count = a->fire_calls;
        CHECK(test_gideon::key(g, *a, "[fire_key:1]"));
        CHECK(a->key_outcome == AIKeyOutcome::Invalid && a->fire_calls == count);
        CHECK(!a->MC_Extra("rr_gun") && a->fire_result == FireResult::Invalid);
        CHECK(g.objects.finish_frame());
        CHECK(!a->GetTarget());
        return true;
    }
    bool failures(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        auto table = std::make_shared<CAnim_Sound>();
        table->m_nAnim_FIRE_STAND[0] = UINT32_MAX;
        CHECK(a->InitStatics(table));
        CHECK(g.step(60) && g.step(1));
        a->m_damage.SetArmorPoints(0);
        CHECK(!a->MC_Fire_Stand() && !a->m_bAnimating && a->shield_grants == 1 && a->m_damage.m_fArmorPoints == 100);
        a->m_InventoryMgr.ClearWeapons();
        CHECK(!a->Fire() && a->fire_result == FireResult::Unavailable);
        a->m_fLastShield = -100;
        a->m_damage.SetArmorPoints(0);
        CHECK(!a->MC_Fire_Stand() && a->m_damage.m_fArmorPoints == 0 && a->shield_grants == 1);
        CHECK(g.objects.remove(a->m_hObject));
        CHECK(!a->ComputeState(0) && !a->MC_Fire_Stand() && !a->Fire() && !a->MC_Extra("powerup"));
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool lifetime(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        auto* t = test_energy::actor(g, {0, 10, 80});
        CHECK(t && a->SetNewTarget(t->m_hObject));
        auto h = a->m_hObject;
        CHECK(a->MagicPowerup());
        CHECK(a->m_InventoryMgr.ChangeWeapon(38) == InventoryResult::Ok);
        t->remove_on_damage = h;
        auto* r = g.objects.get(h);
        auto model = std::make_shared<Model>(*r->model);
        r->model = model;
        model->animations[0].keys = {
            {0, {}, {}, "[extra_key:rr_gun]"}, {10, {}, {}, "[extra_key:powerup]"}, {100, {}, {}, ""}};
        CHECK(g.server.ResetModelAnimation(h) == DE_OK);
        const auto destroyed = test_gideon::Actor::destroyed;
        CHECK(g.server.advance_models(.2));
        // advance_models finishes its deferred batch; the actor is now destroyed.
        CHECK(!g.objects.get(h) && test_gideon::Actor::destroyed == destroyed + 1 &&
              test_gideon::Actor::final_extra == 1 && test_gideon::Actor::final_powerup == 1 &&
              t->damage.m_fHitPoints < 1000);
        CHECK(g.objects.finish_frame());
        CHECK(g.server.object_particles.advance(.2, g.server));
        CHECK(g.server.object_particles.active.empty());
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool private_actions(const char* root)
    {
        GameSession g;
        CHECK(g.initialize() && test_gideon::floor(g));
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            auto* a = test_gideon::actor(g, true, {0, 100, 0});
            CHECK(a);
            auto* t = test_energy::actor(g, {0, 100, 500});
            CHECK(t && a->SetNewTarget(t->m_hObject));
            a->m_fLastShield = -100;
            a->m_damage.SetArmorPoints(0);
            CHECK(a->MC_Fire_Stand());
            const auto clip = g.server.GetModelAnimation(a->m_hObject);
            const auto* r = g.objects.get(a->m_hObject);
            unsigned authored_keys = 0;
            for (const auto& key : r->model->animations[clip].keys)
                if (!key.text.empty())
                {
                    ++authored_keys;
                    std::cout << "shield-key=" << key.text << '\n';
                }
            CHECK(authored_keys > 0);
            CHECK(r->model->animations[clip].duration < 30000);
            const auto steps = r->model->animations[clip].duration * 60 / 1000 + 2;
            for (unsigned i = 0; i < steps && a->m_bAnimating; ++i)
            {
                CHECK(g.step(1. / 60));
                CHECK(a->MC_Fire_Stand());
            }
            std::cout << "cycle=" << cycle << " shield_clip=" << clip << " keys=" << a->model_keys
                      << " shots=" << a->fire_shots << " armor=" << a->m_damage.m_fArmorPoints
                      << " duration_ms=" << r->model->animations[clip].duration << " animating=" << a->m_bAnimating
                      << " key_failures=" << a->key_failures << " key_error=" << a->ai_key_error << '\n';
            CHECK(!a->m_bAnimating && a->fire_shots > 0 && a->m_damage.m_fArmorPoints == 100 && a->key_failures == 0);
            // Run the original magic clip: fire_key values name both guns, but the source ignores those values.
            a->m_nState = AI_Mgr::StateAttackFar;
            for (unsigned attempt = 0; attempt < 128; ++attempt)
            {
                a->m_bAnimating = false;
                CHECK(a->MC_Fire_Stand());
                if (a->selected_weapon == WEAP_ZEALOT_ENERGYBLAST)
                    break;
            }
            CHECK(a->selected_weapon == WEAP_ZEALOT_ENERGYBLAST);
            const auto magic_clip = g.server.GetModelAnimation(a->m_hObject);
            CHECK(magic_clip != clip && r->model->animations[magic_clip].duration < 30000);
            unsigned expected_shots = 0;
            for (const auto& key : r->model->animations[magic_clip].keys)
                if (!key.text.empty())
                {
                    std::cout << "magic-key=" << key.text << '\n';
                    if (key.text.find("[fire_key:") != key.text.npos ||
                        (key.text.find("[extra_key:") != key.text.npos &&
                         key.text.find("[extra_key:powerup") == key.text.npos))
                        ++expected_shots;
                }
            const auto before = a->fire_shots;
            const auto nodes_before = a->fired_nodes.size();
            const auto magic_steps = r->model->animations[magic_clip].duration * 60 / 1000 + 2;
            for (unsigned i = 0; i < magic_steps && a->m_bAnimating; ++i)
            {
                CHECK(g.step(1. / 60));
                CHECK(a->MC_Fire_Stand());
            }
            std::cout << "magic_clip=" << magic_clip << " shots=" << (a->fire_shots - before)
                      << " expected=" << expected_shots << " last_node=" << a->m_szFireNode
                      << " failures=" << a->key_failures << " key_error=" << a->ai_key_error << '\n';
            CHECK(expected_shots > 0 && a->fire_shots - before == expected_shots && a->key_failures == 0 &&
                  !a->m_bAnimating);
            CHECK(a->fired_nodes.size() == nodes_before + expected_shots);
            for (auto i = nodes_before; i < a->fired_nodes.size(); ++i)
                CHECK(a->fired_nodes[i] == "rr_gun");
            CHECK(a->MC_Extra("powerup") && a->powerup_effect);
            CHECK(g.step(.11));
            SpriteView sprites;
            CHECK(update_sprite_scene(g, sprites, error));
            CHECK(!sprites.draws.empty());
            ModelView models;
            CHECK(update_model_scene(g, models, error));
            CHECK(!models.world_draws.empty());
            // All four weapons use Gideon's actual rr_gun, never generic head_gun/r_hand_extra2.
            for (auto id : {41, 27, 38, 42})
            {
                const auto result = a->m_InventoryMgr.ChangeWeapon(id);
                CHECK(result == InventoryResult::Ok || result == InventoryResult::Current);
                CHECK(a->Fire() && a->fire_gun_node && a->clear_shot.rays == 0);
                std::cout << "weapon=" << id << " node=" << a->m_szFireNode << " origin=" << a->fire_origin.x << ','
                          << a->fire_origin.y << ',' << a->fire_origin.z << '\n';
            }
            auto h = a->m_hObject;
            CHECK(g.unload_world() && !g.objects.get(h));
            CHECK(g.server.object_particles.active.empty() && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
            CHECK(test_gideon::floor(g));
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> cases{
        {"decisions", decisions}, {"shield", shield},     {"choices", choices},   {"aim", aim},
        {"keys", keys},           {"failures", failures}, {"lifetime", lifetime}, {"private", private_actions}};
    auto found = cases.find(argv[1]);
    return found != cases.end() && found->second(argv[2]) ? 0 : 1;
}
