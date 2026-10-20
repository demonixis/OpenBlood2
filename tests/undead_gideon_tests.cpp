#include "brush_fixture.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/PlayerObj.h"
#include "game/original/UndeadGideon.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
#include <set>
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
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(g.initialize() && g.resources.mount_game(c, e));
        auto floor = test_brush::box("floor", {-10000, -20, -10000}, {10000, -10, 10000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(g.collision.build(floor, e));
        g.server.collision_ready = true;
        return true;
    }
    UndeadGideon* create(GameSession& g, DVector pos = {0, 300, 0})
    {
        auto h = g.objects.create(g.objects.find_class("UndeadGideon"), {}, Creation::World, {{"Pos", pos}});
        auto* a = dynamic_cast<UndeadGideon*>(g.server.HandleToObject(h));
        if (a && !a->RuntimeReady())
            std::cerr << a->runtime_error << '\n';
        return a && a->m_MoveObj.Update() ? a : nullptr;
    }
    bool profiles(const char* root)
    {
        for (int difficulty : {0, 1, 2})
        {
            GameSession g;
            CHECK(setup(g, root));
            g.server.set_game_settings(difficulty, 0);
            auto* a = create(g, {-500, 300, 0});
            auto* b = create(g, {500, 300, 0});
            CHECK(a && b && a->RuntimeReady() && b->RuntimeReady());
            float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1.f;
            CHECK(a->m_damage.m_fHitPoints >= 12001 * factor && a->m_damage.m_fHitPoints <= 12050 * factor);
            CHECK(a->m_damage.m_fArmorPoints == 0 && a->m_fAIMass == 3000 && a->m_damage.apply_damage_physics &&
                  !a->m_bCabal);
            CHECK(a->m_fWalkSpeed == 180 && a->m_fRunSpeed == 350 && a->m_fRollSpeed == 320 && a->m_fJumpSpeed == 360);
            CHECK(a->m_fSeeingDist == 1000 && a->m_fSensingDist == 500 && a->m_fHearingDist == 2000 &&
                  a->m_fSmellingDist == 2000 && a->m_dwFlags == 0x2a);
            CHECK(a->AnimSound() == b->AnimSound() && a->AnimSound()->GetSoundRoot() == "sounds/enemies/Undead_Gideon");
            const int weapons[] = {45, 48, 43, 31, 36};
            for (unsigned i = 0; i < 5; ++i)
                CHECK(a->m_InventoryMgr.GetWeapon(i) && a->m_InventoryMgr.GetWeapon(i)->data.m_nType == weapons[i]);
            CHECK(g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY);
            CHECK(a->m_fFullHealth == 0 && g.objects.advance(.002) && a->RuntimeReady());
            CHECK(!a->m_bCreateHealth && a->m_fFullHealth == a->m_damage.m_fHitPoints && a->health_requests == 1 &&
                  a->health_no_client == 1);
            CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr) && a->health_requests == 1);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.string_count());
        }
        return true;
    }
    bool decisions(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g);
        CHECK(a && a->RuntimeReady());
        a->m_vTargetPos = a->m_MoveObj.GetPos();
        a->m_fStimuli[0] = .85f;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackFar);
        a->m_fStimuli[0] = .86f;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackClose);
        a->m_vTargetPos.y += a->m_vDims.y + 1;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackFar);
        CHECK(a->ComputeState(3) && a->m_nState == AI_Mgr::StateSearchSmell);
        CHECK(a->ComputeState(0) && a->m_nState == AI_Mgr::StateIdle);
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar) && a->ComputeState(0) && a->m_nState == AI_Mgr::StateSearchSmell);
        CHECK(a->AI_STATE_Recoil() && a->m_nState == AI_Mgr::StateDodge);
        a->Metacmd = 3;
        CHECK(a->AI_STATE_Dodge() && a->m_nState == AI_Mgr::StateIdle);
        a->m_vTargetPos = a->m_MoveObj.GetPos();
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        CHECK(a->AI_STATE_AttackClose() && a->m_nState == AI_Mgr::StateSearchVisual);
        CHECK(g.unload_world());
        return true;
    }
    bool choices(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g);
        CHECK(a && a->RuntimeReady());
        a->m_vTargetPos = a->m_MoveObj.GetPos();
        a->m_vTargetPos.z += 500;
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->MC_Fire_Stand());
        CHECK(a->selected_weapon == 45 && a->fire_column == TYPE_MELEE);
        a->m_vTargetPos.y += a->m_vDims.y + 1;
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar) && a->MC_Fire_Stand());
        CHECK(a->selected_weapon == 43 && a->fire_column == TYPE_MELEE);
        a->m_vTargetPos.y = a->m_MoveObj.GetPos().y;
        std::set<int> selected;
        for (int i = 0; i < 100; ++i)
        {
            CHECK(a->SetNewState(AI_Mgr::StateAttackFar) && a->MC_Fire_Stand());
            selected.insert(a->selected_weapon);
            CHECK(a->fire_column == (a->selected_weapon == 43 ? TYPE_RIFLE : TYPE_MAGIC));
        }
        CHECK(selected == std::set<int>({31, 43, 48}));
        ObjectCreateStruct state;
        state.m_Pos = {0, 300, 200};
        auto h = g.objects.create(g.objects.find_class("CPlayerObj"), state);
        auto* p = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
        CHECK(p && a->SetNewTarget(h) && a->RefreshTargetPosition() && a->MC_FaceTarget());
        p->m_bImprisoned = true;
        selected.clear();
        for (int i = 0; i < 100; ++i)
        {
            CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->MC_Fire_Stand());
            selected.insert(a->selected_weapon);
        }
        CHECK(selected == std::set<int>({36, 43}));
        p->m_bImprisoned = false;
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->MC_Fire_Stand() && a->selected_weapon == 45);
        CHECK(g.unload_world());
        return true;
    }
    bool movement(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g);
        CHECK(a && a->RuntimeReady());
        auto* body = g.objects.get(a->m_hObject);
        DVector grounded = body->state.m_Pos;
        grounded.y = -10 + body->dimensions.y + .005f;
        CHECK(g.server.MoveObject(a->m_hObject, &grounded) == DE_OK && a->m_MoveObj.Update());
        a->m_vTargetPos = a->m_MoveObj.GetPos();
        a->m_vTargetPos.z += 500;
        CHECK(a->MC_Jump());
        DVector velocity;
        CHECK(g.server.GetVelocity(a->m_hObject, &velocity) == DE_OK);
        CHECK(std::abs(velocity.z - 1050) < .01f && std::abs(velocity.y - 360) < .01f);
        velocity = {}; // Complete the airborne phase before testing primary-world support.
        CHECK(g.server.SetVelocity(a->m_hObject, &velocity) == DE_OK);
        const int phase = a->Metacmd;
        CHECK(a->MC_Jump() && a->m_nCurMetacmd == 999 && a->m_bAnimating && a->Metacmd == phase);
        CHECK(g.server.GetVelocity(a->m_hObject, &velocity) == DE_OK && vector_dot(velocity, velocity) == 0);
        CHECK(a->MC_Jump() && a->jumps == 2 && a->m_nCurMetacmd == 3);
        CHECK(a->SetNewState(AI_Mgr::StateDodge) && a->MC_Dodge_Left());
        CHECK(g.server.GetVelocity(a->m_hObject, &velocity) == DE_OK && velocity.x < -349 && velocity.y > 359);
        CHECK(a->MC_Dodge_Left() && a->m_bAnimating); // Positive vertical speed cannot finish.
        velocity = {};
        CHECK(g.server.SetVelocity(a->m_hObject, &velocity) == DE_OK);
        CHECK(a->MC_Dodge_Left() && !a->m_bAnimating && a->Metacmd == 2);
        // Virtual dispatch from generic navigation must also use the source boss jump.
        AI_Mgr* base = a;
        CHECK(base->MC_Jump() && a->jumps == 3);
        CHECK(g.unload_world());
        return true;
    }
    bool fire(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g);
        CHECK(a && a->RuntimeReady());
        CHECK(a->m_InventoryMgr.ChangeWeapon(43) == InventoryResult::Ok);
        DVector head;
        DRotation rotation;
        CHECK(g.server.GetModelNodeTransform(a->m_hObject, "head", &head, &rotation));
        a->m_vTargetPos = vector_add(head, {200, 100, 500});
        CHECK(a->Fire() && a->fire_result == FireResult::Fired && a->fire_shots == 1 && a->fire_gun_node &&
              a->clear_shot.rays == 0);
        CHECK(a->fire_origin.x == head.x && a->fire_origin.y == head.y && a->fire_origin.z == head.z);
        CHECK(std::abs(a->fire_direction.x) < 1e-6 && a->fire_direction.y > 0 && a->fire_direction.z > 0);
        auto h = a->m_InventoryMgr.CurrentWeapon()->projectile;
        auto* p = dynamic_cast<CVomitProjectile*>(g.server.HandleToObject(h));
        CHECK(p && p->trail);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        auto* record = g.objects.get(a->m_hObject);
        auto model = record->model;
        record->model.reset();
        CHECK(!a->Fire() && !a->fire_error.empty());
        record->model = model;
        CHECK(g.unload_world());
        return true;
    }
    bool damage(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g);
        auto* b = create(g, {500, 300, 500});
        CHECK(a && b && g.objects.advance(.002));
        const float hp = a->m_damage.m_fHitPoints;
        const auto requests = a->health_requests;
        auto m = g.server.StartMessageToObject(b, a->m_hObject, damage_message);
        CHECK(m && g.server.WriteDamage(m, {{0, 0, 1}, a->m_MoveObj.GetPos(), 100, DamageType::Normal, b->m_hObject}) ==
                       DE_OK);
        CHECK(g.server.EndMessage(m) == DE_OK && a->m_damage.m_fHitPoints < hp && a->health_requests == requests + 1);
        CHECK(std::abs(a->boss_health_fraction - a->m_damage.m_fHitPoints / a->m_fFullHealth) < 1e-6f);
        CHECK(a->m_damage.last_impulse_ok && a->RuntimeReady());
        CHECK(a->AI_STATE_Recoil() && a->m_nState == AI_Mgr::StateDodge);
        CHECK(g.unload_world());
        return true;
    }
    bool keys(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g);
        auto* b = create(g, {0, 300, 600});
        CHECK(a && b);
        g.server.SetObjectState(b->m_hObject, OBJSTATE_INACTIVE);
        b->m_damage.can_damage = false;
        a->m_bSetShutdown = true;
        g.server.SetObjectState(a->m_hObject, OBJSTATE_ACTIVE);
        CHECK(a->SetNewTarget(b->m_hObject) && a->RefreshTargetPosition());
        // High target selects the source vomit/MELEE clip without a random branch.
        a->m_vTargetPos.y += a->m_vDims.y * 2;
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar) && a->MC_Fire_Stand() && a->selected_weapon == 43);
        for (int i = 0; i < 240 && !a->fire_shots; ++i)
        {
            CHECK(g.objects.advance(1. / 60));
            CHECK(g.server.advance_models(1. / 60));
        }
        if (a->runtime_failed)
            std::cerr << a->runtime_error << "\n";
        CHECK(a->RuntimeReady() && a->fire_shots > 0 && a->model_keys > 0);
        std::cout << "UndeadGideon actual model-key shots=" << a->fire_shots << "\n";
        CHECK(g.unload_world());
        return true;
    }
    bool placement(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g);
        CHECK(a && a->RuntimeReady());
        auto* r = g.objects.get(a->m_hObject);
        auto before = r->state.m_Pos;
        ObjectCreateStruct state;
        state.m_Pos = before;
        state.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        CHECK(h);
        DVector dims{10, 10, 10};
        CHECK(g.server.SetObjectDims(h, &dims) == DE_OK);
        CHECK(g.server.ResolveInitialActorOverlap(a->m_hObject) == DE_OK && r->placement_nudges > 0);
        const auto moved = r->state.m_Pos;
        const auto nudges = r->placement_nudges;
        CHECK(vector_dot(vector_subtract(moved, before), vector_subtract(moved, before)) > 0);
        CHECK(g.server.ResolveInitialActorOverlap(a->m_hObject) == DE_OK && r->placement_nudges == nudges);
        // Cover every face candidate with a second solid body: refuse atomically.
        CHECK(g.server.TeleportObject(a->m_hObject, &before) == DE_OK);
        auto outer = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        dims = {1000, 1000, 1000};
        CHECK(outer && g.server.SetObjectDims(outer, &dims) == DE_OK);
        CHECK(g.server.ResolveInitialActorOverlap(a->m_hObject) == DE_ERROR);
        CHECK(r->state.m_Pos.x == before.x && r->state.m_Pos.y == before.y && r->state.m_Pos.z == before.z &&
              r->placement_nudges == nudges);
        CHECK(g.unload_world());
        return true;
    }
    bool world(const char* root)
    {
        for (int d : {0, 1, 2})
        {
            GameSession g;
            Config c;
            c.data_dir = root;
            std::string e;
            CHECK(g.initialize() && g.resources.mount_game(c, e));
            g.server.set_game_settings(d, 0);
            CHECK(g.request_world("24_ancientcity", e));
            if (!g.run_world(e))
            {
                std::cerr << e << '\n';
                return false;
            }
            unsigned count = 0;
            for (auto id : g.objects.snapshot())
                if (auto* a = dynamic_cast<UndeadGideon*>(g.objects.get(id)->game.get()))
                {
                    CHECK(a->RuntimeReady());
                    ++count;
                }
            CHECK(count == 1);
            for (int i = 0; i < 180; ++i)
                if (!g.step(1. / 60))
                {
                    for (const auto& problem : g.step_errors())
                        std::cerr << problem << "\n";
                    for (const auto& problem : g.objects.diagnostics())
                        std::cerr << problem << "\n";
                    return false;
                }
            CHECK(g.run_world(e));
            CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.string_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string_view s = argv[1];
    return (s == "profiles"    ? profiles(argv[2])
            : s == "decisions" ? decisions(argv[2])
            : s == "choices"   ? choices(argv[2])
            : s == "movement"  ? movement(argv[2])
            : s == "damage"    ? damage(argv[2])
            : s == "keys"      ? keys(argv[2])
            : s == "placement" ? placement(argv[2])
            : s == "fire"      ? fire(argv[2])
                               : world(argv[2]))
               ? 0
               : 1;
}
