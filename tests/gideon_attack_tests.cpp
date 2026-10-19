#include "app/model_scene.h"
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
    bool near(DVector a, DVector b)
    {
        return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) < .003f;
    }
    bool setup(GameSession& g, const char* root)
    {
        std::string error;
        std::filesystem::create_directories(root);
        CHECK(g.initialize() && test_gideon::floor(g) && test_shield::mount(g.resources, root, error));
        return true;
    }
    test_gideon::Actor* actor(GameSession& g)
    {
        auto* a = test_gideon::actor(g);
        auto* t = test_energy::actor(g, {0, 10, 300});
        if (!a || !t || !a->SetNewTarget(t->m_hObject) || !a->RefreshTargetPosition())
            return nullptr;
        a->m_fRollSpeed = 3.5f; // Selected original constructor value, not production initialization.
        a->m_fSeeingDist = 1000;
        return a;
    }
    bool dodge(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        const auto pos = r->state.m_Pos;
        CHECK(a->MC_Dodge_Right() && a->m_nCurMetacmd == 22 && a->Metacmd == 1 && !r->animation.looping);
        CHECK(near(r->velocity, {3.5f, 0, 0}) && near(r->state.m_Pos, pos));
        CHECK(g.server.advance_models(.5) && (g.server.GetModelPlaybackState(a->m_hObject) & MS_PLAYDONE));
        CHECK(a->MC_Dodge_Right() && a->m_bAnimating && a->Metacmd == 1 && a->dodge_elapsed == 0);
        CHECK(g.step(.5) && a->MC_Dodge_Right());
        CHECK(near(r->state.m_Pos, {pos.x + 1.75f, pos.y, pos.z}) && a->m_bAnimating);
        CHECK(g.step(.25) && a->MC_Dodge_Right() && a->Metacmd == 1);
        CHECK(g.step(.25) && a->MC_Dodge_Right() && !a->m_bAnimating && a->Metacmd == 2);
        CHECK(a->dodge_finishes == 1 && a->dodge_elapsed == 1 && near(r->velocity, {3.5f, 0, 0}));
        CHECK(a->MC_Dodge_Left() && a->m_nCurMetacmd == 21 && near(r->velocity, {-3.5f, 0, 0}));
        // Changing command restarts the one-second deadline even during playback.
        CHECK(g.step(.5) && a->MC_Dodge_Right() && a->m_fTimeStart == 1.5f);
        CHECK(g.step(.5) && a->MC_Dodge_Right() && a->Metacmd == 2);
        CHECK(g.step(.5) && a->MC_Dodge_Right() && a->Metacmd == 3 && a->dodge_finishes == 2);
        return true;
    }
    bool cached(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        DRotation turned{{0, .70710678f, 0}, .70710678f};
        CHECK(g.server.SetObjectRotation(a->m_hObject, &turned) == DE_OK);
        CHECK(a->MC_Dodge_Left() && near(r->velocity, {-3.5f, 0, 0})); // old right
        CHECK(a->m_MoveObj.Update());
        CHECK(a->MC_Dodge_Left() && near(r->velocity, {0, 0, 3.5f})); // new cached right
        a->m_nTrapped = 1;
        CHECK(a->MC_Dodge_Left() && near(r->velocity, {}));
        a->m_nTrapped = 0;
        // Cached position, not current object position, determines attack range.
        DVector moved{0, 10, 2000};
        CHECK(g.server.TeleportObject(a->m_hObject, &moved) == DE_OK);
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        a->Metacmd = 2;
        a->m_vTargetPos = {0, 10, 900};
        CHECK(a->DispatchState() && a->attack_distance == 900 && a->m_nCurMetacmd == 7);
        CHECK(a->m_MoveObj.Update());
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        a->Metacmd = 2;
        CHECK(a->DispatchState() && a->attack_distance == 1100 && a->m_nState == AI_Mgr::StateSpecial1);
        return true;
    }
    bool taunt(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        a->AnimSound()->m_nAnim_TAUNT[4] = 1;
        CHECK(a->MC_Taunt_Bold() && a->m_nCurMetacmd == 18 && a->requested_animation == 1);
        CHECK(near(r->velocity, {3.5f, 0, 0}) && !r->animation.looping);
        DVector velocity{7, 8, 9};
        CHECK(g.server.SetVelocity(a->m_hObject, &velocity) == DE_OK);
        CHECK(a->MC_Taunt_Bold() && near(r->velocity, velocity)); // no repeated Move
        CHECK(g.server.advance_models(.2) && a->MC_Taunt_Bold() && a->Metacmd == 2 && !a->m_bAnimating);
        CHECK(near(r->velocity, velocity)); // no StopVelocity on completion
        CHECK(a->MC_Taunt_Beg() && a->m_nCurMetacmd == 17 && a->requested_animation == 0);
        CHECK(a->MC_Taunt_Bold() && a->m_nCurMetacmd == 18 && a->requested_animation == 1);
        return true;
    }
    bool attack_close(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->DispatchState() && a->Metacmd == 2);
        a->m_vTargetPos = {0, 10, 900};
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateAttackClose && a->m_nCurMetacmd == 7 &&
              a->Metacmd == 2);
        a->m_vTargetPos = {0, 10, 5000}; // already firing continues beyond the initial range gate
        CHECK(a->DispatchState() && a->Metacmd == 2 && a->fire_calls == 0);
        CHECK(g.server.advance_models(.2) && a->DispatchState() && a->Metacmd == 3);
        CHECK(a->DispatchState() && a->dodge_roll >= 0 && a->dodge_roll <= 99);
        const int command = a->m_nCurMetacmd, roll = a->dodge_roll;
        CHECK(command == (roll < 50 ? 22 : 21));
        a->dodge_roll = -2; // continued command must not consume a new draw
        CHECK(a->DispatchState() && a->dodge_roll == -2 && a->m_nCurMetacmd == command);
        CHECK(g.step(.5) && g.step(.5) && a->DispatchState() && a->Metacmd == 4);
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateTeleport); // no faction candidates
        auto* target = test_energy::actor(g, {0, 10, 900.01f});
        CHECK(target && a->SetNewTarget(target->m_hObject) && a->RefreshTargetPosition());
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        a->Metacmd = 2;
        a->m_vTargetPos = {0, 10, 900.01f};
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateSpecial1 && a->Metacmd == 1);
        bool left = false, right = false;
        for (unsigned i = 0; i < 50; ++i)
        {
            CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
            a->Metacmd = 3;
            CHECK(a->DispatchState());
            left |= a->m_nCurMetacmd == 21;
            right |= a->m_nCurMetacmd == 22;
        }
        CHECK(left && right);
        return true;
    }
    bool far(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar));
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 7 && a->Metacmd == 1);
        a->m_vTargetPos = {0, 10, 5000};
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateAttackFar && a->Metacmd == 1);
        CHECK(g.server.advance_models(.2) && a->DispatchState() && a->Metacmd == 2);
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 18 && a->Metacmd == 2);
        CHECK(g.server.advance_models(.2) && a->DispatchState() && a->Metacmd == 3);
        a->m_vTargetPos = {0, 10, 400};
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateSpecial1);
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar));
        a->m_vTargetPos = {0, 10, 300.01f};
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateSpecial1);
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar));
        a->Metacmd = 3;
        a->m_vTargetPos = {0, 10, 400.01f};
        a->m_fWalkSpeed = 75;
        CHECK(a->DispatchState() && a->m_nCurMetacmd == 1 && a->Metacmd == 3);
        CHECK(near(g.objects.get(a->m_hObject)->velocity, {0, 0, 75}));
        CHECK(a->SetNewState(AI_Mgr::StateRecoil) && a->DispatchState());
        CHECK(a->m_nState == AI_Mgr::StateSpecial1 && a->Metacmd == 1 && a->recoil_starts == 0);
        CHECK(a->SetNewTarget(nullptr));
        for (auto state : {AI_Mgr::StateAttackClose, AI_Mgr::StateAttackFar})
        {
            CHECK(a->SetNewState(state) && a->DispatchState());
            CHECK(a->m_nState == AI_Mgr::StateSearchVisual);
        }
        return true;
    }
    bool failures(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        a->AnimSound()->m_nAnim_DODGE_LEFT = UINT32_MAX;
        CHECK(!a->MC_Dodge_Left() && !a->m_bAnimating && near(r->velocity, {}));
        CHECK(a->MC_Dodge_Right());
        a->Metacmd = std::numeric_limits<int>::max();
        CHECK(g.step(.5) && g.step(.5));
        const auto velocity = r->velocity;
        CHECK(!a->MC_Dodge_Right() && a->m_bAnimating && a->dodge_finishes == 0 && near(r->velocity, velocity));
        a->Metacmd = 1;
        a->m_fTimeStart = NAN;
        CHECK(!a->MC_Dodge_Right());
        a->m_fTimeStart = 100;
        CHECK(!a->MC_Dodge_Right());
        a->m_fTimeStart = 0;
        a->m_fRollSpeed = NAN;
        CHECK(!a->MC_Dodge_Right() && !a->MC_Taunt_Bold());
        a->m_fRollSpeed = 3.5f;
        a->m_MoveObj.sampled = false;
        CHECK(!a->MC_Dodge_Right());
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar) && !a->DispatchState());
        a->m_MoveObj.sampled = true;
        a->m_fSeeingDist = NAN;
        CHECK(!a->DispatchState());
        a->m_fSeeingDist = 1000;
        a->target_position_valid = false;
        CHECK(!a->DispatchState() && a->Metacmd == 1); // failed facing never decrements
        return true;
    }
    bool lifetime(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        const auto target = a->GetTarget(), owner = a->m_hObject;
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        CHECK(g.objects.remove(target) && !a->DispatchState() && a->Metacmd == 1);
        CHECK(g.objects.finish_frame() && !a->GetTarget());
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateSearchVisual);
        const auto destroyed = test_gideon::Actor::destroyed;
        CHECK(g.objects.remove(owner));
        CHECK(!a->MC_Dodge_Left() && !a->MC_Dodge_Right() && !a->MC_Taunt_Bold() && !a->AI_STATE_Recoil());
        CHECK(!a->AI_STATE_AttackClose() && !a->AI_STATE_AttackFar());
        CHECK(test_gideon::Actor::destroyed == destroyed && g.objects.finish_frame());
        CHECK(test_gideon::Actor::destroyed == destroyed + 1 && !g.objects.get(owner));
        CHECK(g.unload_world() && !g.server.string_count() && !g.server.message_count());
        return true;
    }
    bool private_actions(const char* root)
    {
        GameSession g;
        std::string error;
        Config config;
        config.data_dir = root;
        CHECK(g.initialize() && test_gideon::floor(g) && g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            auto* a = test_gideon::actor(g, true, {0, 100, 0});
            CHECK(a);
            auto* t = test_energy::actor(g, {0, 100, 200});
            CHECK(t);
            CHECK(a->SetNewTarget(t->m_hObject) && a->RefreshTargetPosition());
            a->m_fRollSpeed = 3.5f;
            a->m_fSeeingDist = 1000;
            a->m_damage.SetArmorPoints(0);
            a->m_fLastShield = -100;
            CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->DispatchState() && a->DispatchState());
            // Actual shield animation -> original keys -> step3 dodge -> step4 decision.
            unsigned frames = 0;
            while (a->Metacmd == 2 && frames++ < 600)
            {
                CHECK(g.step(1. / 60) && a->DispatchState());
            }
            CHECK(a->Metacmd == 3 && a->fire_shots == 1 && a->powerup_calls == 1 && a->key_failures == 0);
            CHECK(a->DispatchState());
            const auto clip = g.server.GetModelAnimation(a->m_hObject);
            CHECK(clip == a->AnimSound()->m_nAnim_DODGE_LEFT || clip == a->AnimSound()->m_nAnim_DODGE_RIGHT);
            frames = 0;
            while (a->Metacmd == 3 && frames++ < 90)
            {
                CHECK(g.step(1. / 60) && a->DispatchState());
            }
            CHECK(a->Metacmd == 4 && a->dodge_finishes == 1 && a->dodge_elapsed >= 1 && a->dodge_elapsed < 1.02f);
            ModelView view;
            CHECK(update_model_scene(g, view, error) && !view.world_draws.empty());
            const auto clock = g.objects.get(a->m_hObject)->animation.time;
            const auto calls = a->attack_updates;
            CHECK(update_model_scene(g, view, error) && a->attack_updates == calls &&
                  g.objects.get(a->m_hObject)->animation.time == clock);
            CHECK(a->SetNewState(AI_Mgr::StateAttackFar));
            a->Metacmd = 2;
            CHECK(a->DispatchState() && a->m_nCurMetacmd == 18);
            const auto taunt_clip = g.server.GetModelAnimation(a->m_hObject);
            CHECK(taunt_clip == a->AnimSound()->m_nAnim_TAUNT[4]);
            frames = 0;
            while (a->Metacmd == 2 && frames++ < 600)
            {
                CHECK(g.step(1. / 60) && a->DispatchState());
            }
            CHECK(a->Metacmd == 3 && a->key_failures == 0);
            std::cout << "cycle=" << cycle << " dodge_clip=" << clip << " elapsed=" << a->dodge_elapsed
                      << " taunt_clip=" << taunt_clip << " original_shield_shots=" << a->fire_shots
                      << " updates=" << a->attack_updates << '\n';
            const auto h = a->m_hObject;
            CHECK(g.unload_world() && !g.objects.get(h) && !g.server.string_count() && !g.server.message_count());
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
        {"dodge", dodge}, {"cached", cached},     {"taunt", taunt},       {"close", attack_close},
        {"far", far},     {"failures", failures}, {"lifetime", lifetime}, {"private", private_actions}};
    auto test = cases.find(argv[1]);
    return test != cases.end() && test->second(argv[2]) ? 0 : 1;
}
