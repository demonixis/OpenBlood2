#include "app/sprite_scene.h"
#include "explosion_ring_fixture.h"
#include "game/original/ClientExplosionSFX.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
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
        return std::abs(a - b) < .0001f;
    }
    bool command(GameSession& g, HOBJECT h)
    {
        auto s = g.server.CreateString("TRIGGER");
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = m && g.server.WriteToMessageHString(m, s) == DE_OK && g.server.EndMessage(m) == DE_OK;
        g.server.FreeString(s);
        return ok;
    }
    bool motion()
    {
        std::string error;
        std::vector<Particle> ps{{{1, 2, 3}, {4, 5, 6}, 1}};
        CHECK(advance_particles(ps, -10, .5, error));
        CHECK(ps.size() == 1 && near(ps[0].position.x, 3) && near(ps[0].position.y, 4.5f));
        CHECK(near(ps[0].velocity.y, 0) && near(ps[0].lifetime, .5f));
        CHECK(advance_particles(ps, -10, .5, error) && ps.size() == 1 && ps[0].lifetime == 0);
        CHECK(near(ps[0].position.y, 4.5f) && ps[0].velocity.y == -5);
        CHECK(advance_particles(ps, -10, .01, error) && ps.empty());
        ps = {{{}, {1, 0, 0}, 1}, {{}, {}, -1}};
        CHECK(!advance_particles(ps, 0, .1, error) && !error.empty());
        CHECK(ps[0].position.x == 0 && ps[0].lifetime == 1); // Atomic validation.
        ps.pop_back();
        CHECK(!advance_particles(ps, 0, 61, error) && !advance_particles(ps, 0, -1, error));
        CHECK(!advance_particles(ps, std::numeric_limits<float>::quiet_NaN(), .1, error));
        return true;
    }
    bool lifecycle(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_explosion_ring::mount(r, root, error));
        ExplosionRingSettings s;
        s.count = 4;
        s.duration = 2;
        s.position_radius = 10;
        s.velocity = 20;
        s.gravity = -8;
        CExplosionRingFX fx;
        CHECK(fx.Init(s, error) && fx.CreateObject(r, 10, error) && fx.particles.size() == 4);
        CHECK(near(fx.particles[0].position.x, 10));
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto& a = fx.particles[i];
            const auto& b = fx.particles[(i + 1) % 4];
            CHECK(near(a.position.y, 0) && near(vector_dot(a.position, a.position), 100));
            CHECK(near(vector_dot(a.position, b.position), 0));
        }
        CHECK(fx.Update(10.5) && near(fx.particles[0].position.x, 20));
        CHECK(near(fx.particles[0].position.y, 0) && near(fx.particles[0].velocity.y, -4));
        CHECK(fx.Update(11) && near(fx.particles[0].position.y, -2));
        CHECK(!fx.Update(12) && fx.error.empty());
        s.align = 1;
        s.normal = {0, 0, 3};
        s.gravity = 0;
        CHECK(fx.Init(s, error) && fx.CreateObject(r, 0, error));
        for (const auto& a : fx.particles)
            CHECK(near(a.position.z, 0));
        s.delay = .5f;
        s.fade = 1;
        s.alpha = .6f;
        CHECK(fx.Init(s, error) && fx.CreateObject(r, 10, error) && fx.particles.empty() && fx.alpha == 0);
        CHECK(fx.Update(10.499) && fx.particles.empty());
        CHECK(fx.Update(11) && fx.start_time == 11 && fx.delay == 0 && fx.particles.size() == 4 && fx.alpha == 0);
        CHECK(near(fx.particles[0].position.x, 10) && fx.particles[0].lifetime == 2);
        CHECK(fx.Update(11.5) && near(fx.alpha, .45f) && near(fx.particles[0].position.x, 20));
        s.fade = 0;
        CHECK(fx.Init(s, error) && fx.CreateObject(r, 0, error) && fx.Update(20));
        CHECK(near(fx.alpha, .6f) && fx.start_time == 20 && fx.particles[0].lifetime == 2);
        s.delay = 0;
        s.alpha = .4f;
        s.rotation = .2f;
        s.velocity = 0;
        const float fades[] = {.4f, .3f, .85f, .1f, .55f, .2f};
        const float turns[] = {0, .2f, .15f, .8f, .05f, .4f};
        for (unsigned mode = 0; mode <= 5; ++mode)
        {
            s.fade = s.rotate = mode;
            CHECK(fx.Init(s, error) && fx.CreateObject(r, 0, error) && fx.Update(.5));
            CHECK(near(fx.alpha, fades[mode]));
            auto v = rotate_vector(fx.rotation, {1, 0, 0});
            CHECK(near(v.x, std::cos(turns[mode] * std::numbers::pi_v<float>)));
            CHECK(near(v.y, std::sin(turns[mode] * std::numbers::pi_v<float>)));
            const auto updates = fx.updates;
            CHECK(fx.Update(.5005) && fx.updates == updates);
        }
        s.rotate = 1;
        s.fade = 5;
        CHECK(fx.Init(s, error) && fx.CreateObject(r, 0, error) && fx.Update(.5) && fx.Update(1.5));
        CHECK(near(fx.alpha, .2f));
        CHECK(near(rotate_vector(fx.rotation, {1, 0, 0}).x, std::cos(.4f * std::numbers::pi_v<float>)));
        return true;
    }
    bool recipes()
    {
        std::string error;
        std::vector<ExplosionRingSettings> out;
        for (unsigned type : {0u, 1u, 2u, 3u, 6u, 8u})
        {
            std::minstd_rand random(17), expected(17);
            CHECK(original_explosion_rings(type, {10, 20, 30}, {0, 2, 0}, random, out, error));
            CHECK(out.size() == (type == 3 ? 4u : type <= 2 ? 1u : 0u));
            if (type <= 3)
            {
                CHECK(out[0].position.y == 45 && out[0].count == 32 && out[0].velocity == 400);
                CHECK(out[0].radius == (type == 0 ? 4000 : type == 2 ? 12000 : 8000));
                CHECK(out[0].duration == .5f && out[0].fade == 1 && out[0].align == 0);
            }
            if (type == 3)
            {
                auto draw = [&](float a, float b)
                {
                    return a + (b - a) * float(expected() - std::minstd_rand::min()) /
                                   float(std::minstd_rand::max() - std::minstd_rand::min());
                };
                for (unsigned i = 0; i < 3; ++i)
                {
                    const auto& s = out[i + 1];
                    CHECK(s.position.y == 30 + 15 * i && s.radius == 5000 && s.alpha == .5f && s.duration == 1.5f);
                    CHECK(near(s.velocity, draw(100, 150)) && near(s.gravity, draw(20 * i, 30 + 20 * i)) &&
                          near(s.delay, draw(0, .5f)));
                    CHECK(s.texture == "SpriteTextures\\Smoke32_" + std::to_string(4 + i) + ".dtx");
                }
            }
            CHECK(random() == expected());
        }
        std::minstd_rand random(1);
        CHECK(!original_explosion_rings(255, {}, {0, 1, 0}, random, out, error) && out.empty());
        CHECK(!original_explosion_rings(3, {}, {}, random, out, error) && out.empty());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_explosion_ring::mount(r, root, error));
        CExplosionRingFX fx;
        CHECK(!fx.CreateObject(r, 0, error) && !fx.Update(0));
        for (unsigned kind = 0; kind < 15; ++kind)
        {
            ExplosionRingSettings s;
            switch (kind)
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
                    s.rotate = 6;
                    break;
                case 6:
                    s.align = 2;
                    break;
                case 7:
                    s.normal = {};
                    break;
                case 8:
                    s.gravity = 10001;
                    break;
                case 9:
                    s.color.x = 1.1f;
                    break;
                case 10:
                    s.alpha = -1;
                    break;
                case 11:
                    s.radius = -1;
                    break;
                case 12:
                    s.texture = std::string("a\0b", 3);
                    break;
                case 13:
                    s.position.x = std::numeric_limits<float>::infinity();
                    break;
                case 14:
                    s.velocity = std::numeric_limits<float>::quiet_NaN();
                    break;
            }
            CHECK(!fx.Init(s, error) && !error.empty());
        }
        ExplosionRingSettings s;
        s.texture = "missing.dtx";
        CHECK(fx.Init(s, error) && !fx.CreateObject(r, 0, error));
        CHECK(fx.Init({}, error) && !fx.CreateObject(r, -1, error) && fx.CreateObject(r, 1, error));
        CHECK(!fx.CreateObject(r, 2, error) && !fx.Update(.5) && !fx.error.empty());
        CHECK(!fx.Update(std::numeric_limits<double>::infinity()));
        ExplosionRings rings;
        for (unsigned i = 0; i < 63; ++i)
            CHECK(rings.publish(0, {}, {0, 1, 0}, "fire", r, 0, error) && error.empty());
        CHECK(rings.publish(3, {}, {0, 1, 0}, "partial capacity", r, 0, error) && !error.empty());
        CHECK(rings.active.size() == 64 && rings.reserved_particles() == 2048 && rings.failed == 3);
        CHECK(rings.reports.back().rings.size() == 1 && rings.reports.size() == 64);
        CHECK(rings.publish(0, {}, {0, 1, 0}, "full", r, 0, error) && rings.reports.size() == 64 && !error.empty());
        const auto id = rings.active.back().id, event = rings.reports.back().id;
        CHECK(rings.advance(.5) && rings.active.empty() && rings.expired == 64);
        rings.clear();
        CHECK(rings.publish(3, {}, {0, 1, 0}, "new", r, 1, error) && error.empty());
        CHECK(rings.active.front().id > id && rings.reports.front().id > event);
        CHECK(!rings.advance(0) && rings.active.empty() && rings.failed == 4 && !rings.update_error.empty());
        rings.clear();
        CHECK(rings.reports.empty() && !rings.failed && rings.update_error.empty());
        Resources partial;
        CHECK(test_explosion_ring::mount(partial, root / "partial", error, false));
        CHECK(rings.publish(3, {}, {0, 1, 0}, "partial resource", partial, 0, error) && !error.empty());
        CHECK(rings.active.size() == 1 && rings.failed == 3 && rings.started == 1);
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize() && test_explosion_ring::mount(g.resources, root, error));
        auto h = g.objects.create(g.objects.find_class("CClientExplosionSFX"), {}, Creation::World,
                                  {{"Name", std::string("source")},
                                   {"Pos", DVector{10, 20, 30}},
                                   {"Direction", DVector{0, 3, 0}},
                                   {"ExplosionType", 3}});
        CHECK(h);
        DVector moved{100, 200, 300};
        CHECK(g.server.TeleportObject(h, &moved) == DE_OK && command(g, h) && command(g, h));
        auto& rings = g.server.explosion_rings;
        CHECK(rings.active.size() == 8 && rings.started == 8 && !rings.failed && rings.reserved_particles() == 256);
        const auto& report = g.server.scene_explosions.back();
        CHECK(report.ring_event && report.ring_error.empty() && !report.client_fx_available);
        CHECK(report.targets.null_damager && !report.targets.delivered && !report.model_error.empty());
        CHECK(rings.active[0].settings.position.x == 10 && rings.active[0].settings.position.y == 45);
        CHECK(g.step(.11) && !g.objects.get(h) && rings.active.size() == 8);
        CHECK(rings.active[0].particles.size() == 32 && near(rings.active[0].particles[0].position.x, 54));
        SpriteView scene;
        CHECK(update_sprite_scene(g, scene, error));
        unsigned count = 0;
        for (const auto& entry : scene.entries)
            if (entry.particle_system)
            {
                CHECK(entry.error.empty() && entry.range < scene.mesh.ranges.size());
                ++count;
                const auto& v = scene.mesh.vertices[scene.mesh.ranges[entry.range].first];
                CHECK(near(v.position.x, -12.5f) || near(v.position.x, -5000.f / 640));
            }
        CHECK(count >= 64 && count <= 256);
        const auto updates = rings.active.front().updates;
        CHECK(update_sprite_scene(g, scene, error) && rings.active.front().updates == updates);
        CHECK(!g.server.PublishExplosionRings(h, 3, {}, {0, 1, 0}, error) && !error.empty());
        CHECK(g.step(.5) && rings.active.size() == 6 && rings.expired == 2);
        for (const auto& fx : rings.active)
            CHECK(fx.particles.size() == 32 && !fx.delay);
        CHECK(g.step(1.6) && rings.active.empty());
        CHECK(g.unload_world() && rings.reports.empty() && !g.server.string_count() && !g.server.message_count());
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
            unsigned emitters = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                if (dynamic_cast<CClientExplosionSFX*>(r->game.get()))
                {
                    ++emitters;
                    CHECK(command(g, r->handle));
                }
            }
            auto& rings = g.server.explosion_rings;
            CHECK(emitters == 9 && rings.active.size() == 36 && rings.reserved_particles() == 1152 && !rings.failed);
            CHECK(rings.active.front().id > previous);
            previous = rings.active.back().id;
            CHECK(g.step(.11));
            SpriteView scene;
            CHECK(update_sprite_scene(g, scene, error));
            unsigned drawn = 0;
            for (const auto& e : scene.entries)
                if (e.particle_system)
                {
                    if (!e.error.empty())
                        std::cerr << e.frame_path << ": " << e.error << '\n';
                    CHECK(e.error.empty());
                    ++drawn;
                }
            CHECK(drawn >= 288 && drawn <= 1152);
            for (unsigned i = 0; i < 50; ++i)
                CHECK(g.step(.01));
            CHECK(rings.active.size() == 27 && rings.expired == 9 && rings.reserved_particles() == 864);
            for (const auto& fx : rings.active)
                CHECK(fx.particles.size() == 32 && fx.alpha > 0 && !fx.delay);
            for (unsigned i = 0; i < 160; ++i)
                CHECK(g.step(.01));
            CHECK(rings.active.empty() && rings.expired == 36 && !rings.failed);
            std::cout << "SubwayA cycle " << cycle
                      << ": nine emitters, 36 rings, 1152 reserved particles; all expired, IDs through " << previous
                      << '\n';
            CHECK(g.unload_world() && rings.reports.empty() && g.objects.snapshot().empty());
            CHECK(!g.server.string_count() && !g.server.message_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string name = argv[1];
    return (name == "motion"      ? motion()
            : name == "recipes"   ? recipes()
            : argc != 3           ? false
            : name == "lifecycle" ? lifecycle(argv[2])
            : name == "failures"  ? failures(argv[2])
            : name == "session"   ? session(argv[2])
                                  : name == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
