#include "app/model_scene.h"
#include "game/original/AI_Mgr.h"
#include "wind_fixture.h"
#include <cmath>
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
    bool setup(GameSession& g, const std::filesystem::path& root, bool assets = true)
    {
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(!assets || test_wind::mount(g.resources, root, error));
        return true;
    }
    bool weapon(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = test_energy::actor(g, {100, 50, 100});
        auto* w = test_wind::equip(owner);
        CHECK(w && w->data.m_nType == 42 && w->data.m_nFireType == 0 && w->CheckAmmo() == 1);
        CHECK(w->data.m_fProjVelocity == 0 && w->data.m_nDamageRadius == 0 && w->data.m_fRange == 75);
        CHECK(w->data.m_fReloadTime == 1 && w->data.m_fAltReloadTime == 0 && w->ShowHandModel(true) &&
              !w->m_hHandModel);
        auto* target = test_energy::actor(g, {100, 0, 550}); // Exact 450, from projected ground point.
        auto* outside = test_energy::actor(g, {100, 0, 550.1f});
        auto* inside = test_energy::actor(g, {110, 0, 100});
        CHECK(target && outside && inside);
        ObjectCreateStruct wall;
        wall.m_Pos = {100, 50, 300};
        wall.m_Flags = FLAG_SOLID;
        auto obstacle = g.objects.create(g.objects.find_class("BaseClass"), wall);
        CHECK(obstacle);
        g.objects.get(obstacle)->dimensions = {50, 100, 10}; // Occlusion does not suppress source shockwave damage.
        auto* p = test_wind::fire(g, w);
        CHECK(p && p->m_fDamage >= 10 && p->m_fDamage <= 20 && p->m_fVelocity == 0);
        auto h = p->m_hObject;
        CHECK(g.objects.get(h)->state.m_ObjectType == OT_NORMAL && !g.objects.get(h)->model &&
              !g.objects.get(h)->sprite);
        CHECK(target->damage.m_fHitPoints == 1000 && w->sounds_played == 1 && !w->last_shot.damage_delivered);
        CHECK(g.objects.advance(.0005) && g.objects.get(h) && target->damage.m_fHitPoints == 1000);
        CHECK(g.objects.advance(.0015) && !g.objects.get(h));
        CHECK(near(target->damage.m_fHitPoints, 1000 - w->last_shot.damage));
        CHECK(near(inside->damage.m_fHitPoints, target->damage.m_fHitPoints) && outside->damage.m_fHitPoints == 1000);
        CHECK(owner->damage.m_fHitPoints == 1000); // Three deliveries below include the projectile itself.
        CHECK(target->damage.last_damage.type == DamageType::Explode && !target->damage.last_damage.area_effect);
        CHECK(target->last_sender == h && target->damage.last_damage.attacker == owner->m_hObject);
        CHECK(near(target->damage.last_damage.direction.z, 450) && near(target->damage.last_damage.position.z, 550));
        auto report = g.server.projectile_reports.back();
        CHECK(report.shockwave && report.outcome == "exploded" && report.delivered == 3 && report.in_radius == 3);
        CHECK(report.query == SegmentResult::Hit && near(report.query_from.x, 100) && near(report.query_to.y, -950));
        CHECK(near(report.blast_position.y, 0) && near(report.blast_normal.y, 1));
        CHECK(g.server.scene_explosions.back().voice && g.server.scene_explosions.back().type == 20 &&
              g.server.scene_explosions.back().targets.delivered == report.delivered);
        CHECK(g.server.explosion_models.active.size() == 1 && g.server.explosion_lights.active.size() == 1);
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        auto* alt = test_wind::fire(g, w, true);
        CHECK(alt && w->sounds_played == 2 && w->FireVector() == FireResult::Unavailable);
        std::string error;
        CHECK(g.enter_player(error));
        CHECK(g.server_shell.Player(g.local_client)->m_InventoryMgr.ObtainWeapon(42) == InventoryResult::NotAvailable);
        std::cout << "Original ID42 both modes, no ammo/held model, first-update source-centered 450-unit full damage, "
                     "owner exclusion/self inclusion verified.\n";
        return true;
    }
    bool failure(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, false));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        auto* w = test_wind::equip(owner);
        CHECK(w);
        auto* target = test_energy::actor(g, {0, 0, 100});
        CHECK(target);
        auto* p = test_wind::fire(g, w);
        CHECK(p);
        auto h = p->m_hObject;
        CHECK(g.objects.advance(.002) && target->damage.m_fHitPoints < 1000);
        CHECK(!g.server.scene_explosions.back().model_error.empty() &&
              !g.server.scene_explosions.back().sound_error.empty());
        CHECK(g.server.explosion_lights.active.size() == 1 &&
              g.objects.finish_frame()); // Missing presentation does not suppress damage.
        p = test_wind::fire(g, w);
        CHECK(p);
        h = p->m_hObject;
        g.server.collision_ready = false;
        CHECK(g.objects.advance(.002) && g.server.projectile_reports.back().outcome == "failed");
        CHECK(g.server.projectile_reports.back().delivered == 0 && g.objects.finish_frame() && !g.objects.get(h));
        g.server.collision_ready = true;
        p = test_wind::fire(g, w);
        CHECK(p);
        h = p->m_hObject;
        CHECK(g.objects.remove(owner->m_hObject) && g.objects.finish_frame() && !p->m_hFiredFrom);
        CHECK(g.objects.advance(.002) && g.server.projectile_reports.back().outcome == "failed");
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.objects.create(g.objects.find_class("CBehemothShockwaveProjectile"), {}, Creation::World) == nullptr);
        auto raw = g.objects.create(g.objects.find_class("CBehemothShockwaveProjectile"));
        CHECK(raw);
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(raw));
        std::cout << "Missing presentation preserves damage; missing shooter/setup/collision fails without blast; "
                     "authored creation rejected.\n";
        return true;
    }
    struct Receiver : test_energy::Actor
    {
        Objects* objects = nullptr;
        HOBJECT wave = nullptr, remove = nullptr;
        unsigned received = 0;
        bool spawn_on_damage = false;
        HOBJECT spawned = nullptr;
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD data) override
        {
            auto result = Actor::ObjectMessageFn(sender, message, data);
            if (message == damage_message)
            {
                ++received;
                if (wave)
                    objects->engine_message(wave, MID_UPDATE, nullptr);
                if (remove)
                    GetServerDE()->RemoveObject(remove);
                if (spawn_on_damage)
                {
                    ObjectCreateStruct state;
                    state.m_Pos = {0, 0, 150};
                    if (auto* child = GetServerDE()->CreateObject(GetServerDE()->GetClass("WindReceiver"), &state))
                        spawned = child->m_hObject;
                }
            }
            return result;
        }
    };
    bool callbacks(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        ClassInfo type{"WindReceiver",
                       g.objects.find_class("BaseClass"),
                       []() -> std::unique_ptr<BaseClass> { return std::make_unique<Receiver>(); },
                       {}};
        CHECK(g.objects.register_class(type));
        for (unsigned mode = 0; mode < 4; ++mode)
        {
            auto* owner = test_energy::actor(g, {0, 50, 0});
            auto* w = test_wind::equip(owner);
            CHECK(w);
            const auto owner_handle = owner->m_hObject;
            auto* later = mode == 3 ? test_energy::actor(g, {0, 0, 200}) : nullptr;
            const auto later_handle = later ? later->m_hObject : nullptr;
            ObjectCreateStruct state;
            state.m_Pos = {0, 0, 100};
            auto h = g.objects.create(&type, state);
            auto* r = dynamic_cast<Receiver*>(g.server.HandleToObject(h));
            CHECK(r);
            auto* p = test_wind::fire(g, w);
            CHECK(p);
            auto shot = p->m_hObject;
            r->objects = &g.objects;
            r->wave = shot;
            r->remove = mode == 0 ? nullptr : mode == 1 ? shot : mode == 2 ? owner_handle : later_handle;
            r->spawn_on_damage = mode == 3;
            auto before = g.server.scene_explosions.size();
            CHECK(g.objects.advance(.002) && r->received == 1 && g.server.scene_explosions.size() == before + 1);
            CHECK(g.server.projectile_reports.back().delivered >= 1);
            CHECK(g.server.projectile_reports.back().cancelled == (mode == 1 || mode == 2));
            if (mode == 3)
            {
                CHECK(!g.objects.get(later_handle) && g.server.IsObjectAlive(r->spawned));
                auto* child = dynamic_cast<Receiver*>(g.server.HandleToObject(r->spawned));
                CHECK(child && child->received == 0 && child->damage.m_fHitPoints == 1000);
                CHECK(g.objects.remove(r->spawned));
            }
            CHECK(g.objects.finish_frame() && !g.objects.get(shot) && g.server.message_count() == 0);
            CHECK(g.objects.remove(h));
            if (g.server.IsObjectAlive(owner_handle))
                CHECK(g.objects.remove(owner_handle));
            CHECK(g.objects.finish_frame());
        }
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && g.server.audio.voices().empty());
        std::cout << "Nested update cannot duplicate shockwave; receiver removes projectile/shooter safely, no "
                     "dangling messages or global-list entries.\n";
        return true;
    }
    bool recipe(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* owner = test_energy::actor(g, {2000, 50, 2000});
        auto* w = test_wind::equip(owner);
        CHECK(w);
        auto* p = test_wind::fire(g, w);
        CHECK(p);
        CHECK(g.objects.advance(.002) && g.server.projectile_reports.back().query == SegmentResult::Miss);
        CHECK(near(g.server.projectile_reports.back().blast_normal.y, -1000));
        CHECK(g.server.explosion_models.active.size() == 1 && g.server.explosion_lights.active.size() == 1);
        const auto& model = g.server.explosion_models.active[0];
        const auto& light = g.server.explosion_lights.active[0];
        CHECK(near(model.settings.position.x, 2000) && near(model.settings.position.y, 50));
        CHECK(model.settings.scale1.x == 200 && model.settings.scale2.x == 1500 && model.settings.rotations.x == .05f);
        CHECK(model.settings.alpha == .5f && model.settings.waveform == 1 && model.settings.fade == 1 &&
              model.settings.random_rotation == 0);
        CHECK(light.settings.position.y == -14950 && light.settings.radius1 == 400 && light.settings.radius2 == 100);
        std::minstd_rand random(1), unchanged(1);
        std::optional<ExplosionModelSettings> settings;
        std::string error;
        CHECK(original_explosion_model(20, {}, {0, 1, 0}, random, settings, error) && random == unchanged);
        const auto start = model.start_time;
        CHECK(g.server.explosion_models.advance(start + .125) &&
              near(g.server.explosion_models.active[0].scale.x, 850));
        CHECK(near(g.server.explosion_models.active[0].alpha, .375f));
        auto count = g.server.audio.voices().size();
        CHECK(count == 2);
        CHECK(g.objects.remove(owner->m_hObject) && g.objects.finish_frame());
        CHECK(g.server.audio.voices().size() == count - 1 && g.server.explosion_models.active.size() == 1);
        CHECK(g.server.explosion_models.advance(start + .5) && g.server.explosion_models.active.empty());
        CHECK(g.server.explosion_lights.advance(start + 1) && g.server.explosion_lights.active.empty());
        std::cout << "Miss keeps raw downward normal; source model 200->1500/sqrt, alpha.5, rotation.05; white "
                     "light400->100; detached lifetimes verified.\n";
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
        CHECK(a->m_InventoryMgr.Init(h, nullptr) && a->m_InventoryMgr.ObtainWeapon(42, 0) == InventoryResult::Ok);
        auto model = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *model, error));
        model->nodes[1].name = "r_gun";
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
        CHECK(g.server.advance_models(.01) && a->fire_shots == 1 && a->clear_shot.rays == 1);
        CHECK(target->damage.m_fHitPoints == 1000); // FIRE_KEY creates flight; no immediate vector damage.
        const auto* weapon = a->m_InventoryMgr.CurrentWeapon();
        CHECK(weapon && dynamic_cast<CBehemothShockwaveProjectile*>(g.server.HandleToObject(weapon->projectile)));
        std::cout << "Original generic r_gun FIRE_KEY creates a shockwave with no immediate vector damage.\n";
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
            auto* w = test_wind::equip(owner);
            CHECK(w);
            auto* p = test_wind::fire(g, w);
            CHECK(p && w->sounds_played == 1);
            auto h = p->m_hObject;
            WorldMessage message;
            CHECK(original_world_request("01_SubwayA", 0, 1, 1, message));
            CHECK(g.rehearse_destination(message, "", error) && g.objects.get(h) &&
                  p->m_hFiredFrom == owner->m_hObject);
            CHECK(g.objects.engine_message(h, MID_UPDATE,
                                           nullptr)); // Isolated original callback, not forced production boss AI.
            CHECK(g.server.explosion_models.active.size() == 1 && g.server.explosion_lights.active.size() == 1);
            CHECK(g.server.scene_explosions.back().voice && g.server.scene_explosions.back().model_error.empty());
            const auto& fx = g.server.explosion_models.active[0];
            ModelView view;
            CHECK(view.add(fx.model.get(), fx.animation, fx.skin_path, {fx.settings.position, fx.rotation}, fx.scale,
                           true, false, {}, "shockwave", g.resources, error, {}, fx.alpha, true));
            CHECK(view.entries.size() == 1 && view.entries[0].error.empty() && !view.world_draws.empty());
            std::cout << "Private loop " << i << ": actual windfire/explode WAV, " << view.mesh.vertices.size() / 3
                      << " model triangles, deliveries " << p->targets.delivered
                      << ", rehearsal retains shooter/projectile.\n";
            CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && g.server.audio.voices().empty());
            CHECK(g.server.explosion_models.active.empty() && g.server.explosion_lights.active.empty() &&
                  g.server.projectile_reports.empty());
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string name = argv[1];
    return (name == "weapon"      ? weapon(argv[2])
            : name == "failure"   ? failure(argv[2])
            : name == "callbacks" ? callbacks(argv[2])
            : name == "recipe"    ? recipe(argv[2])
            : name == "ai"        ? ai(argv[2])
            : name == "private"   ? private_data(argv[2])
                                  : false)
               ? 0
               : 1;
}
