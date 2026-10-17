#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/WeaponFX.h"
#include <cmath>
#include <limits>
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .001f;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root, false) && test_blood_spurt::mount(g.resources, root, error));
        return true;
    }
    SparkSettings recipe()
    {
        SparkSettings s;
        s.position = {0, 100, 0};
        s.direction = {0, 0, 50};
        s.color1 = {192, 0, 0};
        s.color2 = {255, 0, 0};
        s.count = 100;
        s.duration = 2;
        s.emission_radius = 5;
        s.radius = 200;
        s.gravity = -150;
        s.texture = "spritetextures/particles/blooddrop_1.dtx";
        return s;
    }
    bool formulas(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        std::minstd_rand rng(1);
        CSparksFX fx;
        auto s = recipe();
        CHECK(fx.CreateObject(s, g.resources, 10, rng, error));
        CHECK(fx.particles.size() == 100 && fx.start_time == 10 && fx.updates == 0);
        CHECK(fx.min_velocity.x == -50 && fx.max_velocity.x == 50 && fx.min_velocity.z == 0 && fx.max_velocity.z == 51);
        for (const auto& p : fx.particles)
        {
            CHECK(std::abs(p.position.x) <= 5 && std::abs(p.position.y) <= 5 && std::abs(p.position.z) <= 5);
            CHECK(p.velocity.x >= -50 && p.velocity.x <= 50 && p.velocity.z >= 0 && p.velocity.z <= 51);
            CHECK(p.color.x >= 192.f / 255 && p.color.x <= 1 && p.color.y == 0 && p.color.z == 0);
            // Later-engine AddParticles uses the same fraction for X position and red.
            CHECK(near(p.color.x, (192 + (p.position.x + 5) / 10 * 63) / 255));
        }
        const auto first = fx.particles.front();
        CHECK(fx.Update(10.5) && near(fx.alpha, .75) && fx.particles.size() == 100);
        CHECK(near(fx.particles[0].position.y, first.position.y + first.velocity.y * .5f));
        CHECK(near(fx.particles[0].velocity.y, first.velocity.y - 75));
        CHECK(fx.Update(11) && fx.Update(12) && fx.alpha == 0 && fx.particles.size() == 100);
        CHECK(fx.Update(12.1) && fx.alpha < 0 && fx.particles.empty()); // Source class itself does not auto-remove.
        fx.want_remove = true;
        CHECK(!fx.Update(12.2) && fx.error.empty());
        s.direction = {-.05f, 0, 50};
        CHECK(fx.CreateObject(s, g.resources, 0, rng, error));
        CHECK(fx.max_velocity.x > 0); // Normalized X is still inside the strict +/- .001 interval.
        s.direction = {-.051f, 0, 50};
        CHECK(fx.CreateObject(s, g.resources, 0, rng, error) && fx.max_velocity.x == 0);
        s.direction = {0, -50, 0};
        CHECK(fx.CreateObject(s, g.resources, 0, rng, error) && fx.max_velocity.y == 0 && fx.min_velocity.y == -51);
        // Detached manager retires invisible systems only AFTER duration, IDs survive clear.
        CHECK(g.server.sparks.publish(recipe(), "lifetime", g.resources, 0, error) && error.empty());
        const auto id = g.server.sparks.active.front().id;
        CHECK(g.server.sparks.advance(1) && g.server.sparks.advance(2) && g.server.sparks.active.size() == 1);
        CHECK(g.server.sparks.advance(2.1) && g.server.sparks.active.empty() && g.server.sparks.retired == 1);
        CHECK(g.server.sparks.reports.back().ended);
        g.server.sparks.clear();
        CHECK(g.server.sparks.publish(recipe(), "reload", g.resources, 0, error) && g.server.sparks.active[0].id > id);
        g.server.sparks.clear();
        CHECK(g.server.sparks.publish(recipe(), "30 Hz rounding", g.resources, 0, error));
        double time = 0;
        for (int i = 0; i < 60; ++i)
        {
            time += 1.0 / 30;
            CHECK(g.server.sparks.advance(time));
        }
        // Double clock can be past end while float particle lifetimes retain a sub-microsecond tail.
        CHECK(!g.server.sparks.active.empty() && g.server.sparks.active[0].alpha < 0);
        SpriteView view;
        CHECK(update_sprite_scene(g, view, error) && !view.entries.empty());
        for (const auto& e : view.entries)
            CHECK(e.error.empty() && e.alpha == 0);
        CHECK(g.server.sparks.advance(time + 1.0 / 30) && g.server.sparks.active.empty());
        return true;
    }
    bool selection(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        auto source = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(source);
        CWeaponFX fx;
        fx.m_nAmmoType = AMMO_BULLET;
        fx.m_vDestPos = {0, 100, 0};
        std::minstd_rand rng(3);
        const auto saved = rng;
        fx.m_eSurfaceType = SURFTYPE_STONE;
        CHECK(fx.CreateSparks(g.server, source, {0, 1, 0}, 20, false, rng) && !fx.sparks_requested && rng == saved);
        CHECK(!fx.CreateSparks(g.server, source, {0, 1, 0}, 20, true, rng) && fx.sparks_requested &&
              !fx.spark_error.empty());
        fx.m_eSurfaceType = SURFTYPE_FLESH;
        CHECK(fx.CreateSparks(g.server, source, {0, 0, 2}, 24.9f, false, rng));
        auto s = g.server.sparks.active.back().settings;
        CHECK(s.count == 99 && s.direction.z == 50 && s.gravity == -150 && s.emission_radius == 5 && s.duration == 2);
        CHECK(fx.CreateSparks(g.server, source, {0, 0, 1}, 500, false, rng) &&
              g.server.sparks.active.back().settings.count == 200);
        CHECK(fx.CreateSparks(g.server, source, {0, 0, 1}, 0, false, rng) &&
              g.server.sparks.active.back().particles.empty());
        fx.m_eSurfaceType = SURFTYPE_METAL;
        // Add the metal resource to the synthetic archive only; source material colors remain unchanged.
        std::vector<std::uint8_t> bytes;
        CHECK(g.resources.read("spritetextures/particles/blooddrop_1.dtx", bytes, error));
        test_player::Directory dir;
        dir.children["spritetextures"].children["particles"].files["particle2.dtx"] = bytes;
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        {
            std::ofstream out(root / "metal.rez", std::ios::binary);
            out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        }
        CHECK(g.resources.mount(root / "metal.rez", error));
        bool big = false, small = false;
        for (unsigned i = 0; i < 100 && !(big && small); ++i)
        {
            g.server.sparks.clear();
            CHECK(fx.CreateSparks(g.server, source, {0, 1, 0}, 49.9f, false, rng));
            s = g.server.sparks.active.back().settings;
            CHECK(s.direction.y == 75 && s.emission_radius == .3f && s.radius == 175 && s.gravity == -150);
            if (fx.sparks_big)
            {
                CHECK(s.count == 249 && s.duration == 2);
                big = true;
            }
            else
            {
                CHECK(s.count == 99 && s.duration == .5f);
                small = true;
            }
        }
        CHECK(big && small);
        CHECK(fx.CreateSparks(g.server, source, {0, 1, 0}, 50, false, rng));
        CHECK(g.server.sparks.active.back().settings.count == (fx.sparks_big ? 250u : 100u));
        CHECK(g.objects.remove(source) && g.objects.finish_frame() && !g.server.sparks.active.empty());
        CHECK(!fx.CreateSparks(g.server, source, {0, 1, 0}, 50, false, rng) && !fx.spark_error.empty());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        auto s = recipe();
        CHECK(!g.server.PublishSparks(nullptr, s, error) && !error.empty());
        for (int bad = 0; bad < 7; ++bad)
        {
            auto v = s;
            if (bad == 0)
                v.direction = {};
            if (bad == 1)
                v.duration = 0;
            if (bad == 2)
                v.count = 256;
            if (bad == 3)
                v.color1.x = 256;
            if (bad == 4)
                v.gravity = std::numeric_limits<float>::infinity();
            if (bad == 5)
                v.texture = "missing.dtx";
            if (bad == 6)
                v.position.y = std::numeric_limits<float>::quiet_NaN();
            CHECK(g.server.sparks.publish(v, "invalid", g.resources, 0, error) && !error.empty() &&
                  g.server.sparks.active.empty());
        }
        s.count = 250;
        for (int i = 0; i < 65; ++i)
            CHECK(g.server.sparks.publish(s, "capacity", g.resources, 0, error) && error.empty());
        CHECK(g.server.sparks.live_particles() == 16250);
        CHECK(g.server.sparks.publish(s, "overflow", g.resources, 0, error) && !error.empty());
        CHECK(g.server.sparks.active.size() == 65 && g.server.sparks.reports.size() == 64);
        g.server.sparks.clear();
        s.count = 0;
        for (int i = 0; i < 128; ++i)
            CHECK(g.server.sparks.publish(s, "empty capacity", g.resources, 0, error) && error.empty());
        CHECK(g.server.sparks.publish(s, "empty overflow", g.resources, 0, error) && !error.empty());
        g.server.sparks.clear();
        s = recipe();
        CHECK(g.server.sparks.publish(s, "clock", g.resources, 1, error) && error.empty());
        CHECK(!g.server.sparks.advance(.5) && !g.server.sparks.update_error.empty() && g.server.sparks.active.empty());
        CHECK(g.server.sparks.publish(s, "decode", g.resources, 0, error));
        g.server.sparks.active[0].settings.texture = "missing.dtx";
        SpriteView view;
        CHECK(update_sprite_scene(g, view, error));
        CHECK(!view.entries.empty() && !view.entries[0].error.empty());
        return true;
    }
    bool private_world(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        std::uint64_t last = 0;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.objects.clear() && test_blood_spurt::floor(g, error));
            auto target = g.objects.create(
                g.objects.find_class("CultistAI"), {}, Creation::World,
                {{"Pos", DVector{0, 100, 0}}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string("BERETTA")}});
            auto owner = g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                                          {{"Pos", DVector{-200, 100, 0}},
                                           {"AIState", std::string("IDLE")},
                                           {"AIWeapon1", std::string("BERETTA")}});
            auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(target));
            auto* b = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(owner));
            CHECK(a && b && a->RuntimeReady() && b->RuntimeReady());
            a->m_damage.SetArmorPoints(0);
            a->m_damage.SetHitPoints(10000);
            b->m_InventoryMgr.damage_multiplier = 3;
            auto* weapon = b->m_InventoryMgr.CurrentWeapon();
            CHECK(weapon);
            const auto center = g.objects.get(target)->state.m_Pos;
            CHECK(weapon->Fire({vector_add(center, {-200, 0, 0}), {1, 0, 0}, 42, WEAP_BERETTA, false, false}) ==
                  FireResult::Fired);
            CHECK(weapon->last_shot.damage_delivered && !weapon->impacts.empty());
            const auto& hit = weapon->impacts.front();
            CHECK(hit.hit.object == target && hit.effect.spark_event && hit.effect.spark_error.empty());
            CHECK(g.server.sparks.active.size() == 1);
            CHECK(g.server.sparks.active[0].settings.count == unsigned(weapon->last_shot.damage * 4));
            CHECK(a->m_damage.m_fHitPoints < 10000 && g.server.sparks.active[0].id > last);
            last = g.server.sparks.active[0].id;
            // Metal recipe also decodes the real particle2 asset; all 3 flesh variants decode independently.
            std::minstd_rand rng(7);
            CWeaponFX fx;
            fx.m_vDestPos = {0, 100, 0};
            fx.m_nAmmoType = AMMO_BULLET;
            fx.m_eSurfaceType = SURFTYPE_METAL;
            CHECK(fx.CreateSparks(g.server, owner, {0, 1, 0}, 50, false, rng));
            for (unsigned i = 1; i <= 3; ++i)
            {
                auto s = recipe();
                s.texture = "spritetextures/particles/blooddrop_" + std::to_string(i) + ".dtx";
                CHECK(g.server.PublishSparks(owner, s, error) && error.empty());
            }
            SpriteView view;
            CHECK(update_sprite_scene(g, view, error));
            unsigned visible = 0;
            for (const auto& e : view.entries)
                if (e.spark_system)
                {
                    CHECK(e.error.empty() && e.width > 0);
                    ++visible;
                }
            CHECK(visible == g.server.sparks.live_particles());
            const auto updates = g.server.sparks.active[0].updates;
            CHECK(update_sprite_scene(g, view, error) && g.server.sparks.active[0].updates == updates);
            CHECK(g.objects.remove(owner) && g.objects.remove(target) && g.objects.finish_frame());
            CHECK(g.server.sparks.active.size() == 5);
            for (int i = 0; i < 21; ++i)
                CHECK(g.objects.advance(.1) && g.server.sparks.advance(g.objects.time()));
            CHECK(g.server.sparks.active.empty() && g.server.sparks.failed == 0);
            std::cout << "Private cycle " << cycle << ": original Beretta damage -> " << visible
                      << " decoded sparks; detached removal and expiry\n";
            CHECK(g.unload_world() && g.server.sparks.reports.empty() && !g.server.message_count() &&
                  !g.server.string_count());
            CHECK(update_sprite_scene(g, view, error) && view.entries.empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string mode = argv[1];
    return (mode == "formulas"    ? formulas(argv[2])
            : mode == "selection" ? selection(argv[2])
            : mode == "failures"  ? failures(argv[2])
                                  : mode == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
