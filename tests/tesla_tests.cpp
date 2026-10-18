// Original Tesla Cannon: weapon definition, alternate counter, primary/bolt/ball projectiles.
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/TeslaProjectile.h"
#include "game/original/Weapon.h"
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
    AI_Mgr* npc(GameSession& g, const char* type, DVector at, const char* weapon)
    {
        auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World,
                                  {{"Pos", at}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string(weapon)}});
        auto* a = h ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    bool fire(GameSession& g, AI_Mgr& shooter, DVector target, bool alt)
    {
        auto* weapon = shooter.m_InventoryMgr.CurrentWeapon();
        CHECK(weapon && weapon->data.m_nType == WEAP_TESLACANNON);
        DVector from;
        CHECK(g.server.GetObjectPos(shooter.m_hObject, &from) == DE_OK);
        from.y += 20;
        WeaponFire request;
        request.weapon = WEAP_TESLACANNON;
        request.position = from;
        auto d = vector_subtract(target, from);
        request.direction = vector_scale(d, 1 / std::sqrt(vector_dot(d, d)));
        request.alternate = alt;
        return weapon->Fire(request) == FireResult::Fired;
    }
    bool weapon(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        const auto* def = weapon_definition(WEAP_TESLACANNON);
        CHECK(def && def->m_nAmmoType == AMMO_BATTERY && def->m_nAmmoUse == 2 && def->m_nAltAmmoUse == 25 &&
              def->m_nDamageRadius == 100 && def->m_nAltDamageRadius == 150);
        // The authored 05_CDM Fanatic weapon name resolves to the cannon.
        auto* shooter = npc(g, "Fanatic", {0, 40, 0}, "TESLA");
        CHECK(shooter);
        auto* cannon = dynamic_cast<CTeslaCannon*>(shooter->m_InventoryMgr.CurrentWeapon());
        CHECK(cannon);
        shooter->m_InventoryMgr.SetAmmoCount(AMMO_BATTERY, 200);
        // 900 units away: the ball's attraction (100 -> 400) never reaches the shooter, which the
        // source does not exclude.
        auto* victim = npc(g, "CultistAI", {0, 40, 900}, "BERETTA");
        auto* bystander = npc(g, "CultistAI", {150, 40, 900}, "BERETTA");
        CHECK(bystander);
        CHECK(victim);
        DVector aim;
        CHECK(g.server.GetObjectPos(victim->m_hObject, &aim) == DE_OK);
        // Primary: a flying sprite that explodes on contact (EXP_TESLA_PRIMARY).
        CHECK(fire(g, *shooter, aim, false) && shooter->m_InventoryMgr.GetAmmoCount(AMMO_BATTERY) == 198);
        auto* primary = dynamic_cast<CTeslaProjectile*>(g.server.HandleToObject(cannon->projectile));
        CHECK(primary && primary->damage_type == DamageType::Explode && primary->m_nRadius == 100);
        CHECK(step(g, 1.0 / 60) && g.server.IsObjectAlive(cannon->projectile));
        CHECK(primary->flight_sound || !primary->sound_error.empty());
        const auto explosions = g.server.scene_explosions.size();
        for (int i = 0; i < 60 && g.server.IsObjectAlive(cannon->projectile); ++i)
            CHECK(step(g, 1.0 / 60));
        CHECK(!g.server.IsObjectAlive(cannon->projectile) && g.server.scene_explosions.size() == explosions + 1);
        CHECK(g.server.scene_explosions.back().type == 7);
        // Alternate: shots 1 and 3 are bolts, shot 2 is the ball.
        CHECK(fire(g, *shooter, aim, true) && cannon->numAltFires == 1 && cannon->lightning_unavailable == 1);
        CHECK(dynamic_cast<CTeslaBoltProjectile*>(g.server.HandleToObject(cannon->projectile)));
        CHECK(step(g, .01) && !g.server.IsObjectAlive(cannon->projectile));
        CHECK(fire(g, *shooter, aim, true) && cannon->numAltFires == 2);
        auto ball_handle = cannon->projectile;
        auto* ball = dynamic_cast<CTeslaBallProjectile*>(g.server.HandleToObject(ball_handle));
        CHECK(ball && ball->m_nRadius == 150);
        CHECK(step(g, .01) && ball->armed && g.server.scene_explosions.back().type == 8);
        DVector velocity;
        CHECK(g.server.GetVelocity(ball_handle, &velocity) == DE_OK && vector_dot(velocity, velocity) == 0);
        // Armed: arcs at nearby characters every 0.5 s for 2.5 s, then removed.
        std::uint64_t bolts = 0, lightning = 0;
        for (int i = 0; i < 40 && g.server.IsObjectAlive(ball_handle); ++i)
        {
            bolts = ball->bolts;
            lightning = ball->lightning_unavailable;
            CHECK(step(g, .1));
        }
        CHECK(!g.server.IsObjectAlive(ball_handle) && bolts >= 1 && lightning == bolts);
        CHECK(fire(g, *shooter, aim, true) && cannon->numAltFires == 3 &&
              dynamic_cast<CTeslaBoltProjectile*>(g.server.HandleToObject(cannon->projectile)));
        // Dry alternate fire still advances the source counter.
        shooter->m_InventoryMgr.SetAmmoCount(AMMO_BATTERY, 0);
        CHECK(!fire(g, *shooter, aim, true) && cannon->numAltFires == 4);
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
    return 2;
}
