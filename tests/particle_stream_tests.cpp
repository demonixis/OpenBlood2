#include "app/sprite_scene.h"
#include "explosion_flame_fixture.h"
#include "game/original/ClientParticleStreamSFX.h"
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
        return std::abs(a - b) < .0001f;
    }
    bool trigger(GameSession& g, HOBJECT h, const char* command)
    {
        auto s = g.server.CreateString(command);
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = m && g.server.WriteToMessageHString(m, s) == DE_OK && g.server.EndMessage(m) == DE_OK;
        g.server.FreeString(s);
        return ok;
    }
    bool rules(const std::filesystem::path& root)
    {
        Resources r;
        std::string e;
        CHECK(test_explosion_flame::mount(r, root, e));
        ParticleStreamSettings s;
        s.on = true;
        s.count = 3;
        s.position_radius = 4;
        s.min_velocity = 10;
        s.max_velocity = 20;
        s.spread = 3;
        s.min_color = {53, 53, 53};
        s.max_color = {255, 214, 128};
        CParticleStreamFX fx;
        std::minstd_rand random(17), expected(17);
        CHECK(fx.Init(s, e) && fx.CreateObject(r, {{10, 20, 30}, {}}, 4, random, e));
        CHECK(fx.state == 2 && fx.particles.size() == 3 && fx.bursts == 1 && fx.last_add == 4);
        // Independently reconstructed first burst from the original draw sequence.
        for (const auto& p : fx.particles)
        {
            float t[9];
            for (auto& v : t)
                v = float(expected() - std::minstd_rand::min()) /
                    float(std::minstd_rand::max() - std::minstd_rand::min());
            CHECK(near(p.lifetime, 1 + t[0]) && near(p.position.x, -4 + 8 * t[1]) &&
                  near(p.position.y, -4 + 8 * t[2]) && near(p.position.z, -4 + 8 * t[3]));
            DVector vel{-3 + 6 * t[4], -3 + 6 * t[5], 17 + 6 * t[6]};
            float length = std::sqrt(vector_dot(vel, vel));
            CHECK(near(p.velocity.x, vel.x / length * (10 + 10 * t[7])) &&
                  near(p.velocity.z, vel.z / length * (10 + 10 * t[7])));
            float red = 53 + 202 * t[8];
            CHECK(near(p.color.x, red / 255) && near(p.color.y, 161 * red / 202 / 255) &&
                  near(p.color.z, 75 * red / 202 / 255));
        }
        CHECK(random() == expected());
        // Equal red bounds ignore the otherwise different blue range (source quirk).
        s.min_color = {255, 255, 0};
        s.max_color = {255, 255, 255};
        s.min_velocity = s.max_velocity = s.spread = 0;
        CHECK(fx.Init(s, e) && fx.CreateObject(r, {}, 0, random, e));
        CHECK(fx.particles[0].color.z == 0 && vector_dot(fx.particles[0].velocity, fx.particles[0].velocity) == 0);
        return true;
    }
    bool clocks(const std::filesystem::path& root)
    {
        Resources r;
        std::string e;
        CHECK(test_explosion_flame::mount(r, root, e));
        ParticleStreamSettings s;
        s.on = true;
        s.delay = .25f;
        s.min_life = s.max_life = 2;
        s.min_velocity = s.max_velocity = 10;
        s.gravity = 4;
        s.position_radius = 0;
        CParticleStreamFX fx;
        std::minstd_rand random(1);
        NodePose pose{};
        CHECK(fx.Init(s, e) && fx.CreateObject(r, pose, 0, random, e));
        CHECK(fx.Update(.25, &pose, true, random) && fx.bursts == 1); // strict boundary
        CHECK(fx.particles[0].position.z == 2.5f && fx.particles[0].position.y == 0 && fx.particles[0].velocity.y == 1);
        pose.position = {10, 20, 30};
        pose.rotation = rotation_from_euler({0, 1.57079632679f, 0});
        CHECK(fx.Update(.5, &pose, true, random) && fx.bursts == 2 && fx.position.x == 10);
        CHECK(near(fx.particles.back().velocity.x, 10) &&
              fx.particles[0].velocity.z == 10); // only new directions change
        CHECK(fx.Update(.5005, &pose, false, random) && fx.state == 2 && fx.particles[0].lifetime < 1.5f);
        CHECK(fx.Update(.502, &pose, false, random) && fx.state == 0 && fx.bursts == 2 && fx.last_add == .5f);
        CHECK(fx.Update(3, &pose, false, random) && fx.particles.empty());
        CHECK(fx.Update(3.25, &pose, true, random) && fx.bursts == 3 && fx.last_add == 3.25f); // no catch-up bursts
        CHECK(!fx.Update(3.5, nullptr, true, random) && fx.error.empty());
        return true;
    }
    bool ramps(const std::filesystem::path& root)
    {
        Resources r;
        std::string e;
        CHECK(test_explosion_flame::mount(r, root, e));
        ParticleStreamSettings s;
        s.ramp_time = 2;
        s.ramp_flags = 15;
        s.count = 8;
        s.delay = .25f;
        s.position_radius = 16;
        s.min_velocity = s.max_velocity = 20;
        s.min_life = s.max_life = 4;
        s.gravity = 0;
        CParticleStreamFX fx;
        std::minstd_rand random(1);
        NodePose pose{};
        CHECK(fx.Init(s, e) && fx.CreateObject(r, pose, 0, random, e) && fx.particles.empty());
        CHECK(fx.Update(1, &pose, true, random) && fx.state == 1 && fx.bursts == 1 && fx.particles.empty());
        CHECK(fx.Update(2, &pose, true, random) && fx.state == 1 && fx.particles.size() == 4);
        for (auto& p : fx.particles)
            CHECK(std::abs(p.position.x) <= 8 && near(p.velocity.z, 10) && p.lifetime == 2);
        CHECK(fx.Update(3, &pose, true, random) && fx.state == 2 && fx.particles.size() == 12);
        CHECK(fx.Update(3.5, &pose, false, random) && fx.state == 3 && fx.particles.size() == 20);
        CHECK(fx.Update(4.5, &pose, false, random) && fx.state == 3 && fx.particles.size() == 20);
        const auto burst = fx.bursts;
        const auto add = fx.last_add;
        CHECK(fx.Update(5.5, &pose, false, random) && fx.state == 0 && fx.bursts == burst && fx.last_add == add);
        // Reversal resets a whole ramp, it does not interpolate from the current ratio.
        CHECK(fx.Update(6, &pose, true, random) && fx.state == 1);
        CHECK(fx.Update(6.5, &pose, false, random) && fx.state == 3);
        CHECK(fx.particles.back().lifetime == 4);
        for (unsigned flag : {1u, 2u, 4u, 8u})
        {
            s.ramp_flags = flag;
            random.seed(17);
            CHECK(fx.Init(s, e) && fx.CreateObject(r, pose, 0, random, e));
            CHECK(fx.Update(1, &pose, true, random));
            const auto before = fx.emitted;
            CHECK(fx.Update(2, &pose, true, random));
            CHECK(fx.emitted - before == (flag == 1 ? 4 : 8));
            const auto& p = fx.particles.back();
            CHECK(p.velocity.z == (flag == 4 ? 10 : 20));
            CHECK(p.lifetime == (flag == 8 ? 2 : 4));
            // Exact last X draw; a small random sample need not reach either edge of its range.
            std::minstd_rand expected(17);
            const unsigned first_count = flag == 1 ? 0 : 8, second_count = flag == 1 ? 4 : 8;
            float sample = 0;
            for (unsigned i = 0; i < 9 * (first_count + second_count - 1) + 2; ++i)
                sample = float(expected() - std::minstd_rand::min()) /
                         float(std::minstd_rand::max() - std::minstd_rand::min());
            CHECK(near(p.position.x, (-16 + 32 * sample) * (flag == 2 ? .5f : 1.f)));
        }
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources r;
        std::string e;
        CHECK(test_explosion_flame::mount(r, root, e));
        ParticleStreamSettings s;
        CParticleStreamFX fx;
        std::minstd_rand random(3);
        NodePose pose{};
        s.count = 257;
        CHECK(!fx.Init(s, e));
        s.count = 1;
        s.ramp_flags = 16;
        CHECK(!fx.Init(s, e));
        s.ramp_flags = 0;
        s.min_life = 3;
        CHECK(!fx.Init(s, e));
        s.min_life = 1;
        s.alpha = std::numeric_limits<float>::quiet_NaN();
        CHECK(!fx.Init(s, e));
        s.alpha = 1;
        s.texture = "missing.dtx";
        CHECK(fx.Init(s, e) && !fx.CreateObject(r, pose, 0, random, e));
        s.texture = "SpriteTextures/drop32_1.dtx";
        s.on = true;
        s.count = 256;
        s.min_life = s.max_life = 100;
        CHECK(fx.Init(s, e) && fx.CreateObject(r, pose, 0, random, e));
        auto saved = random;
        CHECK(!fx.Update(1, &pose, true, random, 256) && !fx.error.empty() && fx.particles.size() == 256);
        CHECK(saved() == random());
        CHECK(fx.Init(s, e) && !fx.CreateObject(r, pose, -1, random, e));
        CHECK(fx.CreateObject(r, pose, 0, random, e) && !fx.Update(-1, &pose, true, random));
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string e;
        CHECK(g.initialize() && test_explosion_flame::mount(g.resources, root, e));
        g.server.client_resources = &g.resources;
        const auto type = g.objects.find_class("CClientParticleStreamSFX");
        CHECK(type);
        CHECK(!g.objects.create(type, {}, Creation::Runtime));
        CHECK(g.objects.finish_frame());
        auto h = g.objects.create(type, {}, Creation::World,
                                  {{"InitiallyOn", true},
                                   {"Pos", DVector{0, 0, 50}},
                                   {"LoopSound", std::string("unread.wav")},
                                   {"Flags", int(FLAG_GRAVITY)}});
        CHECK(h && g.server.particle_streams.active.size() == 1);
        auto* emitter = dynamic_cast<CClientParticleStreamSFX*>(g.server.HandleToObject(h));
        CHECK(emitter && emitter->effect);
        CHECK(!g.objects.get(h)->scheduled && (g.server.GetObjectUserFlags(h) & 9u) == 9u &&
              !(g.server.GetObjectFlags(h) & FLAG_GRAVITY));
        CHECK(g.server.audio.voices().empty()); // Original unread message tail, not failed audio.
        CHECK(!g.server.PublishParticleStream(h, emitter->settings, e) && !e.empty());
        CHECK(g.server.particle_streams.active.size() == 1 && g.server.particle_streams.failed == 1);

        CHECK(trigger(g, h, "off") && emitter->settings.on && trigger(g, h, "OFF trailing") && !emitter->settings.on);
        CHECK(g.step(.25) && g.server.particle_streams.active[0].effect.state == 0);
        CHECK(trigger(g, h, "ONward") && g.step(.25) && g.server.particle_streams.active[0].effect.state == 2);
        CHECK(trigger(g, h, "TOGGLEstuff") && !emitter->settings.on && emitter->commands == 4);
        const auto previous = emitter->effect;
        const auto time = g.objects.time();
        SpriteView scene;
        CHECK(update_sprite_scene(g, scene, e));
        unsigned drawn = 0;
        for (auto& v : scene.entries)
            if (v.stream_system)
            {
                CHECK(v.error.empty() && v.range != UINT32_MAX);
                ++drawn;
            }
        CHECK(drawn == g.server.particle_streams.particle_count() && g.objects.time() == time);
        CHECK(g.objects.remove(h));
        CHECK(update_sprite_scene(g, scene, e) && scene.entries.empty());
        CHECK(g.step(.1) && g.server.particle_streams.active.empty() &&
              g.server.particle_streams.reports.front().removed);
        CHECK(g.unload_world());
        h = g.objects.create(type, {}, Creation::World, {{"InitiallyOn", true}});
        CHECK(h && g.server.particle_streams.active[0].effect.id > previous);
        CHECK(g.unload_world() && g.server.particle_streams.active.empty() && !g.server.string_count() &&
              !g.server.message_count());
        h = g.objects.create(type, {}, Creation::World, {{"ParticleFile", std::string("missing.dtx")}});
        CHECK(!h || g.objects.get(h)->life != Life::Alive);
        CHECK(g.server.particle_streams.failed == 1);
        CHECK(g.objects.finish_frame());
        h = g.objects.create(type, {}, Creation::World, {{"DifficultyMed", false}, {"InitiallyOn", true}});
        CHECK(!h || g.objects.get(h)->life != Life::Alive);
        CHECK(g.step(.1) && g.server.particle_streams.active.empty());
        return true;
    }
    bool budgets(const std::filesystem::path& root)
    {
        GameSession g;
        std::string e;
        CHECK(g.initialize() && test_explosion_flame::mount(g.resources, root, e));
        g.server.client_resources = &g.resources;
        auto type = g.objects.find_class("CClientParticleStreamSFX");
        for (unsigned i = 0; i < 32; ++i)
        {
            auto h = g.objects.create(type, {}, Creation::World, {{"InitiallyOn", true}, {"NumParticles", 256}});
            CHECK(h && g.objects.get(h)->life == Life::Alive);
        }
        CHECK(g.server.particle_streams.particle_count() == 8192);
        auto h = g.objects.create(type, {}, Creation::World, {{"InitiallyOn", true}});
        CHECK(!h || g.objects.get(h)->life != Life::Alive);
        CHECK(g.server.particle_streams.active.size() == 32 && g.server.particle_streams.failed == 1);
        CHECK(g.unload_world());
        for (unsigned i = 0; i < 128; ++i)
        {
            h = g.objects.create(type, {}, Creation::World);
            CHECK(h && g.objects.get(h)->life == Life::Alive);
        }
        h = g.objects.create(type, {}, Creation::World);
        CHECK(!h || g.objects.get(h)->life != Life::Alive);
        CHECK(g.server.particle_streams.active.size() == 128 && g.server.particle_streams.failed == 1);
        CHECK(g.server.particle_streams.reports.size() == 128);
        CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.string_count());
        return true;
    }
    bool private_world(const std::filesystem::path& root)
    {
        GameSession g;
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(g.initialize() && g.resources.mount_game(c, e));
        std::uint64_t last = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", e));
            unsigned created = 0, unknown = 0, failed = 0;
            unsigned omitted = 0;
            for (auto& entry : g.entries)
            {
                created += entry.state == EntryState::Created;
                unknown += entry.state == EntryState::Unsupported;
                failed += entry.state == EntryState::Failed;
                omitted += entry.state == EntryState::MissingWorldModel;
            }
            CHECK(created == 453 && unknown == 0 && failed == 0 && omitted == 1);
            auto& streams = g.server.particle_streams;
            CHECK(streams.active.size() == 8 && !streams.failed);
            unsigned initial = 0;
            for (auto& i : streams.active)
            {
                CHECK(i.effect.id > last);
                last = i.effect.id;
                initial += i.effect.particles.size();
            }
            CHECK(initial == 29); // three steam, two ten-spark streams, six fire particles.
            std::size_t peak = 0;
            for (unsigned tick = 0; tick < 240; ++tick)
            {
                CHECK(g.step(cycle == 0 ? .02 : cycle == 1 ? 1. / 60 : .05));
                peak = std::max(peak, streams.particle_count());
            }
            CHECK(!streams.failed && peak > 50 && streams.active.size() == 8);
            SpriteView scene;
            CHECK(update_sprite_scene(g, scene, e));
            unsigned drawn = 0;
            for (auto& v : scene.entries)
                if (v.stream_system)
                {
                    CHECK(v.error.empty() && v.range != UINT32_MAX);
                    ++drawn;
                }
            CHECK(drawn == streams.particle_count());
            for (auto& i : streams.active)
                CHECK(trigger(g, i.owner, "OFF"));
            for (unsigned i = 0; i < 100; ++i)
                CHECK(g.step(.05));
            CHECK(streams.particle_count() == 0 && streams.active.size() == 8);
            for (auto& i : streams.active)
                CHECK(trigger(g, i.owner, "ON"));
            CHECK(g.step(.2) && streams.particle_count() == 42); // includes shack smoke + third spark source.
            std::cout << "CityHub cycle " << cycle << ": 443 created / 10 unsupported / 1 failed; 8 streams, peak "
                      << peak << ", off expiry and 42-particle rearm\n";
            CHECK(g.unload_world() && streams.active.empty() && streams.reports.empty() && !g.server.string_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string m = argv[1];
    return (m == "rules"      ? rules(argv[2])
            : m == "clocks"   ? clocks(argv[2])
            : m == "ramps"    ? ramps(argv[2])
            : m == "failures" ? failures(argv[2])
            : m == "session"  ? session(argv[2])
            : m == "budgets"  ? budgets(argv[2])
                              : m == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
