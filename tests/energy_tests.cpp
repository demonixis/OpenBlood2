#include "app/sprite_scene.h"
#include "energy_fixture.h"
#include "game/original/AI_Mgr.h"
#include <cmath>
#include <limits>
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
    bool setup(GameSession& g, const std::filesystem::path& path, bool assets = true)
    {
        CHECK(test_player::setup(g, path));
        std::string error;
        CHECK(!assets || test_energy::mount(g.resources, path, error));
        return true;
    }
    bool weapon(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, 0}, true);
        auto* target = test_energy::actor(g, {0, 50, 100});
        auto* w = test_energy::equip(owner);
        CHECK(w && target);
        CHECK(w->data.m_nType == 27 && w->data.m_fProjVelocity == 1000 && w->data.m_nDamageRadius == 175);
        CHECK(w->data.m_nFireType == TYPE_MELEE && w->CheckAmmo() == 1 && w->ShowHandModel(true) && !w->m_hHandModel);
        WeaponFire shot{{0, 50, 0}, {0, 0, 1}, 42, 27, false, false};
        CHECK(w->Fire(shot) == FireResult::Fired && w->projectile && !w->last_shot.damage_delivered);
        const auto h = w->projectile;
        auto* p = dynamic_cast<CEnergyBlastProjectile*>(g.server.HandleToObject(h));
        CHECK(p);
        CHECK(target->damage.m_fHitPoints == 1000 && near(p->m_fDamage, w->last_shot.damage));
        CHECK(w->fire_sound_resource == "sounds\\enemies\\gideon\\distfire.wav" && w->sounds_played == 1);
        CHECK(g.step(.005) && g.objects.get(h)->state.m_Scale.x == 0 && near(g.objects.get(h)->state.m_Pos.z, 5));
        CHECK(g.step(.005) && near(g.objects.get(h)->state.m_Scale.x, .25f));
        CHECK(g.step(.09) && p->m_bExplode && near(g.objects.get(h)->state.m_Pos.z, 98));
        CHECK(target->damage.m_fHitPoints == 1000); // Contact is not damage until next original update.
        CHECK(g.step(.002) && !g.objects.get(h));
        CHECK(near(target->damage.m_fHitPoints, 1000 - w->last_shot.damage));
        CHECK(target->last_sender == h && target->damage.last_damage.attacker == owner->m_hObject);
        CHECK(target->damage.last_damage.type == DamageType::Explode && target->damage.last_damage.area_effect);
        CHECK(g.server.projectile_reports.back().outcome == "exploded" &&
              g.server.projectile_reports.back().delivered >= 2);
        CHECK(g.server.scene_explosions.back().type == 7 && g.server.scene_explosions.back().voice);
        CHECK(!g.server.scene_explosions.back().model_error.empty());
        shot.alternate = true;
        CHECK(w->Fire(shot) == FireResult::Fired);
        CHECK(w->fire_sound_resource == "Sounds\\Enemies\\Zealot\\distfire.wav" && w->sounds_played == 2);
        std::string error;
        CHECK(g.enter_player(error));
        CHECK(g.server_shell.Player(g.local_client)->m_InventoryMgr.ObtainWeapon(27) == InventoryResult::NotAvailable);
        std::cout << "Original ID27/25..50 damage, 1000-unit flight, radius175, primary Gideon/alt Zealot audio, "
                     "deferred explosion and separate sender/attacker verified.\n";
        return true;
    }
    bool filters(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        CHECK(owner);
        auto* nonsolid = test_energy::actor(g, {0, 50, 20}, false, FLAG_RAYHIT);
        CHECK(nonsolid);
        auto* target = test_energy::actor(g, {0, 50, 100});
        CHECK(target);
        auto* first = test_energy::projectile(g, owner->m_hObject, {0, 50, 0});
        CHECK(first);
        auto* second = test_energy::projectile(g, owner->m_hObject, {0, 50, 40}, {0, 0, 1}, 0);
        CHECK(second);
        g.server.SetObjectFlags(second->m_hObject, second->flags | FLAG_SOLID);
        CHECK(g.server.advance_projectiles(.2));
        CHECK(first->contact.object == target->m_hObject && near(first->contact.point.z, 98));
        CHECK(!second->m_bExplode && nonsolid->damage.m_fHitPoints == 1000);
        auto* direct = test_energy::projectile(g, owner->m_hObject, {0, 50, 90}, {0, 0, 1}, 1000, 0);
        CHECK(direct && direct->AdvanceFlight(.02) && direct->m_bExplode);
        direct->Explode();
        CHECK(target->damage.m_fHitPoints == 960 && !target->damage.last_damage.area_effect);
        CHECK(near(target->damage.last_damage.direction.z, 20));
        auto* floor = test_energy::projectile(g, owner->m_hObject, {100, 50, 0}, {0, -1, 0});
        CHECK(floor);
        CHECK(g.server.advance_projectiles(.1) && floor->m_bExplode && floor->contact.polygon != UINT32_MAX);
        CHECK(near(g.objects.get(floor->m_hObject)->state.m_Pos.y, 0));
        WorldModel sky;
        sky.points = {{-1000, 0, -1000}, {1000, 0, -1000}, {1000, 0, 1000}, {-1000, 0, 1000}};
        sky.surfaces.resize(1);
        sky.surfaces[0].flags = 1;
        sky.surfaces[0].texture_flags = 110;
        sky.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(sky, error));
        auto* sky_shot = test_energy::projectile(g, owner->m_hObject, {200, 50, 0}, {0, -1, 0});
        CHECK(sky_shot);
        auto sky_handle = sky_shot->m_hObject;
        CHECK(g.server.advance_projectiles(.1) && g.objects.get(sky_handle)->life != Life::Alive);
        CHECK(g.server.projectile_reports.back().outcome == "sky" && !sky_shot->m_bExplode);
        SegmentHit invalid;
        CHECK(g.server.IntersectSegment({}, {0, 0, 1}, owner->m_hObject, invalid, false, SegmentObjects::SolidOnly,
                                        SegmentFilter::ProjectileContact) == SegmentResult::Invalid);
        std::cout << "Continuous point collision ignores shooter, nonsolid ray targets and exact projectile class; "
                     "solid target and primary world stop flight.\n";
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        CHECK(owner);
        auto* target = test_energy::actor(g, {0, 50, 100});
        CHECK(target);
        auto* p = test_energy::projectile(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        auto h = p->m_hObject;
        CHECK(g.objects.remove(owner->m_hObject) && g.objects.finish_frame() && p->m_hFiredFrom == nullptr);
        CHECK(g.step(.1) && p->m_bExplode && g.step(.002) && !g.objects.get(h));
        CHECK(near(target->damage.m_fHitPoints, 960) && !target->damage.last_damage.attacker &&
              target->last_sender == h);
        auto* p2 = test_energy::projectile(g, target->m_hObject, {0, 80, 0}, {0, 0, 1}, 0);
        CHECK(p2);
        h = p2->m_hObject;
        CHECK(g.objects.advance(double(p2->m_fStartTime + p2->m_fLifeTime) - g.objects.time()) &&
              g.objects.get(h)->life == Life::Alive); // Strict > lifetime.
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.server.projectile_reports.back().outcome == "expired");
        auto* p3 = test_energy::projectile(g, target->m_hObject, {0, 50, 90}, {0, 0, 1}, 1000, 0);
        CHECK(p3);
        h = p3->m_hObject;
        CHECK(g.server.advance_projectiles(.01) && !p3->m_bExplode); // Shooter is excluded.
        p3->m_bExplode = true;
        CHECK(g.objects.advance(.01) && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.unload_world() && g.server.projectile_reports.empty() && g.server.scene_explosions.empty());
        CHECK(CDestructable::m_dwNumDestructables == 0 && g.server.message_count() == 0 &&
              g.server.string_count() == 0);
        std::cout << "Shooter unlink retains unattributed damage; strict lifetime expiry and complete unload release "
                     "verified.\n";
        return true;
    }
    bool callbacks(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        auto* target = test_energy::actor(g, {0, 50, 100});
        CHECK(owner && target);
        auto* p = test_energy::projectile(g, owner->m_hObject, {0, 50, 90});
        CHECK(p);
        auto h = p->m_hObject;
        target->remove_on_damage = h;
        CHECK(g.server.advance_projectiles(.02) && p->m_bExplode);
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(target->damage.m_fHitPoints == 960 && g.server.message_count() == 0);
        p = test_energy::projectile(g, owner->m_hObject, {0, 50, 90});
        CHECK(p);
        h = p->m_hObject;
        CHECK(g.server.advance_projectiles(.02) && p->m_hHitObject == target->m_hObject);
        CHECK(g.objects.remove(target->m_hObject) && g.objects.finish_frame() && !p->m_hHitObject);
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h));
        // Source projectile HP5: damage itself schedules explosion on its next callback.
        p = test_energy::projectile(g, owner->m_hObject, {0, 50, 10}, {0, 0, 1}, 0);
        CHECK(p);
        h = p->m_hObject;
        auto message = g.server.StartMessageToObject(owner, h, damage_message);
        CHECK(message);
        CHECK(g.server.WriteDamage(message, {{0, 0, 1}, {0, 50, 10}, 10, DamageType::Normal, owner->m_hObject}) ==
              DE_OK);
        CHECK(g.server.EndMessage(message) == DE_OK && p->m_damage.IsDead());
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h));
        auto* touched = test_energy::actor(g, {0, 50, 100}, false, FLAG_SOLID | FLAG_TOUCH_NOTIFY);
        CHECK(touched);
        p = test_energy::projectile(g, owner->m_hObject, {0, 50, 90});
        CHECK(p);
        h = p->m_hObject;
        touched->remove_on_touch = h;
        CHECK(g.server.advance_projectiles(.02) && touched->touches == 1);
        CHECK(g.objects.get(h)->life != Life::Alive && g.objects.finish_frame() && !g.objects.get(h));
        std::cout << "Damage callback removes projectile safely, hit removal breaks link, HP5 destruction explodes on "
                     "update.\n";
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, false));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        auto* w = test_energy::equip(owner);
        CHECK(w);
        WeaponFire shot{{0, 50, 0}, {0, 0, 1}, 42, 27, false, false};
        CHECK(w->Fire(shot) == FireResult::Unavailable && !w->projectile && !w->projectile_error.empty());
        CHECK(g.objects.finish_frame());
        std::string error;
        CHECK(test_energy::mount(g.resources, root, error));
        auto* p = test_energy::projectile(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        auto h = p->m_hObject;
        g.server.collision_ready = false;
        CHECK(!g.server.advance_projectiles(.1) && p->failed && g.server.projectile_reports.back().outcome == "failed");
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        g.server.collision_ready = true;
        p = test_energy::projectile(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        h = p->m_hObject;
        CHECK(!g.server.advance_projectiles(.501) && p->failed && g.objects.finish_frame() && !g.objects.get(h));
        p = test_energy::projectile(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        CHECK(g.server.ScaleObject(p->m_hObject, {0, 0, 0}) && !g.server.ScaleObject(p->m_hObject, {-1, 1, 1}));
        auto* other = test_energy::actor(g, {0, 50, 40});
        CHECK(other);
        auto message = g.server.StartMessageToObject(owner, other->m_hObject, damage_message);
        CHECK(message);
        CHECK(g.server.WriteDamage(message, {{}, {}, 1, DamageType::Normal, other->m_hObject}) == DE_INVALIDPARAMS);
        CHECK(g.server.EndMessage(message) != DE_OK); // Unrelated callers cannot forge projectile attribution.
        CHECK(g.objects.create(g.objects.find_class("CEnergyBlastProjectile"), {}, Creation::World) == nullptr);
        return true;
    }
    bool ai(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* target = test_energy::actor(g, {0, 50, 100});
        CHECK(target);
        auto h = g.objects.create(g.objects.find_class("AI_Mgr"));
        auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
        CHECK(a);
        CHECK(a->m_InventoryMgr.Init(h, nullptr) && a->m_InventoryMgr.ObtainWeapon(27, 0) == InventoryResult::Ok);
        auto model = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *model, error));
        model->nodes[1].name = "r_hand_extra2";
        auto* r = g.objects.get(h);
        r->model = model;
        r->state.m_ObjectType = OT_MODEL;
        r->state.m_Pos = {0, 50, 0};
        a->m_vTargetPos = {0, 50, 100};
        for (auto& animation : model->animations)
            for (auto& key : animation.keys)
                key.text.clear();
        model->animations[0].keys = {{0, {}, {}, "[fire_key:1]"}, {100, {}, {}, ""}};
        r->state.m_Flags |= FLAG_MODELKEYS;
        g.server.SetModelAnimation(h, 0);
        CHECK(a->SetNewTarget(target->m_hObject));
        CHECK(g.server.advance_models(.01) && a->fire_shots == 1 && a->clear_shot.rays == 0);
        CHECK(target->damage.m_fHitPoints == 1000); // FIRE_KEY creates flight; no immediate vector damage.
        model->nodes[1].name = "missing";
        CHECK(!a->Fire() && a->fire_error.find("r_hand_extra2") != std::string::npos);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        for (unsigned i = 0; i < 3; ++i)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* owner = test_energy::actor(g, {0, 170, -2400});
            auto* w = test_energy::equip(owner);
            CHECK(w);
            CHECK(w->Fire({{0, 170, -2400}, {0, 0, 1}, 42, 27, false, false}) == FireResult::Fired &&
                  w->sounds_played == 1);
            auto h = w->projectile;
            auto* p = dynamic_cast<CEnergyBlastProjectile*>(g.server.HandleToObject(h));
            CHECK(p);
            auto* r = g.objects.get(h);
            CHECK(r->sprite && !r->sprite->frames.empty());
            SpriteView view;
            CHECK(view.begin_frame({0, 170, -2500}, {}, error));
            CHECK(view.add(r->sprite.get(), r->sprite_track, {r->state.m_Pos, r->state.m_Rotation}, {.25f, .25f, .25f},
                           r->state.m_Flags, r->id, g.resources, error));
            CHECK(view.entries.size() == 1 && view.entries.front().error.empty() && !view.draws.empty());
            WorldMessage message;
            CHECK(original_world_request("01_SubwayA", 0, 1, 1, message));
            CHECK(g.rehearse_destination(message, "", error) && g.objects.get(h) &&
                  p->m_hFiredFrom == owner->m_hObject);
            CHECK(g.server.advance_projectiles(.016));
            p->Explode();
            CHECK(!g.server.scene_explosions.empty() && g.server.scene_explosions.back().voice);
            std::cout << "Private loop " << i << ": SPR " << r->sprite->frames.front()
                      << ", primary and Tesla impact WAV, radius deliveries " << p->targets.delivered
                      << ", isolated rehearsal retains projectile.\n";
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.projectile_reports.empty());
            CHECK(CDestructable::m_dwNumDestructables == 0 && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string n = argv[1];
    return (n == "weapon"      ? weapon(argv[2])
            : n == "filters"   ? filters(argv[2])
            : n == "lifetime"  ? lifetime(argv[2])
            : n == "callbacks" ? callbacks(argv[2])
            : n == "failures"  ? failures(argv[2])
            : n == "ai"        ? ai(argv[2])
            : n == "private"   ? private_data(argv[2])
                               : false)
               ? 0
               : 1;
}
