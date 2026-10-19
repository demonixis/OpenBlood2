#include "engine/collision.h"
#include "engine/world_model.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/CampaignWeapons.h"
#include "game/original/Naga.h"
#include "game/original/Thief.h"
#include "game/original/WeaponPickups.h"
#include "game/original/ZealotAI.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
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
    bool mount(GameSession& g, const char* root)
    {
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        return true;
    }
    bool fixture(GameSession& g, const char* root)
    {
        CHECK(mount(g, root));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "synthetic_campaign_probe";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Pos", DVector{0, 100, 0}}}));
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        if (!g.enter_player(error) || !g.pump_player_messages(error))
        {
            std::cerr << error << '\n';
            for (const auto& e : g.objects.diagnostics())
                std::cerr << e << '\n';
            return false;
        }
        return true;
    }
    bool entry(const char* root, const char* world)
    {
        auto g = std::make_unique<GameSession>();
        CHECK(mount(*g, root));

        // Same transaction as the inspector, including replacement of an already running session.
        for (DBYTE difficulty : {1, 0, 2})
        {
            std::string error;
            if (!GameSession::start_new_game(g, world, difficulty, 0, error))
            {
                std::cerr << error << '\n';
                return false;
            }
            WorldValidation report;
            CHECK(check_world_initialization(*g, report, error));
            CHECK(report.unknown == 0 && report.failed == 0 && report.unavailable_services.empty());
            CHECK(report.omitted == (std::string_view(world) == "11_slaughter_house" ? 0
                                     : std::string_view(world) == "07_cathedral"     ? 2
                                                                                     : 1));
            CHECK(g->local_client && g->client_shell.m_bInWorld && g->startup.checked_textures > 0);
            unsigned corrected = 0, actors = 0;
            for (const auto& e : g->entries)
                if (const auto* r = g->objects.get(e.object))
                {
                    if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get()))
                    {
                        CHECK(ai->RuntimeReady());
                        actors += ai->npc_type == AI_Mgr::NPCType::Thief || ai->npc_type == AI_Mgr::NPCType::Naga ||
                                  ai->npc_type == AI_Mgr::NPCType::Zealot;
                        if (r->placement_nudges)
                        {
                            ++corrected;
                            CollisionHit hit;
                            CHECK(g->collision.sweep(r->state.m_Pos, r->state.m_Pos, r->dimensions, hit) !=
                                  CollisionQuery::Invalid);
                            CHECK(!hit.start_solid || hit.penetration < .01);
                            std::cout << world << " corrected " << r->state.m_Name << " by " << r->placement_nudge.x
                                      << ',' << r->placement_nudge.y << ',' << r->placement_nudge.z << '\n';
                        }
                    }
                }

            CHECK(actors > 0);
            // The first rendered frame can be too short for float positions to move.
            for (double seconds : {0., 1e-8, 1e-7, 1e-5, 1e-4, 1. / 144})
                if (!g->step(seconds))
                {
                    std::cerr << "first dt=" << seconds << '\n';
                    for (const auto& why : g->step_errors())
                        std::cerr << why << '\n';
                    for (const auto& why : g->objects.diagnostics())
                        std::cerr << why << '\n';
                    return false;
                }
            // Exercise real physics, callbacks, model keys and player input after entry.
            g->client_shell.movement_input.forward = true;
            for (int i = 0; i < 180; ++i)
            {
                if (i == 30)
                    g->client_shell.movement_input.forward = false;
                if (!g->step(1. / 60))
                {
                    for (const auto& e : g->step_errors())
                        std::cerr << e << '\n';
                    return false;
                }
            }
            for (auto id : g->objects.snapshot())
                if (const auto* r = g->objects.get(id); r && r->life == Life::Alive)
                    if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get());
                        ai && (ai->npc_type == AI_Mgr::NPCType::Thief || ai->npc_type == AI_Mgr::NPCType::Naga ||
                               ai->npc_type == AI_Mgr::NPCType::Zealot))
                        if (!ai->RuntimeReady() || !ai->state_error.empty())
                        {
                            std::cerr << r->state.m_Name << ": " << ai->runtime_error << " / " << ai->state_error
                                      << '\n';
                            return false;
                        }
            std::cout << world << ": difficulty=" << unsigned(difficulty) << " started and stepped, "
                      << "omitted=" << report.omitted << " corrected=" << corrected << '\n';
        }
        CHECK(g->unload_world() && g->objects.snapshot().empty() && !g->server.string_count() &&
              !g->server.message_count() && !g->server.list_count());
        return true;
    }
    bool actors(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto create = [&](const char* name, DVector pos)
        {
            return g.objects.create(g.objects.find_class(name), {}, Creation::World,
                                    {{"Pos", pos}, {"AIState", std::string("IDLE")}, {"RandomHitPoints", 0.f}});
        };
        auto nh = create("Naga", {1600, 400, 0});
        auto* n = nh ? dynamic_cast<Naga*>(g.server.HandleToObject(nh)) : nullptr;
        CHECK(n && n->RuntimeReady() && n->m_damage.m_fHitPoints >= 10000 && n->m_damage.m_fHitPoints <= 10001 &&
              n->spike_attachment);
        const float full_health = n->m_damage.m_fHitPoints;
        CHECK(n->m_damage.ai_body_hits && !(g.server.GetObjectFlags(nh) & FLAG_SOLID));
        CHECK(g.objects.advance(.01));
        CHECK(n->full_health == full_health);
        g.server.SetNextUpdate(nh, 0);
        CHECK(n->SetNewTarget(p.m_hObject) && n->RefreshTargetPosition() && n->m_MoveObj.Update());
        CHECK(n->SetNewState(AI_Mgr::StateSpecial1));
        for (unsigned i = 0; i < 66; ++i)
            CHECK(n->AI_STATE_Special(1));
        CHECK(g.objects.get(nh)->alpha == 1 && (g.server.GetObjectFlags(nh) & FLAG_SOLID));
        CHECK(n->SetNewState(AI_Mgr::StateAttackFar));
        n->m_fStimuli[0] = 0;
        CHECK(n->MC_Fire_Stand() && n->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_NAGA_DEBRIS);
        CHECK(n->Fire());
        auto* debris = dynamic_cast<CNagaDebrisProjectile*>(
            g.server.HandleToObject(n->m_InventoryMgr.CurrentWeapon()->projectile));
        CHECK(debris);
        CHECK(g.objects.engine_message(debris->m_hObject, MID_UPDATE, nullptr));
        CHECK(debris->children >= 3 && debris->children <= 5 && !debris->failed);
        CHECK(g.objects.finish_frame());
        CHECK(n->m_InventoryMgr.ChangeWeapon(WEAP_NAGA_SPIKE) == InventoryResult::Ok && n->Fire());
        CHECK(g.objects.get(n->m_InventoryMgr.CurrentWeapon()->projectile)->model);
        CHECK(n->MC_Extra("take_spike") && (g.server.GetObjectFlags(n->spike) & FLAG_VISIBLE));
        CHECK(n->SetNewState(AI_Mgr::StateIdle) && !(g.server.GetObjectFlags(n->spike) & FLAG_VISIBLE));

        auto zh = create("ZealotAI", {-1000, 200, 1000});
        auto* z = zh ? dynamic_cast<ZealotAI*>(g.server.HandleToObject(zh)) : nullptr;
        CHECK(z && z->RuntimeReady());
        g.server.SetNextUpdate(zh, 0);
        CHECK(z->SetNewTarget(p.m_hObject) && z->RefreshTargetPosition() && z->m_MoveObj.Update());
        CHECK(z->SetNewState(AI_Mgr::StateAttackFar) && z->MC_Fire_Stand() && z->MC_Fire_Stand());
        CHECK(z->staff && z->staff_attachment && z->Fire());
        auto* energy = z->m_InventoryMgr.CurrentWeapon();
        CHECK(energy->data.m_nType == WEAP_ZEALOT_ENERGYBLAST && energy->projectile);
        z->m_bAnimating = false;
        z->m_damage.SetHitPoints(40);
        CHECK(z->Heal());
        g.server.SetModelAnimation(zh, z->AnimSound()->m_nAnim_FIRE_STAND[6]);
        CHECK(g.server.advance_models(
            g.objects.get(zh)->model->animations[g.objects.get(zh)->animation.animation].duration / 1000.0 +
            .001)); // Advance the authored clip through FIRE_KEY and completion.
        CHECK(z->Heal() && z->m_damage.m_fHitPoints == 60);
        for (DBYTE type : {WEAP_ZEALOT_GROUNDFIRE, WEAP_ZEALOT_SHOCKWAVE})
        {
            CHECK(z->m_InventoryMgr.ChangeWeapon(type) == InventoryResult::Ok && z->Fire());
            auto* spell =
                dynamic_cast<CProjectile*>(g.server.HandleToObject(z->m_InventoryMgr.CurrentWeapon()->projectile));
            CHECK(spell && g.objects.engine_message(spell->m_hObject, MID_UPDATE, nullptr) && !spell->failed);
            CHECK(g.objects.finish_frame());
        }
        CHECK(z->SetNewState(AI_Mgr::StateTeleport));
        for (unsigned i = 0; i < 21; ++i)
        {
            CHECK(z->m_MoveObj.Update());
            if (!z->AI_STATE_Teleport())
            {
                std::cerr << "Zealot step " << i << " cmd " << z->Metacmd << " " << z->state_error << '\n';
                return false;
            }
        }
        CHECK(g.server.GetObjectFlags(zh) & FLAG_SOLID);
        CHECK(z->state_error.empty() && z->runtime_error.empty());

        auto th = create("Thief", {1000, 100, 1000});
        auto* t = th ? dynamic_cast<Thief*>(g.server.HandleToObject(th)) : nullptr;
        CHECK(t && t->RuntimeReady() && t->m_damage.ai_body_hits);
        CHECK(t->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_THIEF_SUCK);
        CHECK(t->RequestAttach(p.m_hObject) && t->AI_STATE_EnemyAttach());
        std::string error;
        CHECK(g.pump_player_messages(error) && t->proceed);
        CHECK(g.client_shell.creature == th && g.client_shell.creature_light == 1);
        CHECK(t->AI_STATE_EnemyAttach()); // Accepted handshake enters the periodic damage state.
        g.server.SetNextUpdate(th, 0);
        CHECK(g.objects.advance(1.01) && t->AI_STATE_EnemyAttach());
        CHECK(t->damage_ticks == 1 && p.m_damage.last_damage.amount >= 7 && p.m_damage.last_damage.amount <= 15);
        CHECK(t->Detach() && t->AI_STATE_EnemyAttach() && g.pump_player_messages(error));
        CHECK(!g.client_shell.creature && t->state_error.empty());
        auto dh = g.objects.create(g.objects.find_class("ZealotAI"), {}, Creation::World,
                                   {{"Pos", DVector{2600, 200, 1600}}, {"Divine", true}, {"RandomHitPoints", 0.f}});
        auto* divine = dh ? dynamic_cast<ZealotAI*>(g.server.HandleToObject(dh)) : nullptr;
        CHECK(divine && divine->RuntimeReady() && divine->npc_divine && divine->m_damage.m_fHitPoints >= 200);
        CHECK(divine->m_InventoryMgr.damage_multiplier == 1.5f);
        CHECK(divine->SetNewTarget(p.m_hObject) && divine->RefreshTargetPosition() && divine->m_MoveObj.Update());
        CHECK(divine->m_InventoryMgr.ChangeWeapon(WEAP_ZEALOT_ENERGYBLAST) == InventoryResult::Ok && divine->Fire());
        auto* divine_blast = dynamic_cast<CEnergyBlastProjectile*>(
            g.server.HandleToObject(divine->m_InventoryMgr.CurrentWeapon()->projectile));
        CHECK(divine_blast && divine_blast->divine);
        CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.message_count());
        return true;
    }
    bool weapons(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto& inv = p.m_InventoryMgr;
        std::string error;
        for (const auto* name : {"HowitzerPU", "NapalmCannonPU", "DeathRayPU", "TimeBombPU"})
        {
            auto h =
                g.objects.create(g.objects.find_class(name), {}, Creation::World, {{"Pos", DVector{200, 100, 200}}});
            CHECK(h && g.objects.get(h)->model && g.server.ValidateSpawnObject(name));
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && !g.server.IsObjectAlive(h));
        }
        p.m_damage.m_bGodMode = true;
        CHECK(g.objects.advance(.11));
        for (DBYTE type : {WEAP_HOWITZER, WEAP_NAPALMCANNON, WEAP_DEATHRAY})
        {
            CHECK(inv.ChangeWeapon(type) == InventoryResult::Ok);
            for (bool alternate : {false, true})
            {
                auto* gun = inv.CurrentWeapon();
                inv.SetAmmoCount(gun->GetAmmoType(alternate), 100);
                CHECK(g.pump_player_messages(error));
                WeaponFire shot{{0, 200, 0}, {0, 0, 1}, 17, type};
                shot.alternate = alternate;
                CHECK(p.HandleWeaponFireMessage(shot) == FireResult::Fired);
                if (type == WEAP_DEATHRAY && alternate)
                    continue; // Source secondary uses the shared vector path.
                auto h = gun->projectile;
                auto* projectile = dynamic_cast<CProjectile*>(g.server.HandleToObject(h));
                CHECK(projectile && projectile->configured && !projectile->failed);
                SegmentHit hit;
                hit.point = {0, 200, 200};
                hit.normal = {0, 0, -1};
                hit.polygon = 0;
                projectile->HandleContact(hit);
                CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr));
                CHECK(!projectile->failed && projectile->error.empty());
                CHECK(g.objects.finish_frame());
            }
        }
        CHECK(inv.GetAmmoCount(AMMO_TIMEBOMB) == 1 && p.OnCommandItem(9));
        CHECK(inv.CurrentWeapon()->data.m_nType == WEAP_TIMEBOMB);
        // Real client press edge selects the reserved slot, independent of numeric weapon slots.
        CHECK(inv.ChangeWeapon(WEAP_BERETTA) == InventoryResult::Ok);
        g.client_shell.time_bomb_down = true;
        CHECK(g.step(.001) && inv.CurrentWeapon()->data.m_nType == WEAP_TIMEBOMB);
        inv.SetAmmoCount(AMMO_TIMEBOMB, 2);
        WeaponFire shot{{2000, 200, 2000}, {0, 0, 1}, 17, WEAP_TIMEBOMB};
        shot.projectile_speed = 100;
        CHECK(p.HandleWeaponFireMessage(shot) == FireResult::Fired && inv.GetAmmoCount(AMMO_TIMEBOMB) == 1);
        auto h = inv.CurrentWeapon()->projectile;
        auto* bomb = dynamic_cast<CTimeBomb*>(g.server.HandleToObject(h));
        CHECK(bomb);
        SegmentHit hit;
        hit.point = {2000, 0, 2000};
        hit.normal = {0, 1, 0};
        hit.polygon = 0;
        bomb->HandleContact(hit);
        CHECK(bomb->armed && !bomb->sounds && !(g.server.GetObjectFlags(h) & FLAG_GRAVITY));
        for (unsigned i = 0; i < 50; ++i)
            CHECK(g.objects.advance(.1));
        CHECK(g.server.IsObjectAlive(h) && !bomb->exploded);
        CHECK(g.objects.advance(.01) && bomb->m_bExplode && !bomb->exploded);
        CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && bomb->exploded && !g.server.IsObjectAlive(h));
        CHECK(!bomb->failed && bomb->scans == 0 && bomb->sounds == 0);
        CHECK(g.objects.finish_frame());
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "actors"    ? actors(argv[2])
            : mode == "weapons" ? weapons(argv[2])
                                : entry(argv[2], argv[1]))
               ? 0
               : 1;
}
