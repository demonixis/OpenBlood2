#include "app/model_scene.h"
#include "brush_fixture.h"
#include "explosion_fragment_fixture.h"
#include "game/original/ClientExplosionSFX.h"
#include <cmath>
#include <limits>
#include <numbers>
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
        return std::abs(a - b) < .0001f;
    }
    bool command(GameSession& game, HOBJECT h)
    {
        auto s = game.server.CreateString("TRIGGER");
        auto m = game.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = m && game.server.WriteToMessageHString(m, s) == DE_OK && game.server.EndMessage(m) == DE_OK;
        game.server.FreeString(s);
        return ok;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(g.initialize() && test_explosion_fragment::mount(g.resources, root, error));
        auto floor = test_brush::box("floor", {-10000, -10, -10000}, {10000, 0, 10000});
        for (auto& surface : floor.surfaces)
            surface.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    bool lifecycle(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        std::minstd_rand rng(1);
        ExplosionFragSettings s;
        s.position = {0, 5, 0};
        s.normal = {0, -1, 0};
        s.velocity = 4;
        s.gravity = 1;
        s.duration = 10;
        s.fade_time = 2;
        s.rotate_max = {1, 1, 1};
        CExplosionFragFX f;
        CHECK(f.Init(s, error) && f.CreateObject(g.models, g.resources, 0, rng, error));
        auto turn = f.angular_step;
        CHECK(f.last_normal.y == 0 && f.alpha == 1 && f.flags & FLAG_NOLIGHT);
        CHECK(f.Update(0, g.server) && f.impacts == 1 && f.last_query == SegmentResult::Hit);
        CHECK(near(f.position.y, 1) && near(f.velocity.y, 1.25f) && f.last_hit.polygon != UINT32_MAX);
        CHECK(near(f.angles.x, turn.x) && near(f.angular_step.x, turn.x / 2));
        CHECK(f.Update(.0005, g.server) && f.updates == 1 && near(f.position.y, 1));
        CHECK(f.Update(.01, g.server) && f.updates == 2 && near(f.position.y, 1.25f) && near(f.velocity.y, .25f));
        CHECK(f.Update(.02, g.server) && !f.moving && near(f.position.y, 1.25f));
        const auto angle = f.angles;
        CHECK(f.Update(8, g.server) && f.alpha == 1 && near(f.angles.x, angle.x));
        CHECK(f.Update(9, g.server) && near(f.alpha, .5f));
        CHECK(!f.Update(10, g.server) && f.error.empty() && near(f.alpha, .5f)); // Strict fade endpoint.
        s.normal = {1, 0, 0};
        s.position = {0, 50, 0};
        s.gravity = 0;
        s.velocity = 2;
        s.fade_time = 0;
        s.rotate_max = {};
        CHECK(f.Init(s, error) && f.CreateObject(g.models, g.resources, 0, rng, error));
        CHECK(f.Update(.1, g.server) && near(f.position.x, 2));
        CHECK(f.Update(5, g.server) && near(f.position.x, 4));                      // Per update, not dt-scaled.
        CHECK(!f.Update(10, g.server) && f.error.empty() && near(f.position.x, 6)); // Terminal motion retained.
        s.duration = 2;
        s.fade_time = 1;
        s.alpha = .4f;
        CHECK(f.Init(s, error) && f.CreateObject(g.models, g.resources, 0, rng, error));
        CHECK(f.Update(1.25, g.server) && near(f.alpha, .15f));  // Subtraction, not alpha multiplied by fade.
        CHECK(f.Update(1.75, g.server) && near(f.alpha, -.35f)); // Renderer must reject this unsupported color range.
        ModelView rejected;
        CHECK(!rejected.add(f.model.get(), f.animation, f.skin_path, {f.position, f.rotation}, s.scale, true, false, {},
                            "unsupported negative source alpha", g.resources, error, {}, f.alpha, true));
        s.alpha = 1;
        s.fade_time = 0;
        s.velocity = 1;
        CHECK(f.Init(s, error) && f.CreateObject(g.models, g.resources, 0, rng, error));
        f.last_normal = {0, 1, 0};
        CHECK(f.Update(0, g.server) && f.moving && near(f.position.x, 1)); // speed == 1 still moves.
        f.velocity = {.5f, 0, 0};
        f.last_normal = {0, .9f, 0};
        CHECK(f.Update(.01, g.server) && f.moving && near(f.position.x, 1.5f)); // normal.y == .9 still moves.
        f.last_normal.y = .9001f;
        CHECK(f.Update(.02, g.server) && !f.moving && near(f.position.x, 1.5f));
        return true;
    }
    bool collision(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        std::minstd_rand rng(1);
        ObjectCreateStruct object;
        object.m_Pos = {0, 5, 5};
        object.m_Flags = FLAG_RAYHIT;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), object);
        DVector dims{1, 1, 1};
        CHECK(h && g.server.SetObjectDims(h, &dims) == DE_OK);
        ExplosionFragSettings s;
        s.position = {0, 5, 0};
        s.normal = {0, 0, 1};
        s.velocity = 10;
        s.gravity = 0;
        CExplosionFragFX f;
        CHECK(f.Init(s, error) && f.CreateObject(g.models, g.resources, 0, rng, error) && f.Update(0, g.server));
        CHECK(f.last_hit.object == h && near(f.position.z, 3) && near(f.velocity.z, -2.5f));
        g.server.RemoveObject(h);
        CHECK(f.Init(s, error) && f.CreateObject(g.models, g.resources, 0, rng, error) && f.Update(0, g.server));
        CHECK(f.last_query == SegmentResult::Miss && near(f.position.z, 10));
        g.server.collision_ready = false;
        auto pos = f.position;
        CHECK(!f.Update(.01, g.server) && !f.error.empty() && near(f.position.z, pos.z));
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        return true;
    }
    bool recipes(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        ExplosionFragments manager;
        std::minstd_rand expected(1);
        auto draw = [&](float lo, float hi)
        {
            return lo + (hi - lo) * float(expected() - std::minstd_rand::min()) /
                            float(std::minstd_rand::max() - std::minstd_rand::min());
        };
        for (unsigned event = 0; event < 10; ++event)
        {
            const auto begin = manager.active.size();
            CHECK(manager.publish(3, {10, 20, 30}, {0, 2, 0}, "recipe", g.models, g.resources, 0, error) &&
                  error.empty());
            unsigned count = 0;
            for (; count < 2 + (expected() - std::minstd_rand::min()) % 4; ++count)
            {
                CHECK(begin + count < manager.active.size());
                const auto& f = manager.active[begin + count];
                CHECK(near(f.settings.spread, draw(40, 60)) && near(f.settings.velocity, draw(20, 40)));
                const float roll = draw(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
                const float spread = f.settings.spread, speed = f.settings.velocity;
                const float launch_length = std::sqrt(spread * spread + speed * speed);
                CHECK(near(f.velocity.x, std::cos(roll) * spread / launch_length * speed));
                CHECK(near(f.velocity.y, speed / launch_length * speed));
                CHECK(near(f.velocity.z, -std::sin(roll) * spread / launch_length * speed));
                CHECK(near(f.angular_step.x, draw(-1, 1)) && near(f.angular_step.y, draw(-1, 1)) &&
                      near(f.angular_step.z, draw(-1, 1)));
                CHECK(f.settings.position.x == 10 && f.settings.normal.y == 1 && f.settings.duration == 10 &&
                      f.settings.fade_time == 2);
                CHECK(f.settings.trail_type == 1 && f.trail_error.empty() && f.settings.spawn_explosion == 0);
                CHECK(near(std::sqrt(vector_dot(f.velocity, f.velocity)), f.settings.velocity));
            }
            CHECK(count >= 2 && count <= 5 && manager.active.size() == begin + count);
            CHECK(manager.reports.back().fragments.size() == count && manager.reports.back().unavailable.empty() &&
                  manager.reports.back().trails.size() == count);
        }
        for (unsigned type : {0u, 1u, 2u, 6u, 8u})
            CHECK(manager.publish(type, {}, {0, 1, 0}, "no branch", g.models, g.resources, 0, error) && error.empty() &&
                  manager.reports.back().fragments.empty());
        CHECK(manager.publish(24, {}, {0, 1, 0}, "unsupported", g.models, g.resources, 0, error) && !error.empty());
        // Explicit random x/y/z ordering, then pitch/yaw/roll.
        ExplosionFragSettings s;
        s.random_direction = true;
        s.normal = {};
        s.velocity = 5;
        s.rotate_max = {1, 2, 3};
        CExplosionFragFX f;
        std::minstd_rand random(17), compare(17);
        auto rd = [&](float a, float b)
        {
            return a + (b - a) * float(compare() - std::minstd_rand::min()) /
                           float(std::minstd_rand::max() - std::minstd_rand::min());
        };
        const float x = rd(-10, 10), y = rd(-10, 10), z = rd(-10, 10);
        const float length = std::sqrt(x * x + y * y + z * z);
        CHECK(f.Init(s, error) && f.CreateObject(g.models, g.resources, 0, random, error));
        CHECK(near(f.velocity.x, x / length * 5) && near(f.velocity.y, y / length * 5) &&
              near(f.velocity.z, z / length * 5));
        CHECK(near(f.angular_step.x, rd(-1, 1)) && near(f.angular_step.y, rd(-2, 2)) &&
              near(f.angular_step.z, rd(-3, 3)));
        CHECK(random() == compare());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        std::minstd_rand random(1);
        CExplosionFragFX f;
        CHECK(!f.CreateObject(g.models, g.resources, 0, random, error) && !f.Update(0, g.server));
        for (unsigned kind = 0; kind < 12; ++kind)
        {
            ExplosionFragSettings s;
            switch (kind)
            {
                case 0:
                    s.duration = 0;
                    break;
                case 1:
                    s.normal = {};
                    break;
                case 2:
                    s.bounce = 1.1f;
                    break;
                case 3:
                    s.fade_time = 6;
                    break;
                case 4:
                    s.rotate_max.x = -1;
                    break;
                case 5:
                    s.spawn_explosion = 1;
                    break;
                case 6:
                    s.spawn_type = 3;
                    break;
                case 7:
                    s.trail_type = 2;
                    break;
                case 8:
                    s.alpha = -1;
                    break;
                case 9:
                    s.model = std::string("a\0b", 3);
                    break;
                case 10:
                    s.gravity = std::numeric_limits<float>::infinity();
                    break;
                case 11:
                    s.scale.z = -1;
                    break;
            }
            CHECK(!f.Init(s, error) && !error.empty());
        }
        ExplosionFragSettings s;
        s.model = "missing.abc";
        CHECK(f.Init(s, error) && !f.CreateObject(g.models, g.resources, 0, random, error));
        s.model.reset();
        s.skin = "missing.dtx";
        CHECK(f.Init(s, error) && !f.CreateObject(g.models, g.resources, 0, random, error) && !f.model);
        s.skin.reset();
        CHECK(f.Init(s, error) && !f.CreateObject(g.models, g.resources, -1, random, error));
        CHECK(f.CreateObject(g.models, g.resources, 1, random, error) &&
              !f.CreateObject(g.models, g.resources, 2, random, error));
        CHECK(!f.Update(0, g.server) && !f.Update(std::numeric_limits<double>::infinity(), g.server));
        ExplosionFragments manager;
        for (unsigned i = 0; i < 70; ++i)
            CHECK(manager.publish(3, {}, {0, 1, 0}, "capacity", g.models, g.resources, 0, error));
        CHECK(manager.active.size() == 128 && manager.started == 128 && manager.failed > 0 &&
              manager.reports.size() == 64);
        const auto previous = manager.active.back().id, event = manager.reports.back().id;
        CHECK(manager.advance(11, g.server) && manager.active.empty() && manager.expired == 128);
        manager.clear();
        CHECK(manager.reports.empty() && !manager.failed && !manager.started);
        CHECK(manager.publish(3, {}, {0, 1, 0}, "new", g.models, g.resources, 1, error) && error.empty());
        CHECK(manager.active.front().id > previous && manager.reports.front().id > event);
        CHECK(!manager.advance(0, g.server) && manager.active.empty() && !manager.update_error.empty());
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        auto h = g.objects.create(g.objects.find_class("CClientExplosionSFX"), {}, Creation::World,
                                  {{"Name", std::string("fragment source")},
                                   {"Pos", DVector{0, 20, 0}},
                                   {"Direction", DVector{0, 1, 0}},
                                   {"ExplosionType", 3}});
        CHECK(h);
        DVector changed{500, 500, 500};
        CHECK(g.server.TeleportObject(h, &changed) == DE_OK && command(g, h) && command(g, h));
        auto& m = g.server.explosion_fragments;
        CHECK(m.started >= 4 && m.started <= 10 && !m.failed);
        const auto& report = g.server.scene_explosions.back();
        CHECK(report.fragment_event && report.fragment_error.empty() && !report.client_fx_available &&
              report.targets.null_damager && !report.targets.delivered);
        for (const auto& f : m.active)
            CHECK(f.settings.position.x == 0 && f.settings.position.y == 20);
        CHECK(g.step(.11) && !g.objects.get(h));
        ModelView view;
        CHECK(update_explosion_model_scene(g, view, error));
        unsigned count = 0;
        for (const auto& e : view.entries)
            if (e.fragment_effect)
            {
                CHECK(e.error.empty() && e.triangles == 1 && e.range < view.mesh.ranges.size());
                ++count;
            }
        CHECK(count == m.active.size());
        const auto steps = m.active.front().updates;
        CHECK(update_explosion_model_scene(g, view, error) && m.active.front().updates == steps);
        CHECK(!g.server.PublishExplosionFragments(h, 3, {}, {0, 1, 0}, error));
        CHECK(g.step(10) && m.active.empty() && m.expired == count);
        CHECK(g.unload_world() && m.reports.empty() && !g.server.string_count() && !g.server.message_count());
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
                if (dynamic_cast<CClientExplosionSFX*>(r->game.get()))
                {
                    CHECK(command(g, r->handle));
                    ++sources;
                }
            }
            auto& m = g.server.explosion_fragments;
            for (const auto& r : m.reports)
                if (!r.error.empty())
                    std::cerr << r.error << '\n';
            CHECK(sources == 9 && m.active.size() >= 18 && m.active.size() <= 45 && !m.failed);
            const auto count = m.started;
            CHECK(m.active.front().id > previous);
            previous = m.active.back().id;
            CHECK(g.step(.11));
            ModelView view;
            CHECK(update_explosion_model_scene(g, view, error));
            unsigned drawn = 0;
            for (const auto& e : view.entries)
                if (e.fragment_effect)
                {
                    if (!e.error.empty())
                        std::cerr << e.error << '\n';
                    CHECK(e.error.empty() && e.triangles > 0);
                    ++drawn;
                }
            CHECK(drawn == count);
            std::uint64_t impacts = 0;
            for (unsigned frame = 0; frame < 201; ++frame)
            {
                CHECK(g.step(.05));
                std::uint64_t total = 0;
                for (const auto& f : m.active)
                    total += f.impacts;
                impacts = std::max(impacts, total);
            }
            CHECK(impacts > 0 && m.active.empty() && m.expired == count && !m.failed);
            std::cout << "SubwayA cycle " << cycle << ": nine sources, " << count << " metal fragments, " << impacts
                      << " observed impacts, full expiry; IDs through " << previous << '\n';
            CHECK(g.unload_world() && m.reports.empty() && g.objects.snapshot().empty());
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
    return (n == "lifecycle"   ? lifecycle(argv[2])
            : n == "collision" ? collision(argv[2])
            : n == "recipes"   ? recipes(argv[2])
            : n == "failures"  ? failures(argv[2])
            : n == "session"   ? session(argv[2])
                               : n == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
