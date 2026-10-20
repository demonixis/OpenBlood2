#include "app/sprite_scene.h"
#include "game/original/AI_Mgr.h"
#include "shield_fixture.h"
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
    bool same(DVector a, DVector b)
    {
        return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
    }
    float draw(std::minstd_rand& rng, float a, float b)
    {
        return a + (b - a) * float(rng() - rng.min()) / float(rng.max() - rng.min());
    }
    bool recipes(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_shield::mount(r, root, error));
        for (unsigned type : {19u, 36u})
        {
            CObjectParticleFX fx;
            std::minstd_rand random(17), expected(17);
            NodePose pose{{100, 20, 30}, rotation_from_euler({0, 0, 1.57079632679f})};
            CHECK(fx.Init(type, 42, random, error));
            if (type == 19)
            {
                const DVector rotations{draw(expected, 0, .01f), draw(expected, 0, .015f), draw(expected, 0, .01f)};
                CHECK(same(fx.rotations, rotations) && fx.radius == 500 && fx.gravity == 0 && fx.duration == 5);
            }
            else
                CHECK(random == expected && fx.radius == 1750 && fx.gravity == 150 && fx.duration == 2.5f);
            CHECK(fx.CreateObject(r, pose, 100, error) && fx.Update(100, &pose, random));
            CHECK(fx.particles.empty() && fx.alpha == 1 && fx.position.x == 100);
            CHECK(fx.Update(100.002, &pose, random)); // Absolute last-add=0, not creation time.
            CHECK(fx.particles.size() == (type == 19 ? 24 : 18) && fx.bursts == 1 && fx.moving);
            CHECK(same(fx.position, type == 19 ? pose.position : DVector{125, 20, 30})); // Offset x maps to local up.
            CHECK(same(fx.particles[0].position, rotate_vector(fx.system_rotation, {fx.position_radius, 0, 0})));
            for (const auto& particle : fx.particles)
            {
                expected();
                if (type == 19)
                    expected();
                expected(); // angles then zero speed
                const float ratio = draw(expected, 0, 1);
                const auto color = vector_scale(
                    vector_add(fx.min_color, vector_scale(vector_subtract(fx.max_color, fx.min_color), ratio)),
                    1 / 255.f);
                CHECK(same(particle.color, color) && same(particle.velocity, {}) &&
                      near(particle.lifetime, draw(expected, fx.min_life, fx.max_life)));
                CHECK(near(std::sqrt(vector_dot(particle.position, particle.position)), fx.position_radius));
            }
            CHECK(random == expected);
            const auto old = fx.particles[0];
            const auto turn = fx.system_rotation;
            pose.position.x += 10;
            pose.rotation = {};
            CHECK(fx.Update(100.022, &pose, random) && fx.bursts == 1 && random == expected);
            CHECK(same(fx.particles[0].position, old.position) && near(fx.particles[0].velocity.y, fx.gravity * .02f));
            CHECK(!same(turn.m_Vec, fx.system_rotation.m_Vec)); // per-callback, not dt-scaled
            CHECK(same(fx.position, type == 19 ? pose.position : DVector{110, -5, 30}));
            CHECK(fx.Update(101, &pose, random) && fx.bursts == 2); // one overdue burst, no catch-up
        }
        std::cout << "Shield sphere24 and powerup ring18: recipe/RNG, cached-right, local-up offset, whole-system "
                     "follow/rotation and no catch-up verified.\n";
        return true;
    }
    bool clock(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_shield::mount(r, root, error));
        NodePose pose;
        std::minstd_rand random(1);
        for (unsigned type : {19u, 36u})
        {
            CObjectParticleFX fx;
            CHECK(fx.Init(type, 1, random, error) && fx.CreateObject(r, pose, 10, error));
            CHECK(fx.Update(10 + fx.duration + 1, &pose, random) &&
                  fx.first_update == false); // first return before expiry
            CHECK(!fx.Update(10 + fx.duration + 1.002, &pose, random) && fx.error.empty());
            CHECK(fx.Init(type, 1, random, error) && fx.CreateObject(r, pose, 0, error) && fx.Update(0, &pose, random));
            CHECK(fx.Update(fx.duration - .3, &pose, random) && fx.alpha == 1 && !fx.ramping);
            CHECK(fx.Update(fx.duration - .25, &pose, random) && fx.ramping && fx.alpha == fx.requested_alpha);
            CHECK(fx.Update(fx.duration - .125, &pose, random) && near(fx.alpha, fx.requested_alpha * .5f));
            CHECK(fx.Update(fx.duration, &pose, random) && fx.alpha == 0 && !fx.particles.empty());
            CHECK(!fx.Update(fx.duration + .002, &pose, random) && fx.error.empty());
        }
        std::cout << "Delayed first-return, source alpha1 until ramp (powerup jumps to .5), exact duration final "
                     "emission and strict removal verified.\n";
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources r, missing;
        std::string error;
        CHECK(test_shield::mount(r, root, error));
        CObjectParticleFX fx;
        NodePose pose;
        std::minstd_rand random(1);
        CHECK(!fx.Init(0, 1, random, error) && !fx.Init(19, 0, random, error)); // OBJFX_NONE; absent parent.
        CHECK(fx.Init(19, 1, random, error) && !fx.CreateObject(missing, pose, 0, error));
        pose.rotation.m_Spin = 0;
        CHECK(!fx.CreateObject(r, pose, 0, error));
        pose = {};
        CHECK(fx.CreateObject(r, pose, 0, error) && fx.Update(0, &pose, random));
        auto previous = random;
        CHECK(!fx.Update(.11, &pose, random, 23) && !fx.error.empty() && fx.particles.empty() && random == previous);
        CHECK(!fx.Update(-1, &pose, random) && !fx.Update(std::numeric_limits<double>::infinity(), &pose, random));
        GameSession g;
        CHECK(test_player::setup(g, root / "session") && test_shield::mount(g.resources, root / "session", error));
        auto h = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(h);
        CHECK(!g.server.PublishObjectParticles(nullptr, 19, error) && !error.empty());
        CHECK(!g.server.PublishObjectParticles(h, 1, error) && !error.empty());
        auto& manager = g.server.object_particles;
        for (unsigned i = 0; i < 128; ++i)
            CHECK(g.server.PublishObjectParticles(h, 19, error));
        CHECK(!g.server.PublishObjectParticles(h, 19, error) && manager.reports.size() == 64);
        CHECK(manager.advance(0, g.server) && manager.advance(.11, g.server));
        CHECK(manager.advance(.22, g.server));
        CHECK(!manager.advance(.33, g.server) && manager.failed > 3 && manager.particle_count() <= 8192);
        CHECK(!manager.update_error.empty());
        CHECK(g.objects.remove(h));
        CHECK(!g.server.PublishObjectParticles(h, 19, error)); // pending removal is already stale
        CHECK(g.objects.finish_frame() && manager.advance(.34, g.server) && manager.active.empty());
        std::cout << "Invalid recipe/owner/pose/clock/resources and preflight budgets fail; history bounded, pending "
                     "owners rejected, no live overflow.\n";
        return true;
    }
    bool weapon(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && test_shield::mount(g.resources, root, error));
        auto* owner = test_energy::actor(g, {100, 50, 0});
        auto* target = test_energy::actor(g, {0, 50, 25});
        auto* w = test_shield::equip(owner);
        CHECK(w && target);
        CHECK(w->data.m_nType == 41 && w->data.m_nFireType == 0 && w->CheckAmmo() == 1 && !w->data.m_szProjectileClass);
        CHECK(w->data.m_fRange == 75 && w->data.m_fReloadTime == 1 && w->data.m_fAltReloadTime == 0);
        CHECK(w->ShowHandModel(true) && !w->m_hHandModel && w->FireVector() == FireResult::Invalid);
        const auto armor = owner->damage.m_fArmorPoints;
        g.server.collision_ready = false; // Pure shield source branch requires no collision query.
        for (bool alt : {false, true})
        {
            CHECK(w->Fire(test_shield::shot(alt)) == FireResult::Fired && w->effect && w->effect_error.empty());
            CHECK(!w->last_shot.damage_delivered && w->last_shot.collision == SegmentResult::Unavailable);
        }
        CHECK(owner->damage.m_fArmorPoints == armor && target->damage.m_fHitPoints == 1000 && w->sounds_played == 2);
        CHECK(g.server.object_particles.active.size() == 2 && w->CheckAmmo() == 1);
        g.server.client_resources = nullptr;
        CHECK(w->Fire(test_shield::shot()) == FireResult::Fired && !w->effect && !w->effect_error.empty());
        CHECK(g.server.object_particles.failed == 1 && owner->damage.m_fArmorPoints == armor);
        g.server.client_resources = &g.resources;
        g.server.collision_ready = true;
        CHECK(g.enter_player(error));
        CHECK(g.server_shell.Player(g.local_client)->m_InventoryMgr.ObtainWeapon(41, 4) ==
              InventoryResult::NotAvailable);
        std::cout << "Both shield modes publish original FX/audio, no collision/damage/armor/ammo mutation; failed FX "
                     "remains visible, player acquisition refused.\n";
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && test_shield::mount(g.resources, root, error));
        auto h = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(h);
        auto first = g.server.PublishObjectParticles(h, 36, error);
        CHECK(first);
        CHECK(g.step(.02) && g.step(.11));
        auto& m = g.server.object_particles;
        CHECK(m.particle_count() == 18 && m.active[0].effect.updates == 2);
        SpriteView scene;
        const auto updates = m.active[0].effect.updates;
        CHECK(update_sprite_scene(g, scene, error) && update_sprite_scene(g, scene, error));
        CHECK(m.active[0].effect.updates == updates);
        unsigned drawn = 0;
        for (const auto& e : scene.entries)
            if (e.object_effect == first)
            {
                CHECK(e.error.empty());
                ++drawn;
            }
        CHECK(drawn == 18);
        CHECK(g.objects.remove(h) && g.step(.02) && m.active.empty());
        CHECK(update_sprite_scene(g, scene, error));
        for (const auto& e : scene.entries)
            CHECK(!e.object_effect);
        CHECK(g.unload_world() && m.reports.empty());
        h = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(g.server.PublishObjectParticles(h, 19, error) > first);
        CHECK(m.advance(g.objects.time(), g.server));
        CHECK(!m.advance(-1, g.server) && m.active.empty() && !m.reports.back().error.empty());
        std::cout << "UI-independent session updates, read-only repeated render, removal/unload, nonreused IDs and "
                     "durable failure inspection verified.\n";
        return true;
    }
    bool ai(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && test_shield::mount(g.resources, root, error));
        auto* target = test_energy::actor(g, {0, 50, 100});
        CHECK(target);
        auto h = g.objects.create(g.objects.find_class("AI_Mgr"));
        auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
        CHECK(a);
        CHECK(a->m_InventoryMgr.Init(h, nullptr) && a->m_InventoryMgr.ObtainWeapon(41, 0) == InventoryResult::Ok);
        auto model = std::make_shared<Model>();
        CHECK(read_model(test_model::abc().b, *model, error));
        model->nodes[1].name = "r_gun";
        for (auto& animation : model->animations)
            for (auto& key : animation.keys)
                key.text.clear();
        model->animations[0].keys = {{0, {}, {}, "[fire_key:1]"}, {100, {}, {}, ""}};
        auto* r = g.objects.get(h);
        r->model = model;
        r->state.m_ObjectType = OT_MODEL;
        r->state.m_Pos = {0, 50, 0};
        r->state.m_Flags |= FLAG_MODELKEYS;
        a->m_vTargetPos = {0, 50, 100};
        CHECK(a->SetNewTarget(target->m_hObject));
        g.server.SetModelAnimation(h, 0);
        CHECK(g.server.advance_models(.01) && a->fire_shots == 1 && g.server.object_particles.active.size() == 1);
        CHECK(target->damage.m_fHitPoints == 1000);
        std::cout << "Original generic AI FIRE_KEY reaches actual shield publication with no target damage; boss AI "
                     "not claimed.\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        std::uint64_t last = 0;
        for (unsigned loop = 0; loop < 3; ++loop)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* owner = test_energy::actor(g, {0, 170, -2400});
            auto* w = test_shield::equip(owner);
            CHECK(w);
            CHECK(w->Fire(test_shield::shot()) == FireResult::Fired && w->effect > last && w->sounds_played == 1);
            last = w->effect;
            CHECK(g.server.PublishObjectParticles(owner->m_hObject, 36, error));
            auto& m = g.server.object_particles;
            const auto now = g.objects.time();
            CHECK(m.advance(now, g.server) && m.advance(now + .11, g.server));
            CHECK(m.particle_count() == 42);
            const auto first = m.active[0].effect.particles[0];
            WorldMessage message;
            CHECK(original_world_request("01_SubwayA", 0, 1, 1, message));
            CHECK(g.rehearse_destination(message, "", error) && m.active.size() == 2 && m.particle_count() == 42);
            CHECK(same(m.active[0].effect.particles[0].position, first.position));
            SpriteView scene;
            CHECK(update_sprite_scene(g, scene, error));
            unsigned rendered = 0;
            std::size_t partial = 0;
            for (const auto& entry : scene.entries)
                if (entry.object_effect)
                {
                    CHECK(entry.error.empty());
                    ++rendered;
                    partial += entry.partial_alpha;
                }
            CHECK(rendered == 42 && partial > 0);
            CHECK(m.advance(now + 2.501, g.server) && m.active.size() == 1);
            CHECK(m.advance(now + 5.001, g.server) && m.active.empty());
            std::cout << "Private loop " << loop
                      << ": actual shield WAV, 42 decoded shield/powerup quads, source preserved across destination "
                         "rehearsal; exact independent expiry.\n";
            CHECK(g.unload_world() && m.reports.empty() && g.server.audio.voices().empty());
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 &&
                  CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    return (name == "recipes"    ? recipes(argv[2])
            : name == "clock"    ? clock(argv[2])
            : name == "failures" ? failures(argv[2])
            : name == "weapon"   ? weapon(argv[2])
            : name == "session"  ? session(argv[2])
            : name == "ai"       ? ai(argv[2])
            : name == "private"  ? private_data(argv[2])
                                 : false)
               ? 0
               : 1;
}
