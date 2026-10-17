#include "app/sprite_scene.h"
#include "brush_fixture.h"
#include "explosion_fragment_fixture.h"
#include "game/original/ClientExplosionSFX.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ":" << __LINE__ << ": " #x << "\n";                                               \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool near(float a, float b)
    {
        return std::abs(a - b) < .0002f;
    }
    bool setup(Resources& r, const std::filesystem::path& root)
    {
        std::string e;
        return test_explosion_fragment::mount(r, root, e);
    }
    bool emission(const std::filesystem::path& root)
    {
        Resources resources;
        CHECK(setup(resources, root));
        std::string error;
        CObjectParticleFX t;
        NodePose owner{{100, 20, 30}, {}};
        std::minstd_rand rng(17), expected(17);
        auto draw = [&](float a, float b)
        { return a + (b - a) * float(expected() - expected.min()) / float(expected.max() - expected.min()); };
        CHECK(t.Init(1, 42, error) && t.CreateObject(resources, owner, 100, error));
        CHECK(t.Update(100, &owner, rng) && t.particles.empty() && t.first_update == false);
        owner.position.x += 10;
        CHECK(t.Update(100.002, &owner, rng) && t.particles.size() == 5 && t.emitted == 5); // Absolute last-add zero.
        for (unsigned i = 0; i < 5; ++i)
        {
            const auto& v = t.particles[i];
            CHECK(near(v.position.x, 10 - 2.f * i) && near(v.position.y, 0) && near(v.position.z, 0));
            CHECK(near(v.velocity.z, draw(20, 50)) && near(v.velocity.x, 0));
            const float gray = (96 + 96 * draw(0, 1)) / 255;
            CHECK(near(v.color.x, gray) && v.color.x == v.color.y && v.color.y == v.color.z);
            CHECK(near(v.lifetime, draw(.5f, .75f)));
        }
        CHECK(rng() == expected() && t.authored_count == 12 && t.alpha == 1 && t.requested_alpha == .25f);
        auto old = t.particles[0];
        owner.position.x += 2;
        owner.rotation = rotation_from_euler({0, 1.57079632679f, 0});
        CHECK(t.Update(100.015, &owner, rng) && t.particles.size() == 6);
        CHECK(near(t.particles[0].position.z, old.velocity.z * .013f));
        CHECK(near(t.particles[0].velocity.y, 25 * .013f));
        CHECK(t.particles.back().velocity.x > 19 && std::abs(t.particles.back().velocity.z) < .001f);
        CHECK(t.origin.x == 100 && t.position.x == 112 && t.particles.back().position.x == 12);
        return true;
    }
    bool stopping(const std::filesystem::path& root)
    {
        Resources r;
        CHECK(setup(r, root));
        std::string e;
        CObjectParticleFX t;
        NodePose p;
        std::minstd_rand random(1);
        CHECK(t.Init(1, 1, e) && t.CreateObject(r, p, 0, e) && t.Update(.1, &p, random));
        p.position.x = 4;
        CHECK(t.Update(.2, &p, random) && t.emitted == 2);
        CHECK(t.Update(.3, &p, random) && !t.moving && near(t.duration, 1.05f));
        p.position.x = 100;
        CHECK(t.Update(.4, &p, random) && t.position.x == 4 && t.emitted == 2);
        CHECK(t.Update(double(t.duration), &p, random) && t.particles.empty()); // strict duration < elapsed
        CHECK(!t.Update(double(t.duration) + .01, &p, random) && t.error.empty());
        CHECK(t.Init(1, 1, e) && t.CreateObject(r, {}, 0, e) && t.Update(0, &p, random));
        p.position.x = 104;
        CHECK(t.Update(.01, &p, random) && !t.particles.empty());
        CHECK(!t.Update(.02, nullptr, random) && t.error.empty()); // parent removal stops even a nonempty tail
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        // Existing callers retain 2048; explicitly bounded smoke pools can be larger.
        std::string capacity_error;
        std::vector<Particle> pool(2049);
        for (auto& particle : pool)
            particle.lifetime = 1;
        CHECK(!advance_particles(pool, 0, .1, capacity_error) && pool.front().lifetime == 1);
        CHECK(advance_particles(pool, 0, .1, capacity_error, max_particle_capacity));
        CHECK(near(pool.front().lifetime, .9f));
        CHECK(!advance_particles(pool, 0, .1, capacity_error, max_particle_capacity + 1));
        pool.resize(max_particle_capacity + 1);
        CHECK(!advance_particles(pool, 0, .1, capacity_error, max_particle_capacity));
        Resources r;
        CHECK(setup(r, root));
        std::string e;
        CObjectParticleFX t;
        NodePose p;
        std::minstd_rand random(1);
        CHECK(!t.Init(2, 1, e) && !t.Init(1, 0, e) && !t.CreateObject(r, p, 0, e) && !t.Update(0, &p, random));
        CHECK(t.Init(1, 1, e));
        Resources missing;
        CHECK(!t.CreateObject(missing, p, 0, e));
        CHECK(!t.CreateObject(r, p, -1, e));
        p.rotation.m_Spin = 0;
        CHECK(!t.CreateObject(r, p, 0, e));
        p = {};
        CHECK(t.CreateObject(r, p, 0, e) && !t.CreateObject(r, p, 0, e));
        CHECK(!t.Update(-1, &p, random) && !t.Update(std::numeric_limits<double>::infinity(), &p, random));
        CHECK(t.Update(0, &p, random));
        p.position.x = 1.9f;
        auto saved = random;
        CHECK(t.Update(.02, &p, random) && t.emitted == 0 && random == saved); // No sample, no zero division or RNG.
        p.position.x = 5000;
        CHECK(!t.Update(.04, &p, random) && !t.error.empty() && t.particles.empty() && random == saved);
        CHECK(t.Init(1, 1, e) && t.CreateObject(r, {}, 0, e) && t.Update(0, &p, random));
        p.position.x = 10;
        CHECK(!t.Update(.02, &p, random, 4) && t.particles.empty());
        std::vector<Particle> colors{{{}, {}, 1, {1, 1, 1}}, {{}, {}, 1, {-1, 1, 1}}};
        CHECK(!advance_particles(colors, 0, .5, e) && colors[0].lifetime == 1); // Atomic shared color validation.
        Resources partial;
        CHECK(test_explosion_fragment::mount(partial, root / "partial", e, false));
        ModelStore models(partial);
        ExplosionFragments manager;
        CHECK(manager.publish(3, {}, {0, 1, 0}, "partial", models, partial, 0, e) && e.empty());
        CHECK(!manager.active.empty() && manager.trails.empty() && manager.trail_failed == manager.started);
        CHECK(!manager.reports.back().unavailable.empty() && !manager.active.front().trail_error.empty());
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize() && setup(g.resources, root));
        auto& m = g.server.explosion_fragments;
        CHECK(m.publish(3, {0, 20, 0}, {0, 1, 0}, "detached", g.models, g.resources, 0, error) && error.empty());
        CHECK(m.trail_started == m.started && m.trails.size() == m.active.size() && !m.trail_failed);
        auto floor = test_brush::box("floor", {-1000, -10, -1000}, {1000, 0, 1000});
        for (auto& surface : floor.surfaces)
            surface.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        CHECK(m.advance(0, g.server) && m.trail_particles() == 0);
        CHECK(m.advance(.02, g.server) && m.trail_particles() > 0);
        SpriteView view;
        CHECK(update_sprite_scene(g, view, error));
        unsigned count = 0;
        for (const auto& e : view.entries)
            if (e.trail_system)
            {
                CHECK(e.error.empty() && e.range < view.mesh.ranges.size());
                ++count;
            }
        CHECK(count == m.trail_particles());
        auto emitted = m.trails.front().emitted;
        CHECK(update_sprite_scene(g, view, error) && m.trails.front().emitted == emitted);
        auto identity = m.trails.back().id;
        auto parent = m.trails.front().parent;
        std::erase_if(m.active, [&](const auto& x) { return x.id == parent; });
        CHECK(m.advance(.04, g.server));
        for (const auto& t : m.trails)
            CHECK(t.parent != parent);
        CHECK(m.advance(11, g.server) && m.trails.empty() && m.trail_expired == m.trail_started);
        m.clear();
        CHECK(m.publish(3, {}, {0, 1, 0}, "new", g.models, g.resources, 12, error) && error.empty());
        CHECK(m.trails.front().id > identity);
        CHECK(g.unload_world() && m.trails.empty() && m.reports.empty());
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
            unsigned sources = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                if (!dynamic_cast<CClientExplosionSFX*>(r->game.get()))
                    continue;
                auto str = g.server.CreateString("TRIGGER");
                auto msg = g.server.StartMessageToObject(nullptr, r->handle, 1000);
                CHECK(msg && g.server.WriteToMessageHString(msg, str) == DE_OK && g.server.EndMessage(msg) == DE_OK);
                g.server.FreeString(str);
                ++sources;
            }
            auto& m = g.server.explosion_fragments;
            CHECK(sources == 9 && m.trails.size() == m.active.size() && !m.trail_failed);
            CHECK(m.trails.front().id > previous);
            previous = m.trails.back().id;
            std::uint64_t samples = 0;
            std::size_t peak = 0;
            SpriteView view;
            bool rendered = false;
            const double dt = cycle == 0 ? .02 : cycle == 1 ? 1. / 60 : .05;
            for (unsigned frame = 0; frame < unsigned(std::ceil(10.1 / dt)); ++frame)
            {
                const bool stepped = g.step(dt);
                if (!stepped)
                    std::cerr << m.update_error << "\n";
                CHECK(stepped);
                CHECK(m.update_error.empty() && !m.trail_failed && !m.failed);
                peak = std::max(peak, m.trail_particles());
                std::uint64_t total = 0;
                for (const auto& t : m.trails)
                    total += t.emitted;
                samples = std::max(samples, total);
                if (!rendered && m.trail_particles())
                {
                    CHECK(update_sprite_scene(g, view, error));
                    unsigned count = 0;
                    for (const auto& e : view.entries)
                        if (e.trail_system)
                        {
                            CHECK(e.error.empty() && e.range < view.mesh.ranges.size());
                            ++count;
                        }
                    CHECK(count == m.trail_particles());
                    rendered = true;
                }
            }
            CHECK(rendered && peak > 0 && samples > 0 && m.trails.empty() && m.active.empty() &&
                  m.trail_expired == m.trail_started);
            std::cout << "SubwayA cycle " << cycle << " dt=" << dt << ": " << m.trail_started << " trails, peak "
                      << peak << " particles, observed " << samples << " emitted; all expired\n";
            CHECK(g.unload_world() && m.trails.empty() && !m.trail_started && g.objects.snapshot().empty());
            CHECK(!g.server.string_count() && !g.server.message_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string n = argv[1];
    return (n == "emission"   ? emission(argv[2])
            : n == "stopping" ? stopping(argv[2])
            : n == "failures" ? failures(argv[2])
            : n == "session"  ? session(argv[2])
                              : n == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
