#include "engine/collision.h"
#include "engine/world_model.h"
#include "game/original/AI_Mgr.h"
#include "game/original/PowerupPickups.h"
#include "game/original/WeaponPickups.h"
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
        g.world_path = "synthetic_airship_probe";
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
        const bool first = std::string_view(world) == "06_airship1";
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
            CHECK(report.omitted == (first ? 1 : 0)); // Airship1's already documented missing girder1 BSP.
            CHECK(g->local_client && g->client_shell.m_bInWorld && g->startup.checked_textures > 0);
            unsigned corrected = 0, snipers = 0, anger = 0;
            for (const auto& e : g->entries)
                if (const auto* r = g->objects.get(e.object))
                {
                    if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get()))
                    {
                        CHECK(ai->RuntimeReady());
                        for (unsigned slot = 0; slot < inventory_weapon_slots; ++slot)
                            if (const auto* w = ai->m_InventoryMgr.GetWeapon(slot))
                                snipers += w->data.m_nType == WEAP_SNIPERRIFLE;
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
                    anger += dynamic_cast<const AngerPU*>(r->game.get()) != nullptr;
                }
            CHECK(corrected >= 3 && (first ? snipers == 2 : anger == 1));
            // The first rendered frame can be too short for float positions to move.
            for (double seconds : {0., 1e-8, 1e-7, 1e-5, 1e-4, 1. / 144})
                CHECK(g->step(seconds));
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
            std::cout << world << ": difficulty=" << unsigned(difficulty) << " started and stepped, "
                      << "omitted=" << report.omitted << " corrected=" << corrected << '\n';
        }
        CHECK(g->unload_world() && g->objects.snapshot().empty() && !g->server.string_count() &&
              !g->server.message_count() && !g->server.list_count());
        return true;
    }
    bool weapons(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto& inv = p.m_InventoryMgr;
        std::string error;
        auto h = g.objects.create(g.objects.find_class("SniperRiflePU"), {}, Creation::World,
                                  {{"Pos", DVector{200, 100, 200}}});
        CHECK(h && g.objects.get(h)->model && g.server.ValidateSpawnObject("SniperRiflePU"));
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject));
        CHECK(!g.server.IsObjectAlive(h) && inv.GetAmmoCount(AMMO_BMG) == 10);
        unsigned slot = 0;
        while (slot < 10 && (!inv.GetWeapon(slot) || inv.GetWeapon(slot)->data.m_nType != WEAP_SNIPERRIFLE))
            ++slot;
        CHECK(slot < 10 && g.command_weapon(DBYTE(slot), error));
        auto* gun = inv.CurrentWeapon();
        CHECK(dynamic_cast<CSniperRifle*>(gun) && gun->data.m_bAltFireZoom && gun->data.m_fRange == 10000);
        auto target = g.objects.create(g.objects.find_class("Fanatic"), {}, Creation::World,
                                       {{"Name", std::string("target")},
                                        {"Pos", DVector{0, 100, 500}},
                                        {"AIWeapon1", std::string("SNIPE")},
                                        {"AIWeapon2", std::string("NONE")},
                                        {"AIWeapon3", std::string("NONE")}});
        auto* ai = target ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(target)) : nullptr;
        CHECK(ai && ai->RuntimeReady() && dynamic_cast<CSniperRifle*>(ai->m_InventoryMgr.CurrentWeapon()));
        ai->m_damage.SetHitPoints(10000);
        ai->m_damage.m_fMaxHitPoints = 10000;
        DVector from, to;
        CHECK(g.server.GetObjectPos(p.m_hObject, &from) == DE_OK && g.server.GetObjectPos(target, &to) == DE_OK);
        const auto delta = vector_subtract(to, from);
        WeaponFire shot{from, vector_scale(delta, 1 / std::sqrt(vector_dot(delta, delta))), 17, WEAP_SNIPERRIFLE};
        const auto before = ai->m_damage.accepted_damage;
        CHECK(p.HandleWeaponFireMessage(shot) == FireResult::Fired && gun->last_pellets == 1);
        CHECK(gun->last_shot.damage >= 100 && gun->last_shot.damage <= 125 && inv.GetAmmoCount(AMMO_BMG) == 9);
        CHECK(ai->m_damage.accepted_damage > before && gun->sounds_played == 1);
        const auto base_loss = gun->last_shot.damage;
        CHECK(p.AddPowerup(7, 30));
        CHECK(p.HandleWeaponFireMessage(shot) == FireResult::Fired && inv.GetAmmoCount(AMMO_BMG) == 8);
        CHECK(ai->m_damage.last_damage.amount == base_loss * 3);
        p.ResetPowerups();
        shot.alternate = true;
        CHECK(p.HandleWeaponFireMessage(shot) == FireResult::Blocked && inv.GetAmmoCount(AMMO_BMG) == 8);
        auto& c = g.client_shell;
        const auto shots = gun->shots;
        c.weapon_input.alternate = true;
        CHECK(c.UpdateWeapons(g.server_shell, .01, error) && g.pump_player_messages(error));
        CHECK(p.m_bZoomView && c.m_bZoomView && c.m_nCrosshair == 99 && p.zoom_sound_error.empty());
        for (int i = 0; i < 20; ++i)
            CHECK(c.UpdateWeapons(g.server_shell, .01, error));
        CHECK(p.m_bZoomView && gun->shots == shots && inv.GetAmmoCount(AMMO_BMG) == 8);
        c.weapon_input.alternate = false;
        CHECK(c.UpdateWeapons(g.server_shell, .01, error));
        c.weapon_input.alternate = true;
        CHECK(c.UpdateWeapons(g.server_shell, .01, error) && !p.m_bZoomView);
        CHECK(inv.AddItem(5, 1) == InventoryResult::Ok && p.OnCommandItem(5));
        CHECK(p.m_bZoomView && !p.OnCommandAltFire() && p.m_bZoomView); // Active binoculars own zoom.
        CHECK(p.OnCommandItem(5) && !p.m_bZoomView);
        p.camera_movement_blocked = true;
        CHECK(!p.OnCommandAltFire() && !p.m_bZoomView);
        p.camera_movement_blocked = false;
        // Source rifle aiming and animation can execute on a Fanatic, not just instantiate.
        ai->m_InventoryMgr.SetAmmoCount(AMMO_BMG, 3);
        CHECK(ai->SetNewTarget(p.m_hObject) && ai->m_MoveObj.Update());
        CHECK(ai->Fire(false) && ai->fire_shots == 1 && ai->m_InventoryMgr.GetAmmoCount(AMMO_BMG) == 2);
        CHECK(g.unload_world());
        return true;
    }
    bool revenant(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto h = g.objects.create(g.objects.find_class("RevenantPU"), {}, Creation::World,
                                  {{"Pos", DVector{200, 100, 200}}});
        auto* item = h ? dynamic_cast<RevenantPU*>(g.server.HandleToObject(h)) : nullptr;
        CHECK(item && item->value.type == 8 && item->value.amount == 30 && item->resource_id == 316);
        CHECK(g.objects.get(h)->model && item->rotate_fx);
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject));
        CHECK(g.server.IsObjectAlive(h) && !item->picked_up && item->attempts == 1);
        CHECK(p.powerup_pickups == 0 && !p.IsPowerupActive(PU_INCORPOREAL));
        CHECK(g.unload_world());
        return true;
    }
    bool anger(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto& inv = p.m_InventoryMgr;
        inv.damage_multiplier = .5f;
        auto h =
            g.objects.create(g.objects.find_class("AngerPU"), {}, Creation::World, {{"Pos", DVector{200, 100, 200}}});
        auto* item = h ? dynamic_cast<AngerPU*>(g.server.HandleToObject(h)) : nullptr;
        CHECK(item && item->value.type == 7 && item->value.amount == 30 && item->resource_id == 315);
        CHECK(g.objects.get(h)->model && g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject));
        CHECK(!g.server.IsObjectAlive(h) && p.IsPowerupActive(PU_TRIPLEDAMAGE) && inv.damage_multiplier == 1.5f);
        CHECK(p.m_fTripleDamageTime == g.server.GetTime() + 30 &&
              (g.server.GetObjectUserFlags(p.m_hObject) & (1u << 8)));
        CHECK(p.AddPowerup(7, 10) && inv.damage_multiplier == 1.5f); // Refresh, never multiply twice.
        p.m_fTripleDamageTime = g.server.GetTime();
        CHECK(p.CheckPowerups() && p.IsPowerupActive(PU_TRIPLEDAMAGE)); // Strict expiry.
        CHECK(g.objects.advance(.01) && p.CheckPowerups());
        CHECK(!p.IsPowerupActive(PU_TRIPLEDAMAGE) && inv.damage_multiplier == .5f);
        CHECK(!(g.server.GetObjectUserFlags(p.m_hObject) & (1u << 8)) && p.powerup_expiries == 1);
        // Preserve the original shared-time quirk: Stealth blinking overwrites fTime
        // before Anger's expiry check. A later non-blinking update ends it.
        CHECK(p.AddPowerup(6, 4) && p.AddPowerup(7, 1));
        CHECK(g.objects.advance(2.1) && p.CheckPowerups() && p.IsPowerupActive(PU_TRIPLEDAMAGE));
        CHECK(g.objects.advance(2.1) && p.CheckPowerups() && !p.IsPowerupActive(PU_TRIPLEDAMAGE));
        CHECK(p.AddPowerup(7, 30));
        PlayerRestoreState saved;
        std::string error;
        // Active Anger travels with its relative expiry and the tripled multiplier (save/load lot 5).
        CHECK(g.capture_player(saved, error) && saved.player.powers[PU_TRIPLEDAMAGE] &&
              saved.inventory.damage_multiplier == 1.5f && saved.player.power_offsets[PU_TRIPLEDAMAGE] > 29.9f);
        p.ResetPowerups();
        CHECK(inv.damage_multiplier == .5f && !p.IsPowerupActive(PU_TRIPLEDAMAGE));
        CHECK(g.capture_player(saved, error) && saved.inventory.damage_multiplier == .5f);
        CHECK(g.leave_player() && g.restore_player(saved, error));
        auto* restored = g.server_shell.Player(g.local_client);
        CHECK(restored && !restored->IsPowerupActive(PU_TRIPLEDAMAGE) &&
              restored->m_InventoryMgr.damage_multiplier == .5f);
        CHECK(restored->AddPowerup(7, 30) && restored->m_InventoryMgr.damage_multiplier == 1.5f);
        restored->m_damage.last_damage.attacker = restored->m_hObject;
        restored->m_damage.reaction_damage_type = DamageType::FreeFall;
        restored->m_damage.m_bDead = true;
        restored->m_damage.m_fHitPoints = 0;
        restored->HandleDeath();
        CHECK(restored->m_bDead && !restored->IsPowerupActive(PU_TRIPLEDAMAGE) &&
              restored->m_InventoryMgr.damage_multiplier == .5f);
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    if (std::string_view(argv[1]) == "weapons")
        return weapons(argv[2]) ? 0 : 1;
    if (std::string_view(argv[1]) == "anger")
        return anger(argv[2]) ? 0 : 1;
    if (std::string_view(argv[1]) == "revenant")
        return revenant(argv[2]) ? 0 : 1;
    return entry(argv[2], argv[1]) ? 0 : 1;
}
