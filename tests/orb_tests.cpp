#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "brush_fixture.h"
#include "game/original/ChosenAI.h"
#include "game/original/ClientCrosshair.h"
#include "game/original/OrbProjectile.h"
#include "game/original/WeaponPickups.h"
#include "game/original/spawn_object.h"
#include <cmath>
#include <iostream>
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
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        auto floor = test_brush::box("floor", {-4000, -20, -4000}, {4000, -10, 4000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "synthetic Orb probe with retail assets";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Pos", DVector{0, 100, -500}}}));
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        if (!g.enter_player(error) || !g.pump_player_messages(error))
        {
            std::cerr << error << '\n';
            return false;
        }
        return true;
    }
    ChosenAI* target(GameSession& g, DVector at)
    {
        auto h = g.objects.create(g.objects.find_class("GabriellaAI"), {}, Creation::World, {{"Pos", at}});
        auto* p = dynamic_cast<ChosenAI*>(g.server.HandleToObject(h));
        if (p)
        {
            p->m_damage.SetArmorPoints(0);
            p->m_damage.apply_damage_physics = false;
        }
        return p;
    }
    COrbProjectile* orb(GameSession& g, DVector at, bool alternate = false)
    {
        ObjectCreateStruct state;
        state.m_Pos = at;
        auto* p = dynamic_cast<COrbProjectile*>(
            g.server.CreateObject(g.server.GetClass(alternate ? "COrbAltProjectile" : "COrbProjectile"), &state));
        if (!p || !p->Setup({0, 0, 1}, WEAP_ORB, 12, 500, 0, g.client_shell.player))
            return nullptr;
        return p;
    }
    bool steering(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* t = target(g, {80, 100, 0});
        CHECK(t && t->RuntimeReady());
        DVector head;
        DRotation rot;
        CHECK(g.server.GetModelNodeTransform(t->m_hObject, "head", &head, &rot));
        auto* p = orb(g, vector_add(head, {-50, 0, -100}));
        CHECK(p && p->trail);
        const auto id = p->m_hObject;
        DVector v{-100, -100, -100};
        CHECK(g.server.SetVelocity(t->m_hObject, &v) == DE_OK);
        CHECK(g.objects.advance(.002) && !p->tracked);
        v = {.1f, 0, 0};
        CHECK(g.server.SetVelocity(t->m_hObject, &v) == DE_OK);
        CHECK(g.objects.advance(.002) && p->tracked == t->m_hObject);
        CHECK(g.server.GetVelocity(id, &v) == DE_OK && v.x > 0 && std::abs(std::sqrt(vector_dot(v, v)) - 500) < .01f);
        CHECK(g.objects.advance(11) && g.objects.get(id)); // Original override has no generic ten-second expiry.
        CHECK(g.objects.remove(t->m_hObject) && g.objects.finish_frame());
        CHECK(g.objects.advance(.002) && !p->tracked && !p->failed);
        CHECK(g.unload_world());
        return true;
    }
    bool attachment(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* t = target(g, {100, 100, 0});
        CHECK(t);
        DVector head;
        DRotation rot;
        CHECK(g.server.GetModelNodeTransform(t->m_hObject, "head", &head, &rot));
        auto* p = orb(g, head);
        CHECK(p);
        auto h = p->m_hObject;
        SegmentHit hit;
        hit.object = t->m_hObject;
        hit.point = head;
        p->HandleContact(hit);
        CHECK(p->attached && !p->m_bExplode && p->m_hHitObject == hit.object);
        CHECK(g.objects.advance(1) && p->damage_ticks == 0);
        const auto hp = t->m_damage.m_fHitPoints;
        CHECK(g.objects.advance(.002));
        CHECK(p->damage_ticks == 1 && t->m_damage.last_damage.amount == 12 && t->m_damage.m_fHitPoints < hp);
        CHECK(p->blood.blood_requested && p->blood.blood_events[0] && p->blood.blood_events[1] && p->blood.spark_event);
        DVector pos;
        CHECK(g.server.GetObjectPos(h, &pos) == DE_OK);
        CHECK(vector_dot(vector_subtract(pos, vector_add(head, vector_scale(rotate_vector(rot, {0, 0, 1}), 7.5f))),
                         vector_subtract(pos, vector_add(head, vector_scale(rotate_vector(rot, {0, 0, 1}), 7.5f)))) <
              .001f);
        CHECK(g.objects.advance(4.1) && p->m_bExplode && !p->exploded); // Strict >5.1, explosion next update.
        CHECK(g.objects.advance(.002) && !g.objects.get(h));
        CHECK(!g.server.scene_explosions.empty() && g.server.scene_explosions.back().fragment_error.empty());
        CHECK(g.server.explosion_fragments.active.size() == 10 && g.server.explosion_fragments.trails.size() == 10);
        for (const auto& f : g.server.explosion_fragments.active)
            CHECK(f.settings.trail_type == 34 && f.settings.scale.x == .25f);
        // Alternate requires a head within 45 units; primary did not impose that distance gate.
        p = orb(g, vector_add(head, {0, -60, 0}), true);
        CHECK(p);
        p->HandleContact(hit);
        CHECK(!p->attached && p->m_bExplode);
        CHECK(g.objects.advance(.002));
        p = orb(g, head, true);
        CHECK(p);
        h = p->m_hObject;
        p->HandleContact(hit);
        CHECK(p->attached);
        CHECK(g.objects.remove(t->m_hObject) && g.objects.finish_frame() && !p->m_hHitObject);
        CHECK(g.objects.advance(.002) && p->m_bExplode);
        CHECK(g.objects.advance(.002) && !g.objects.get(h));
        CHECK(g.unload_world());
        return true;
    }
    bool camera(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& c = g.client_shell;
        auto* player = g.server_shell.Player(g.local_client);
        auto& inv = player->m_InventoryMgr;
        std::string error;
        CHECK(inv.ObtainWeapon(WEAP_ORB, 2) == InventoryResult::Ok && g.select_weapon(2, error));
        inv.SetAmmoCount(AMMO_FOCUS, 500);
        WeaponFire request;
        request.weapon = WEAP_ORB;
        request.position = {0, 200, 0};
        request.direction = {0, 0, 1};
        request.alternate = true;
        CHECK(g.server_shell.OnMessage(g.local_client, request) == FireResult::Fired);
        auto* weapon = inv.CurrentWeapon();
        auto* p = dynamic_cast<COrbAltProjectile*>(g.server.HandleToObject(weapon->projectile));
        CHECK(p);
        auto h = p->m_hObject;
        CHECK(g.pump_player_messages(error) && c.orb_view == h && c.view.object == h &&
              player->camera_movement_blocked);
        CHECK(c.player_visible && !c.weapons_visible && std::abs(c.ViewFovX() - original_fov_normal * 1.5f) < .001f);
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerRotation{.2f, .7f}));
        CHECK(g.objects.advance(.002) && player->orb_object == h);
        DRotation r;
        CHECK(g.server.GetObjectRotation(h, &r) == DE_OK);
        const auto f = rotate_vector(r, {0, 0, 1}),
                   expected = rotate_vector(rotation_from_euler({.2f, .7f, 0}), {0, 0, 1});
        CHECK(vector_dot(vector_subtract(f, expected), vector_subtract(f, expected)) < .00001f);
        PlayerState state;
        CHECK(!player->SaveFields(state, error) && error.find("Orb") != error.npos);
        CHECK(player->HandleWeaponFireMessage(request) == FireResult::Blocked);
        ModelView models;
        CHECK(update_model_scene(g, models, error));
        bool hidden = false;
        for (const auto& entry : models.entries)
            if (entry.object == g.objects.get(h)->id)
                hidden = entry.range == UINT32_MAX;
        CHECK(hidden);
        // An old camera-off event cannot clear a newer Orb.
        CHECK(g.server.SendToClient(g.local_client, OrbView{player->m_hObject, false}) == DE_OK);
        CHECK(g.pump_player_messages(error) && c.orb_view == h);
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && !player->orb_object);
        CHECK(c.UpdateInventoryEffects() && c.UpdateCameraSelection() && !c.orb_view &&
              !player->camera_movement_blocked);
        CHECK(g.unload_world() && !c.orb_view);
        return true;
    }
    bool client(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* player = g.server_shell.Player(g.local_client);
        auto& inventory = player->m_InventoryMgr;
        CHECK(inventory.ObtainWeapon(WEAP_ORB, 2) == InventoryResult::Ok);
        std::string error;
        CHECK(g.select_weapon(2, error));
        CHECK(std::string(WeaponPickupClass(WEAP_ORB)) == "OrbPU" && g.server.ValidateSpawnObject("OrbPU"));
        auto pickup = SpawnObject("OrbPU", {500, 100, 0}, {}, {10, 20, 30});
        CHECK(pickup && dynamic_cast<OrbPU*>(g.server.HandleToObject(pickup)) && g.objects.get(pickup)->model);
        CHECK(g.objects.remove(pickup) && g.objects.finish_frame());
        auto& view = g.client_shell.weapons[0];
        auto tick = [&](WeaponInput input)
        {
            return g.objects.advance(1. / 60) &&
                   view.Update(g.server, g.server_shell, g.local_client, input, {0, 200, 0}, {0, 0, 1}, 1. / 60, error);
        };
        for (bool alt : {false, true})
        {
            for (unsigned i = 0; i < 120; ++i)
                CHECK(tick({}));
            CHECK(view.state == WS_REST && view.animation.looping);
            inventory.SetAmmoCount(AMMO_FOCUS, 1000);
            const auto shots = view.shots;
            for (unsigned i = 0; i < 180; ++i)
                CHECK(tick({!alt, alt, false}));
            CHECK(view.shots > shots && view.fire_keys > 0);
            CHECK(inventory.GetAmmoCount(AMMO_FOCUS) == 1000 - float(view.shots - shots) * (alt ? 200.f : 20.f));
        }
        auto* ai = target(g, {300, 100, 500});
        CHECK(ai);
        CHECK(ai->m_InventoryMgr.ObtainWeapon(WEAP_ORB) == InventoryResult::Ok);
        CHECK(ai->m_InventoryMgr.ChangeWeapon(WEAP_ORB) == InventoryResult::Ok && ai->SetNewTarget(player->m_hObject));
        ai->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 100);
        // Explicit base fire is the same path used by Ishmael model keys, without Chosen alternate selection.
        CHECK(ai->AI_Mgr::Fire(false) && ai->fire_shots == 1 && ai->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 80);
        CHECK(dynamic_cast<COrbProjectile*>(g.server.HandleToObject(ai->m_InventoryMgr.CurrentWeapon()->projectile)));
        CHECK(g.unload_world());
        return true;
    }
    bool effects(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        CObjectParticleFX primary, fragments;
        std::minstd_rand rng{11};
        CHECK(primary.Init(33, 1, rng, error) && fragments.Init(34, 2, error));
        CHECK(primary.radius == 200 && primary.gravity == 0 && primary.authored_count == 3 && primary.density == 7.5f);
        CHECK(fragments.radius == 200 && fragments.gravity == 0 && fragments.density == 5 && fragments.max_life == .2f);
        CHECK(primary.CreateObject(g.resources, {}, 0, error) && fragments.CreateObject(g.resources, {}, 0, error));
        NodePose at;
        CHECK(primary.Update(.001, &at, rng) && fragments.Update(.001, &at, rng));
        at.position = {15, 0, 0};
        CHECK(primary.Update(.03, &at, rng) && fragments.Update(.03, &at, rng));
        CHECK(primary.particles.size() == 30 && fragments.particles.size() == 15);
        for (const auto& p : primary.particles)
            CHECK(p.color.z == 1 && p.color.x >= 128.f / 255 && p.color.x == p.color.y && p.lifetime >= .25f &&
                  p.lifetime <= .35f);
        for (const auto& p : fragments.particles)
            CHECK(p.color.z == 1 && p.velocity.z >= 20 && p.velocity.z <= 40);
        const auto event =
            g.server.explosion_fragments.publish(21, {0, 100, 0}, {0, 1, 0}, "orb", g.models, g.resources, 0, error);
        CHECK(event && error.empty() && g.server.explosion_fragments.active.size() == 10 &&
              g.server.explosion_fragments.trails.size() == 10);
        CHECK(g.server.explosion_fragments.advance(.001, g.server) &&
              g.server.explosion_fragments.advance(.03, g.server));
        CHECK(g.server.explosion_fragments.trail_particles() > 0);
        SpriteView sprites;
        ModelView models;
        CHECK(update_sprite_scene(g, sprites, error) && update_model_scene(g, models, error));
        CHECK(g.server.explosion_fragments.advance(8, g.server) && g.server.explosion_fragments.active.empty() &&
              g.server.explosion_fragments.trails.empty());
        g.server.explosion_fragments.trails.resize(128);
        CHECK(
            g.server.explosion_fragments.publish(21, {0, 100, 0}, {0, 1, 0}, "full", g.models, g.resources, 8, error));
        CHECK(!error.empty() && g.server.explosion_fragments.trail_failed == 10);
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "steering"     ? steering(argv[2])
            : mode == "attachment" ? attachment(argv[2])
            : mode == "camera"     ? camera(argv[2])
            : mode == "client"     ? client(argv[2])
                                   : effects(argv[2]))
               ? 0
               : 1;
}
