// Original Singularity generator: blue beam, zero-velocity projectile that arms where its ray
// stops, then pulls, damages and swallows attractable models for five seconds.
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/SingularityProjectile.h"
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
        WorldModel world;
        // Floor and a back wall at z = 800 for the beam to stop on.
        world.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000},  {-4000, 0, 4000},
                        {-4000, 0, 800},   {4000, 0, 800},   {4000, 800, 800}, {-4000, 800, 800}};
        world.surfaces.resize(1);
        world.surfaces[0].flags = 1;
        world.polygons = {{0, {0, 1, 2, 3}}, {0, {4, 7, 6, 5}}};
        CHECK(g.collision.build(world, error));
        g.server.collision_ready = true;
        return true;
    }
    bool step(GameSession& g, double dt)
    {
        CHECK(g.server.advance_models(dt) && g.server.advance_physics(dt) && g.objects.advance(dt) &&
              g.server.advance_projectiles(dt));
        return true;
    }
    AI_Mgr* npc(GameSession& g, const char* type, DVector at, const char* weapon, const char* name)
    {
        Properties p{{"Name", std::string(name)},
                     {"Pos", at},
                     {"AIState", std::string("IDLE")},
                     {"AIWeapon1", std::string(weapon)}};
        if (std::string_view(type) == "Prophet")
            p["AIWeapon2"] = p["AIWeapon3"] = p["AIWeapon4"] = p["AIWeapon5"] = std::string("NONE");
        auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World, p);
        auto* a = h ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    bool weapon(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        const auto* def = weapon_definition(WEAP_SINGULARITY);
        CHECK(def && def->m_nAmmoType == AMMO_BATTERY && def->m_nAmmoUse == 50 && def->m_fProjVelocity == 0 &&
              def->m_nDamageRadius == 500 && std::string(def->m_szProjectileClass) == "CSingularityProjectile");
        // A distant monster: without one, the original Enemies list of a Cabal NPC falls back to
        // the Cabal list, which holds the observer itself (in game the player is in both lists).
        CHECK(npc(g, "ShikariAI", {3500, 40, -3500}, "NONE", "monster"));
        // The authored 03_Museum Prophet weapon name resolves to the generator.
        auto* shooter = npc(g, "Prophet", {0, 40, 0}, "SINGULARITY", "prophet");
        CHECK(shooter);
        auto* gun = dynamic_cast<CSingularity*>(shooter->m_InventoryMgr.CurrentWeapon());
        CHECK(gun);
        shooter->m_InventoryMgr.SetAmmoCount(AMMO_BATTERY, 200);
        // Characters carry USRFLG_SINGULARITY_ATTRACT (with SAVEABLE).
        CHECK((g.server.GetObjectUserFlags(shooter->m_hObject) & ((1u << 3) | (1u << 5))) == ((1u << 3) | (1u << 5)));
        WeaponFire request;
        request.weapon = WEAP_SINGULARITY;
        request.position = {0, 60, 0};
        request.direction = {0, 0, 1};
        const auto fired = gun->Fire(request);
        CHECK(fired == FireResult::Fired && shooter->m_InventoryMgr.GetAmmoCount(AMMO_BATTERY) == 150);
        // Blue-large laser to the wall, then the still projectile.
        CHECK(gun->beam && g.server.laser_beams.reports.back().type == LASER_BLUE_LARGE &&
              std::abs(gun->beam_end.z - 800) < .1f);
        const auto& fx = g.server.laser_beams.active.back();
        CHECK(fx.max_scale == 2.5f && fx.duration == 1 && fx.skin_path == "Skins\\Ammo\\beamblue.dtx");
        auto hole_handle = gun->projectile;
        auto* hole = dynamic_cast<CSingularityProjectile*>(g.server.HandleToObject(hole_handle));
        CHECK(hole && !hole->armed && hole->damage_type == DamageType::Singularity);
        // Its first update arms it 25 units off the wall, along the hit normal.
        CHECK(step(g, .01) && hole->armed && hole->ray == SegmentResult::Hit);
        DVector center;
        CHECK(g.server.GetObjectPos(hole_handle, &center) == DE_OK && std::abs(center.z - 775) < .1f &&
              std::abs(center.y - 60) < .1f);
        CHECK(hole->effect.type == 18 && hole->effect.sound == "Sounds\\Weapons\\Singularity\\impact.wav");
        // Victims, placed once it is armed (they would stop its ray): a character about 260
        // units from the hole (pulled, damaged inside half the squared radius) and a pushable
        // model 30 units from it (swallowed).
        auto* victim = npc(g, "CultistAI", {250, 40, 700}, "BERETTA", "victim");
        CHECK(victim);
        victim->m_damage.m_fMaxHitPoints = 1000;
        victim->m_damage.SetHitPoints(1000);
        auto crate = g.objects.create(g.objects.find_class("CDestructableModel"), {}, Creation::World,
                                      {{"InitFilename", std::string("models/characters/caleb.abc")},
                                       {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                       {"Pos", DVector{0, 60, 745}},
                                       {"Pushable", true},
                                       {"Gravity", false}});
        CHECK(crate && (g.server.GetObjectUserFlags(crate) & (1u << 5)));
        const float before = victim->m_damage.m_fHitPoints;
        DVector start;
        CHECK(g.server.GetObjectPos(victim->m_hObject, &start) == DE_OK);
        // A player would ping the NPCs awake every 0.5 s; an observer stands in for it.
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 100, 300};
        auto observer = g.objects.create(g.objects.find_class("BaseClass"), eye);
        CHECK(observer);
        for (int i = 0; i < 30; ++i)
        {
            if (i % 5 == 0)
                CHECK(g.server.PingObjects(observer));
            CHECK(step(g, .1));
        }
        CHECK(hole->swallowed == 1 && !g.server.IsObjectAlive(crate) && hole->object_fx_unavailable == 0 &&
              hole->object_fx_error.empty());
        DVector now;
        CHECK(g.server.IsObjectAlive(victim->m_hObject) && g.server.GetObjectPos(victim->m_hObject, &now) == DE_OK);
        std::cout << "pulls=" << hole->pulls << " damage=" << hole->damage_messages << " moved=" << now.z - start.z
                  << " hp=" << victim->m_damage.m_fHitPoints << "\n";
        CHECK(hole->pulls > 0 && now.z > start.z && hole->damage_messages > 0 &&
              victim->m_damage.m_fHitPoints < before);
        // Five seconds after arming it expires.
        for (int i = 0; i < 30 && g.server.IsObjectAlive(hole_handle); ++i)
            CHECK(step(g, .1));
        CHECK(!g.server.IsObjectAlive(hole_handle));
        CHECK(g.unload_world());
        return true;
    }
    bool pickup(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h =
            g.objects.create(g.objects.find_class("SingularityPU"), {}, Creation::World, {{"Pos", DVector{0, 40, 0}}});
        auto* r = g.objects.get(h);
        CHECK(r && r->model && r->model_error.empty());
        auto* item = dynamic_cast<SingularityPU*>(r->game.get());
        CHECK(item && item->value.type == WEAP_SINGULARITY && item->value.amount == 100 && item->resource_id == 365);
        CHECK(std::string(WeaponPickupClass(WEAP_SINGULARITY)) == "SingularityPU");
        CHECK(g.unload_world());
        return true;
    }
    bool ai(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* shooter = npc(g, "Prophet", {0, 40, 0}, "SINGULARITY", "prophet");
        // A monster (the Prophet is Cabal) beyond the 500-unit damage radius, in front of
        // the test wall at z = 800.
        auto* victim = npc(g, "SoulDrudge", {0, 40, 700}, "NONE", "victim");
        CHECK(shooter && victim);
        auto* gun = dynamic_cast<CSingularity*>(shooter->m_InventoryMgr.CurrentWeapon());
        CHECK(gun);
        // The NPC fire path aims the generator from r_gun (default AI_Mgr::Fire branch).
        CHECK(shooter->SetTarget("victim") && step(g, .01));
        CHECK(shooter->Fire(false));
        if (!gun->shots)
            std::cerr << shooter->fire_error << "\n";
        CHECK(gun->shots == 1 && gun->beam && shooter->fire_gun_node &&
              dynamic_cast<CSingularityProjectile*>(g.server.HandleToObject(gun->projectile)));
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
    if (mode == "ai")
        return ai(argv[2]) ? 0 : 1;
    if (mode == "pickup")
        return pickup(argv[2]) ? 0 : 1;
    return 2;
}
