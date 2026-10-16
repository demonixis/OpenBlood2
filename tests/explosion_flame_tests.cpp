#include "app/sprite_scene.h"
#include "explosion_flame_fixture.h"
#include "game/original/ClientExplosionSFX.h"
#include <cmath>
#include <limits>
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
        return std::abs(a - b) < .0001f;
    }
    bool zones(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_explosion_flame::mount(r, root, error));
        ExplosionFlameSettings s;
        s.count = 2;
        s.position_radius = 9;
        s.velocity = 10;
        s.lifetimes = {1, 2, 3};
        s.life_offsets = {.1f, .2f, .3f};
        s.colors = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
        CExplosionFlameFX fx;
        std::minstd_rand random(17), expected(17);
        CHECK(fx.Init(s, error) && fx.CreateObject(r, 4, random, error));
        CHECK(fx.particles.size() == 12 && fx.bursts == 1 && fx.emitted == 12 && fx.add_time == s.delay);
        unsigned k = 0;
        for (unsigned zone = 0; zone < 3; ++zone)
            for (unsigned i = 0; i < 2 * (zone + 1); ++i)
            {
                float t[6];
                for (auto& v : t)
                    v = float(expected() - std::minstd_rand::min()) /
                        float(std::minstd_rand::max() - std::minstd_rand::min());
                const auto& v = fx.particles[k++];
                float radius = 3 * (zone + 1);
                CHECK(near(v.position.x, -radius + 2 * radius * t[0]) &&
                      near(v.position.y, -radius + 2 * radius * t[1]) &&
                      near(v.position.z, -radius + 2 * radius * t[2]));
                CHECK(near(v.velocity.x, -10 + 20 * t[3]) && near(v.velocity.y, -10 + 20 * t[4]) &&
                      near(v.velocity.z, -10 + 20 * t[5]));
                CHECK(near(v.lifetime, s.lifetimes[zone] - s.life_offsets[zone] + 2 * s.life_offsets[zone] * t[3]));
                CHECK(v.color.x == s.colors[zone].x && v.color.y == s.colors[zone].y && v.color.z == s.colors[zone].z);
            }
        CHECK(random() == expected());
        // Authored normal/ramps do not change emission, speed or lifetime.
        s.normal = {1, 0, 0};
        s.ramp_up = 500;
        s.ramp_down = 400;
        s.ramp_flags = 7;
        CExplosionFlameFX other;
        std::minstd_rand same(17);
        CHECK(other.Init(s, error) && other.CreateObject(r, 4, same, error));
        for (unsigned i = 0; i < 12; ++i)
            CHECK(near(fx.particles[i].velocity.x, other.particles[i].velocity.x) &&
                  near(fx.particles[i].position.y, other.particles[i].position.y) &&
                  near(fx.particles[i].lifetime, other.particles[i].lifetime));
        return true;
    }
    bool lifecycle(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_explosion_flame::mount(r, root, error));
        ExplosionFlameSettings s;
        s.count = 1;
        s.delay = .25f;
        s.duration = 2;
        s.alpha = .4f;
        s.lifetimes = {10, 10, 10};
        s.gravity = 10;
        std::minstd_rand random(1);
        CExplosionFlameFX fx;
        const float fades[]{.4f, .3f, .85f, .1f, .55f, .2f};
        for (unsigned mode = 0; mode <= 5; ++mode)
        {
            s.fade = mode;
            CHECK(fx.Init(s, error) && fx.CreateObject(r, 10, random, error));
            CHECK(fx.alpha == (mode ? 1 : s.alpha) && fx.particles.size() == 6);
            CHECK(fx.Update(10.5, random) && near(fx.alpha, fades[mode]) && fx.bursts == 2 && fx.add_time == .5f);
            CHECK(fx.particles[0].position.y == 0 && fx.particles[0].velocity.y == 5 &&
                  fx.particles[0].lifetime == 9.5f);
            CHECK(fx.particles.back().lifetime == 10 && fx.particles.back().velocity.y == 0);
            CHECK(fx.Update(10.5005, random) && fx.bursts == 2 && fx.updates == 1); // particle clocks still run.
            CHECK(fx.particles[0].lifetime < 9.5f);
            CHECK(fx.Update(10.502, random) && fx.bursts == 3 &&
                  fx.add_time == .75f); // backlog one burst per callback.
            CHECK(fx.Update(11.5, random) && fx.bursts == 4 && fx.add_time == 1);
            if (mode == 5)
                CHECK(near(fx.alpha, .2f));
            CHECK(!fx.Update(12, random) && fx.error.empty() && fx.bursts == 5 &&
                  fx.particles.size() == 30); // add before expiry.
        }
        s.delay = 0;
        s.fade = 0;
        CHECK(fx.Init(s, error) && fx.CreateObject(r, 0, random, error) && fx.Update(0, random) && fx.bursts == 2);
        CHECK(fx.Update(.0005, random) && fx.bursts == 2);
        CHECK(fx.Update(.001, random) && fx.bursts == 3 && fx.add_time == 0);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_explosion_flame::mount(r, root, error));
        CExplosionFlameFX fx;
        std::minstd_rand random(1), untouched(1);
        CHECK(!fx.CreateObject(r, 0, random, error) && !fx.Update(0, random));
        for (unsigned i = 0; i < 16; ++i)
        {
            ExplosionFlameSettings s;
            switch (i)
            {
                case 0:
                    s.count = 0;
                    break;
                case 1:
                    s.count = 257;
                    break;
                case 2:
                    s.duration = 0;
                    break;
                case 3:
                    s.delay = -1;
                    break;
                case 4:
                    s.fade = 6;
                    break;
                case 5:
                    s.ramp_flags = 8;
                    break;
                case 6:
                    s.colors[1].z = 1.1f;
                    break;
                case 7:
                    s.life_offsets[0] = 1;
                    break;
                case 8:
                    s.lifetimes[2] = std::numeric_limits<float>::infinity();
                    break;
                case 9:
                    s.velocity = -1;
                    break;
                case 10:
                    s.position_radius = 10001;
                    break;
                case 11:
                    s.alpha = -1;
                    break;
                case 12:
                    s.gravity = 10001;
                    break;
                case 13:
                    s.radius = -1;
                    break;
                case 14:
                    s.texture = std::string("a\0b", 3);
                    break;
                case 15:
                    s.normal.x = std::numeric_limits<float>::quiet_NaN();
                    break;
            }
            CHECK(!fx.Init(s, error) && !error.empty());
        }
        ExplosionFlameSettings s;
        s.count = 1;
        s.delay = 0;
        s.lifetimes = {10, 10, 10};
        CHECK(fx.Init(s, error) && !fx.CreateObject(r, -1, random, error));
        CHECK(!fx.CreateObject(r, 0, random, error, 5) && fx.particles.empty() && random == untouched);
        CHECK(fx.CreateObject(r, 0, random, error, 6));
        untouched = random;
        CHECK(!fx.Update(.1, random, 11) && !fx.error.empty() && fx.particles.size() == 6 && fx.bursts == 1 &&
              random == untouched);
        CHECK(!fx.Update(-1, random) && !fx.Update(std::numeric_limits<double>::infinity(), random));
        s.texture = "missing.dtx";
        CHECK(fx.Init(s, error) && !fx.CreateObject(r, 0, random, error));
        ExplosionFlames fs;
        std::vector<ExplosionFlameSettings> out;
        for (unsigned type : {0u, 1u, 2u, 3u, 6u})
            CHECK(original_explosion_flames(type, {}, {0, 1, 0}, out, error) && out.empty());
        CHECK(!original_explosion_flames(8, {}, {}, out, error) && out.empty());
        CHECK(!original_explosion_flames(99, {}, {0, 1, 0}, out, error) && out.empty());
        CHECK(original_explosion_flames(8, {10, 20, 30}, {0, 2, 0}, out, error) && out.size() == 1);
        CHECK(out[0].position.y == 50 && out[0].count == 4 && out[0].velocity == 125 && out[0].ramp_flags == 1 &&
              out[0].duration == 2.5f);
        for (unsigned i = 0; i < 64; ++i)
            CHECK(fs.publish(8, {}, {0, 1, 0}, "burst", r, 0, error) && error.empty());
        CHECK(fs.live_particles() == 1536);
        CHECK(fs.publish(8, {}, {0, 1, 0}, "full", r, 0, error) && !error.empty() && fs.active.size() == 64 &&
              fs.reports.size() == 64);
        CHECK(fs.advance(.33) && fs.live_particles() == 3072);
        CHECK(!fs.advance(.66) && fs.failed > 1 && !fs.update_error.empty() && fs.live_particles() <= 4096);
        bool retained = false;
        for (const auto& report : fs.reports)
            retained |= !report.error.empty();
        CHECK(retained);
        auto id = fs.active.back().id, event = fs.reports.back().id;
        fs.clear();
        CHECK(fs.publish(8, {}, {0, 1, 0}, "new", r, 1, error) && error.empty() && fs.active.front().id > id &&
              fs.reports.front().id > event);
        CHECK(!fs.advance(0) && fs.active.empty() && fs.failed == 1);
        fs.clear();
        CHECK(fs.reports.empty() && fs.update_error.empty() && !fs.failed);
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize() && test_explosion_flame::mount(g.resources, root, error));
        auto h = test_explosion_flame::create(g);
        CHECK(h);
        DVector moved{100, 200, 300};
        CHECK(g.server.TeleportObject(h, &moved) == DE_OK);
        CHECK(test_explosion_flame::trigger(g, h) && test_explosion_flame::trigger(g, h));
        auto& fs = g.server.explosion_flames;
        CHECK(fs.active.size() == 2 && fs.live_particles() == 48 && !fs.failed);
        CHECK(fs.active[0].settings.position.x == 0 && fs.active[0].settings.position.y == 0 &&
              fs.active[0].settings.position.z == 60);
        const auto& report = g.server.scene_explosions.back();
        CHECK(report.flame_event && report.flame_error.empty() && !report.model_error.empty() &&
              !report.client_fx_available && report.targets.null_damager && !report.targets.delivered);
        CHECK(g.step(.11) && !g.objects.get(h) && fs.active.size() == 2);
        SpriteView scene;
        CHECK(update_sprite_scene(g, scene, error));
        unsigned drawn = 0;
        for (const auto& e : scene.entries)
            if (e.flame_system)
            {
                CHECK(e.error.empty() && e.range != UINT32_MAX && e.alpha < 1);
                ++drawn;
            }
        CHECK(drawn == 48);
        const auto updates = fs.active[0].updates;
        CHECK(update_sprite_scene(g, scene, error) && fs.active[0].updates == updates);
        CHECK(!g.server.PublishExplosionFlames(h, 8, {}, {0, 1, 0}, error) && !error.empty());
        CHECK(g.step(3) && fs.active.empty() && fs.expired == 2);
        CHECK(g.unload_world() && fs.reports.empty() && !g.server.string_count() && !g.server.message_count());
        return true;
    }
    bool private_world(const std::filesystem::path& root)
    {
        GameSession g;
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            // Explicit probe: this preset is not among SubwayA's nine grenade emitters.
            auto h = test_explosion_flame::create(g);
            CHECK(h && test_explosion_flame::trigger(g, h));
            auto& fs = g.server.explosion_flames;
            CHECK(fs.active.size() == 1 && fs.active[0].id > previous);
            previous = fs.active[0].id;
            const auto& report = g.server.scene_explosions.back();
            CHECK(report.flame_error.empty() && report.sprite_error.empty() && report.model_error.empty() &&
                  report.light_error.empty());
            CHECK(g.server.explosion_sprites.reports.back().unavailable == "light illumination");
            unsigned peak = 0;
            std::uint64_t bursts = 0;
            double dt = cycle == 0 ? .02 : cycle == 1 ? 1. / 60 : .05;
            for (unsigned frame = 0; frame < static_cast<unsigned>(3 / dt); ++frame)
            {
                CHECK(g.step(dt));
                SpriteView scene;
                CHECK(update_sprite_scene(g, scene, error));
                unsigned drawn = 0;
                for (const auto& e : scene.entries)
                    if (e.flame_system)
                    {
                        CHECK(e.error.empty() && e.range != UINT32_MAX &&
                              e.frame_path == "SpriteTextures\\teslaprime_3.dtx");
                        ++drawn;
                    }
                CHECK(drawn == fs.live_particles());
                peak = std::max(peak, drawn);
                if (!fs.active.empty())
                    bursts = fs.active[0].bursts;
            }
            CHECK(fs.active.empty() && fs.expired == 1 && !fs.failed && peak > 24 && bursts == 8);
            std::cout << "SubwayA diagnostic Tesla cycle " << cycle << ": peak " << peak
                      << ", eight bursts observed, complete expiry\n";
            CHECK(g.unload_world() && fs.reports.empty() && g.objects.snapshot().empty());
            CHECK(!g.server.string_count() && !g.server.message_count());
        }
        std::vector<std::uint8_t> bytes;
        World cathedral;
        CHECK(g.resources.read("worlds/07_cathedral.dat", bytes, error) && read_world(bytes, cathedral, error));
        unsigned authored = 0;
        for (const auto& o : cathedral.objects)
            if (o.class_name == "CClientExplosionSFX")
            {
                Properties props;
                for (const auto& v : o.properties)
                    props.emplace(v.name, v.value);
                auto h = g.objects.create(g.objects.find_class(o.class_name), {}, Creation::World, props);
                CHECK(h);
                auto* emitter = dynamic_cast<CClientExplosionSFX*>(g.server.HandleToObject(h));
                CHECK(emitter);
                if (emitter->type == 8)
                {
                    ++authored;
                    CHECK(test_explosion_flame::trigger(g, h));
                    const auto& f = g.server.explosion_flames.active.back();
                    CHECK(near(f.settings.position.y, emitter->position.y + 30) && f.particles.size() == 24);
                    CHECK(g.server.scene_explosions.back().flame_error.empty());
                    std::cout << "Authored Cathedral Tesla: " << g.server.GetObjectName(h)
                              << ", 24 initial particles\n";
                }
                CHECK(g.objects.remove(h) && g.objects.finish_frame());
            }
        CHECK(authored == 3 && g.server.explosion_flames.active.size() == 3 && g.step(.35));
        SpriteView scene;
        CHECK(update_sprite_scene(g, scene, error));
        unsigned drawn = 0;
        for (const auto& e : scene.entries)
            if (e.flame_system)
            {
                CHECK(e.error.empty() && e.range != UINT32_MAX);
                ++drawn;
            }
        CHECK(drawn == 144 && g.step(3) && g.server.explosion_flames.active.empty());
        CHECK(g.unload_world() && g.server.explosion_flames.reports.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string mode = argv[1];
    return (mode == "zones"       ? zones(argv[2])
            : mode == "lifecycle" ? lifecycle(argv[2])
            : mode == "failures"  ? failures(argv[2])
            : mode == "session"   ? session(argv[2])
                                  : mode == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
