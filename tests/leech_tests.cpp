#include "app/sprite_scene.h"
#include "game/original/LeechProjectile.h"
#include "game/original/WeaponPickups.h"
#include "player_fixture.h"
#include <cmath>
#include <numbers>
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .002f;
    }
    bool setup(GameSession& g, const char* data, const char* fixture)
    {
        CHECK(test_player::setup(g, fixture));
        std::string error;
        CHECK(g.enter_player(error));
        Config config;
        config.data_dir = data;
        CHECK(g.resources.mount_game(config, error));
        return true;
    }
    CBaseCharacter* target(GameSession& g, DVector at)
    {
        ObjectCreateStruct state;
        state.m_Pos = at;
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        auto* result = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
        if (!result || !result->m_damage.Init(h))
            return nullptr;
        result->m_damage.SetHitPoints(1000);
        result->m_damage.m_fMaxHitPoints = 1000;
        result->m_damage.SetArmorPoints(0);
        result->m_damage.apply_damage_physics = false;
        g.objects.get(h)->dimensions = {2, 2, 2};
        return result;
    }
    bool damage(GameSession& g, HOBJECT attacker, CBaseCharacter& victim, float amount)
    {
        auto message =
            g.server.StartMessageToObject(g.server.HandleToObject(attacker), victim.m_hObject, damage_message);
        CHECK(message);
        CHECK(g.server.WriteDamage(message, {{0, 0, 1}, {}, amount, DamageType::Leech, attacker}) == DE_OK);
        CHECK(g.server.EndMessage(message) == DE_OK);
        return true;
    }
    bool healing(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* player = g.server_shell.Player(g.local_client);
        auto* victim = target(g, {100, 40, 0});
        CHECK(victim);
        player->m_damage.SetHitPoints(50);
        player->m_damage.m_fMaxHitPoints = 100;
        victim->m_damage.SetArmorPoints(20); // 30% absorption: 14 health damage.
        CHECK(damage(g, player->m_hObject, *victim, 20));
        CHECK(near(victim->m_damage.m_fHitPoints, 986) && near(player->m_damage.m_fHitPoints, 51.4f));
        DestructableState saved;
        std::string error;
        CHECK(victim->m_damage.Save(saved, error) && saved.last_type == DamageType::Leech);
        CHECK(victim->m_damage.Load(saved, error));
        // Source CPlayerObj::IsDead is its player flag, not the aggregate's pending death flag.
        player->m_damage.m_bDead = true;
        CHECK(damage(g, player->m_hObject, *victim, 20));
        CHECK(near(player->m_damage.m_fHitPoints, 52.8f) && !player->m_damage.m_bDead);
        player->m_damage.SetHitPoints(51.4f);
        victim->m_damage.m_bGodMode = true;
        CHECK(damage(g, player->m_hObject, *victim, 20));
        CHECK(near(player->m_damage.m_fHitPoints, 51.4f));
        victim->m_damage.m_bGodMode = false;
        player->m_bDead = true;
        CHECK(damage(g, player->m_hObject, *victim, 20));
        CHECK(near(player->m_damage.m_fHitPoints, 51.4f));
        player->m_bDead = false;
        player->m_damage.SetHitPoints(99.9f);
        victim->m_damage.SetArmorPoints(0);
        CHECK(damage(g, player->m_hObject, *victim, 20));
        CHECK(near(player->m_damage.m_fHitPoints, 100));
        // Dead victim is ignored, including health return.
        victim->m_damage.m_bDead = true;
        player->m_damage.SetHitPoints(50);
        CHECK(damage(g, player->m_hObject, *victim, 20));
        CHECK(near(player->m_damage.m_fHitPoints, 50));
        return true;
    }
    bool projectiles(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* player = g.server_shell.Player(g.local_client);
        auto& inventory = player->m_InventoryMgr;
        CHECK(inventory.ObtainWeapon(WEAP_LIFELEECH, 2) == InventoryResult::Ok);
        auto* weapon = dynamic_cast<CLifeLeech*>(inventory.GetWeapon(2));
        CHECK(weapon && weapon->data.m_nAmmoUse == 2 && weapon->data.m_nAltAmmoUse == 100);
        CHECK(weapon->data.m_fProjVelocity == 1000 && weapon->data.m_nDamageRadius == 100);
        CHECK(std::string(WeaponPickupClass(16)) == "LifeLeechPU");
        inventory.SetAmmoCount(AMMO_FOCUS, 500);
        WeaponFire shot{{0, 200, 0}, {0, 0, 1}, 42, 16, false, false};
        CHECK(weapon->Fire(shot) == FireResult::Fired);
        CHECK(inventory.GetAmmoCount(AMMO_FOCUS) == 498);
        auto h = weapon->projectile;
        auto* prime = dynamic_cast<CLeechPrimeProjectile*>(g.server.HandleToObject(h));
        CHECK(prime && prime->trail && prime->trail_error.empty() && prime->damage_type == DamageType::Leech);
        CHECK(std::abs(prime->curve) >= std::numbers::pi_v<float> * .05f && prime->amplitude >= 5 &&
              prime->amplitude <= 20);
        CHECK(g.objects.get(h)->state.m_Scale.x == 0);
        CHECK(g.objects.advance(.005));
        if (!g.server.projectile_reports.empty())
            std::cerr << g.server.projectile_reports.back().error << "\n";
        CHECK(g.server.IsObjectAlive(h));
        CHECK(prime->flight_sound && g.objects.get(h)->state.m_Scale.x == 0);
        CHECK(g.server.advance_projectiles(.005));
        DVector velocity;
        CHECK(g.server.GetVelocity(h, &velocity) == DE_OK);
        const auto bend = vector_subtract(velocity, vector_scale(prime->m_vDir, prime->m_fVelocity));
        CHECK(near(std::sqrt(vector_dot(bend, bend)), prime->amplitude));
        CHECK(g.objects.advance(.006) && near(g.objects.get(h)->state.m_Scale.x, .2f));
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server));
        CHECK(g.server.advance_projectiles(.03));
        CHECK(g.objects.advance(.03));
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server));
        CHECK(g.server.object_particles.particle_count() > 0);
        const auto& trail = g.server.object_particles.active.front().effect;
        CHECK(trail.recipe == 3 && near(trail.min_life, .25f * prime->trail_scale));
        for (const auto& p : trail.particles)
            CHECK(p.color.x == 1 && p.color.z == 0);
        // Source override ignores base lifetime.
        prime->m_fLifeTime = 0;
        CHECK(g.objects.advance(.002) && g.server.IsObjectAlive(h));
        auto* victim = target(g, g.objects.get(h)->state.m_Pos);
        CHECK(victim);
        prime->m_bExplode = true;
        CHECK(g.objects.advance(.002) && !g.server.IsObjectAlive(h));
        CHECK(victim->m_damage.m_fHitPoints < 1000 && victim->m_damage.last_damage.type == DamageType::Leech);
        const auto& effect = g.server.scene_explosions.back();
        CHECK(effect.type == 16 && effect.sprite_event && effect.light_event && effect.sprite_error.empty() &&
              effect.light_error.empty());
        CHECK(g.server.explosion_sprites.active.size() == 2);
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server) &&
              g.server.object_particles.active.empty());
        // Empty focus rejects a real shot before creating another projectile.
        inventory.SetAmmoCount(AMMO_FOCUS, 1);
        CHECK(weapon->Fire(shot) == FireResult::Dry);
        // Live owner requirement, malformed recipe scaling and missing trail assets remain failures.
        std::string error;
        CHECK(!g.server.PublishObjectParticles(player->m_hObject, 3, error, 0) && !error.empty());
        CHECK(!g.server.PublishObjectParticles(player->m_hObject, 19, error, 2) && !error.empty());
        CHECK(g.unload_world());
        return true;
    }
    bool client(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* player = g.server_shell.Player(g.local_client);
        auto& inventory = player->m_InventoryMgr;
        CHECK(inventory.ObtainWeapon(16, 2) == InventoryResult::Ok);
        std::string error;
        CHECK(g.select_weapon(2, error));
        auto& view = g.client_shell.weapons[0];
        CHECK(view.type == 16 && view.model);
        auto tick = [&](WeaponInput input)
        {
            return g.objects.advance(1. / 60) &&
                   view.Update(g.server, g.server_shell, g.local_client, input, {0, 200, 0}, {0, 0, 1}, 1. / 60, error);
        };
        for (bool alt : {false, true})
        {
            for (unsigned i = 0; i < 120; ++i)
                CHECK(tick({}));
            inventory.SetAmmoCount(AMMO_FOCUS, 500);
            const auto before = view.shots;
            for (unsigned i = 0; i < 180; ++i)
                CHECK(tick({!alt, alt, false}));
            if (!error.empty())
                std::cerr << error << "\n";
            CHECK(view.shots > before && view.fire_keys > 0);
            CHECK(inventory.GetAmmoCount(AMMO_FOCUS) == 500 - float(view.shots - before) * (alt ? 100.f : 2.f));
        }
        CHECK(g.unload_world());
        return true;
    }
    bool alternate(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* player = g.server_shell.Player(g.local_client);
        DVector center{0, 100, 0};
        CHECK(g.server.TeleportObject(player->m_hObject, &center) == DE_OK);
        auto* victim = target(g, {100, 100, 0});
        auto* far = target(g, {451, 100, 0});
        auto* zero = target(g, center);
        CHECK(victim && far && zero);
        for (auto* body : {victim, far, zero})
        {
            // Original source acts on OT_MODEL objects, not the normal fixture type.
            g.objects.get(body->m_hObject)->state.m_ObjectType = OT_MODEL;
            g.server.SetObjectUserFlags(body->m_hObject, 1u << 5);
        }
        auto& inventory = player->m_InventoryMgr;
        CHECK(inventory.ObtainWeapon(16, 2) == InventoryResult::Ok);
        auto* weapon = inventory.GetWeapon(2);
        inventory.SetAmmoCount(AMMO_FOCUS, 500);
        CHECK(weapon->Fire({center, {0, 0, 1}, 42, 16, true, false}) == FireResult::Fired);
        auto h = weapon->projectile;
        CHECK(dynamic_cast<CLeechAltProjectile*>(g.server.HandleToObject(h)));
        CHECK(g.objects.advance(.002) && !g.server.IsObjectAlive(h));
        if (!g.server.projectile_reports.empty())
            std::cerr << g.server.projectile_reports.back().error << "\n";
        CHECK(inventory.GetAmmoCount(AMMO_FOCUS) == 400);
        CHECK(near(victim->m_damage.m_fHitPoints, 1000 - weapon->last_shot.damage));
        CHECK(victim->m_damage.last_damage.attacker == player->m_hObject &&
              victim->m_damage.last_damage.type == DamageType::Explode);
        CHECK(far->m_damage.m_fHitPoints == 1000 && zero->m_damage.m_fHitPoints < 1000);
        DVector gravity, velocity;
        CHECK(g.server.GetGlobalForce(&gravity) == DE_OK &&
              g.server.GetVelocity(victim->m_hObject, &velocity) == DE_OK);
        CHECK(near(velocity.x, 2625) && near(velocity.y, -gravity.y * .25f));
        CHECK(g.server.GetVelocity(zero->m_hObject, &velocity) == DE_OK && physics_vector(velocity));
        const auto& effect = g.server.scene_explosions.back();
        CHECK(effect.type == 17 && effect.model_error.empty() && effect.sprite_error.empty() &&
              effect.ring_error.empty() && effect.light_error.empty());
        CHECK(effect.model_event && effect.ring_event && effect.sprite_event && effect.light_event);
        CHECK(g.server.explosion_sprites.active.back().settings.align == 2);
        CHECK(g.server.explosion_rings.active.back().particles.size() == 32);
        CHECK(g.unload_world() && g.server.explosion_sprites.active.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    const std::string_view mode(argv[1]);
    return (mode == "client"      ? client(argv[2], argv[3])
            : mode == "healing"   ? healing(argv[2], argv[3])
            : mode == "alternate" ? alternate(argv[2], argv[3])
                                  : projectiles(argv[2], argv[3]))
               ? 0
               : 1;
}
