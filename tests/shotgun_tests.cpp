// Original sawed-off shotgun: sixteen-vector FireSpread, shell costs, pickup and NPC use.
#include "camera_fixture.h"
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Weapon.h"
#include "game/original/WeaponPickups.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << "\n";                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool setup(GameSession& g, const char* root)
    {
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    bool step(GameSession& g, double dt)
    {
        CHECK(g.server.advance_models(dt) && g.server.advance_physics(dt) && g.objects.advance(dt) &&
              g.server.advance_projectiles(dt));
        return true;
    }
    AI_Mgr* npc(GameSession& g, const char* type, DVector at, const char* weapon, const char* name = "npc")
    {
        Properties p{{"Name", std::string(name)},
                     {"Pos", at},
                     {"AIState", std::string("IDLE")},
                     {"AIWeapon1", std::string(weapon)}};
        if (std::string_view(type) == "Fanatic") // Keep only the authored weapon (no rifle to switch to).
            p["AIWeapon2"] = p["AIWeapon3"] = std::string("NONE");
        auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World, p);
        auto* a = h ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    FireResult fire(GameSession& g, AI_Mgr& shooter, DVector target, bool alt, DBYTE seed = 7)
    {
        auto* weapon = shooter.m_InventoryMgr.CurrentWeapon();
        DVector from;
        g.server.GetObjectPos(shooter.m_hObject, &from);
        from.y += 20;
        WeaponFire request;
        request.weapon = WEAP_SHOTGUN;
        request.position = from;
        auto d = vector_subtract(target, from);
        request.direction = vector_scale(d, 1 / std::sqrt(vector_dot(d, d)));
        request.alternate = alt;
        request.seed = seed;
        return weapon ? weapon->Fire(request) : FireResult::Invalid;
    }
    bool weapon(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        const auto* def = weapon_definition(WEAP_SHOTGUN);
        CHECK(def && def->m_nAmmoType == AMMO_SHELL && def->m_nAmmoUse == 1 && def->m_nAltAmmoUse == 2 &&
              def->m_dwShotsPerFire == 16 && def->m_dwAltShotsPerFire == 16 && def->m_Spread.h == 200 &&
              def->m_Spread.v == 100 && !def->m_szProjectileClass);
        // The authored 03_Museum Fanatic weapon name resolves to the shotgun.
        auto* shooter = npc(g, "Fanatic", {0, 40, 0}, "SHOTGUN", "shooter");
        CHECK(shooter && dynamic_cast<CShotgun*>(shooter->m_InventoryMgr.CurrentWeapon()));
        auto* gun = shooter->m_InventoryMgr.CurrentWeapon();
        auto* victim = npc(g, "CultistAI", {0, 40, 300}, "BERETTA", "victim");
        CHECK(victim);
        victim->m_damage.SetHitPoints(1000);
        victim->m_damage.m_fMaxHitPoints = 1000;
        DVector aim;
        CHECK(g.server.GetObjectPos(victim->m_hObject, &aim) == DE_OK);
        shooter->m_InventoryMgr.SetAmmoCount(AMMO_SHELL, 3);
        // Primary: one shell for sixteen vectors sharing one 4..6 damage draw.
        float before = victim->m_damage.m_fHitPoints;
        CHECK(fire(g, *shooter, aim, false) == FireResult::Fired && gun->last_pellets == 16);
        CHECK(shooter->m_InventoryMgr.GetAmmoCount(AMMO_SHELL) == 2);
        CHECK(g.server.shotgun_effects.casings.empty());
        const float pellet = gun->last_shot.damage;
        CHECK(pellet >= 4 && pellet <= 6);
        const float primary_loss = before - victim->m_damage.m_fHitPoints;
        std::cout << "primary pellet=" << pellet << " loss=" << primary_loss << " impacts=" << gun->impacts.size()
                  << "\n";
        CHECK(primary_loss > pellet * 1.5f); // Several pellets hit at 300 units.
        // The same seed spreads the same vectors.
        const auto direction = gun->last_shot.direction;
        CHECK(fire(g, *shooter, aim, false) == FireResult::Fired && gun->last_shot.direction.x == direction.x &&
              gun->last_shot.direction.y == direction.y && gun->last_shot.direction.z == direction.z);
        // Alternate: two shells, 8..12 per vector; with one shell left it is dry.
        CHECK(shooter->m_InventoryMgr.GetAmmoCount(AMMO_SHELL) == 1);
        CHECK(fire(g, *shooter, aim, true) == FireResult::Dry && shooter->m_InventoryMgr.GetAmmoCount(AMMO_SHELL) == 1);
        shooter->m_InventoryMgr.SetAmmoCount(AMMO_SHELL, 2);
        before = victim->m_damage.m_fHitPoints;
        CHECK(fire(g, *shooter, aim, true) == FireResult::Fired && gun->last_pellets == 16 &&
              shooter->m_InventoryMgr.GetAmmoCount(AMMO_SHELL) == 0);
        CHECK(gun->last_shot.damage >= 8 && gun->last_shot.damage <= 12 &&
              before - victim->m_damage.m_fHitPoints > gun->last_shot.damage * 1.5f);
        CHECK(fire(g, *shooter, aim, false) == FireResult::Dry);
        CHECK(g.server.shotgun_effects.casings.size() == 2); // once per discharge, not per pellet.
        const auto events = g.server.shotgun_effects.started;
        shooter->m_InventoryMgr.SetAmmoCount(AMMO_SHELL, 2);
        CHECK(fire(g, *shooter, {0, 10000, 0}, true) == FireResult::Fired && gun->impacts.empty());
        CHECK(g.server.shotgun_effects.started == events); // complete miss: source sends no composite FX.
        CHECK(g.unload_world());
        return true;
    }
    bool pickup(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = g.objects.create(g.objects.find_class("ShotgunPU"), {}, Creation::World, {{"Pos", DVector{0, 40, 0}}});
        auto* r = g.objects.get(h);
        CHECK(r && r->model && r->model_error.empty());
        auto* item = dynamic_cast<ShotgunPU*>(r->game.get());
        CHECK(item && item->value.type == WEAP_SHOTGUN && item->value.amount == 8 && item->resource_id == 351);
        CHECK(std::string(WeaponPickupClass(WEAP_SHOTGUN)) == "ShotgunPU");
        CHECK(g.server.ValidateSpawnObject("ShotgunPU"));
        CHECK(g.unload_world());
        return true;
    }
    bool ai(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* shooter = npc(g, "Fanatic", {0, 40, 0}, "SHOTGUN", "shooter");
        CHECK(shooter);
        auto* victim = npc(g, "CultistAI", {0, 40, 400}, "BERETTA", "victim");
        CHECK(victim);
        victim->m_damage.SetHitPoints(1000);
        victim->m_damage.m_fMaxHitPoints = 1000;
        const float before = victim->m_damage.m_fHitPoints;
        CHECK(shooter->SetTarget("victim") && shooter->SetNewState(AI_Mgr::StateAttackFar));
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 60, -200};
        auto observer = g.objects.create(g.objects.find_class("BaseClass"), eye);
        CHECK(observer);
        auto* gun = shooter->m_InventoryMgr.CurrentWeapon();
        for (int i = 0; i < 900 && !gun->shots && !shooter->runtime_failed; ++i)
        {
            if (i % 30 == 0)
                CHECK(g.server.PingObjects(observer));
            CHECK(step(g, 1.0 / 60));
        }
        if (shooter->runtime_failed)
            std::cerr << shooter->runtime_error << "\n";
        CHECK(!shooter->runtime_failed && gun->shots >= 1 && gun->last_pellets == 16);
        std::cout << "ai shots=" << gun->shots << " loss=" << before - victim->m_damage.m_fHitPoints << "\n";
        CHECK(g.unload_world());
        return true;
    }
    bool player(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(cfg, error));
        CHECK(g.request_world("01_subwaya", error) && g.enter_player(error) && test_camera::complete_subway_intro(g));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        auto& inv = p->m_InventoryMgr;
        CHECK(inv.ObtainWeapon(WEAP_SHOTGUN, 3) == InventoryResult::Ok);
        inv.SetAmmoCount(AMMO_SHELL, 10);
        CHECK(g.select_weapon(3, error) && g.client_shell.weapons[0].type == WEAP_SHOTGUN &&
              g.client_shell.weapons[0].model);
        g.client_shell.weapon_input.primary = true;
        auto* gun = inv.CurrentWeapon();
        for (int i = 0; i < 300 && !gun->shots; ++i)
            CHECK(g.step(1.0 / 60));
        g.client_shell.weapon_input.primary = false;
        CHECK(gun->shots >= 1 && gun->last_pellets == 16 && inv.GetAmmoCount(AMMO_SHELL) == 10 - gun->shots);
        std::cout << "player shots=" << gun->shots << " impacts=" << gun->impacts.size() << "\n";
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "weapon")
        return weapon(argv[2]) ? 0 : 1;
    if (mode == "pickup")
        return pickup(argv[2]) ? 0 : 1;
    if (mode == "player")
        return player(argv[2]) ? 0 : 1;
    if (mode == "ai")
        return ai(argv[2]) ? 0 : 1;
    return 2;
}
