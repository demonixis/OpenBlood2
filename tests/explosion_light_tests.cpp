#include "game/original/ClientExplosionSFX.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
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
    bool command(GameSession& g, HOBJECT h)
    {
        auto s = g.server.CreateString("TRIGGER");
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = m && g.server.WriteToMessageHString(m, s) == DE_OK && g.server.EndMessage(m) == DE_OK;
        g.server.FreeString(s);
        return ok;
    }
    bool lifecycle()
    {
        std::string error;
        ExplosionLightSettings s;
        s.color1 = {.2f, .4f, .6f};
        s.color2 = {.8f, .2f, 1};
        s.radius1 = 200;
        s.radius2 = 0;
        s.duration = 2;
        CExplosionLightFX fx;
        CHECK(fx.Init(s, error) && fx.CreateObject(10, error));
        CHECK(fx.flags == FLAG_VISIBLE && near(fx.radius, 200) && near(fx.color.x, .2f));
        CHECK(fx.Update(10) && fx.updates == 1);
        CHECK(fx.Update(10.0005) && fx.updates == 1);
        CHECK(fx.Update(10.5) && fx.updates == 2 && near(fx.radius, 150));
        CHECK(near(fx.color.x, .35f) && near(fx.color.y, .35f) && near(fx.color.z, .7f));
        CHECK(!fx.Update(12) && fx.error.empty() && near(fx.radius, 0) && near(fx.color.x, .8f));
        // Do not initialize the no-change flags from indeterminate original bytes.
        s.color2 = s.color1;
        s.radius2 = s.radius1;
        CHECK(fx.Init(s, error) && fx.CreateObject(1, error) && fx.Update(2));
        CHECK(near(fx.radius, 200) && near(fx.color.x, .2f));
        CHECK(!fx.Update(3) && fx.error.empty());
        // Delays hold the initial color and zero radius; exact boundary starts a fresh clock.
        s.delay = .5f;
        s.radius2 = 0;
        s.color2 = {1, 1, 1};
        CHECK(fx.Init(s, error) && fx.CreateObject(10, error));
        CHECK(fx.radius == 0 && near(fx.color.x, .2f));
        CHECK(fx.Update(10.499) && fx.radius == 0 && fx.delay == .5f);
        CHECK(fx.Update(10.5) && fx.delay == 0 && fx.start_time == 10.5 && fx.radius == 200);
        CHECK(near(fx.color.x, .2f)); // Activation returns before interpolation.
        CHECK(fx.Update(11) && near(fx.radius, 150) && near(fx.color.x, .4f));
        CHECK(!fx.Update(12.5) && fx.error.empty());
        // Source discards delay overshoot even when it exceeds the effect duration.
        CHECK(fx.Init(s, error) && fx.CreateObject(10, error) && fx.Update(100));
        CHECK(fx.start_time == 100 && fx.radius == 200 && fx.delay == 0 && near(fx.color.x, .2f));
        CHECK(fx.Update(101) && near(fx.radius, 100));
        CHECK(!fx.Update(103) && fx.error.empty() && near(fx.radius, -100));
        return true;
    }
    bool recipes()
    {
        std::vector<ExplosionLightSettings> out;
        std::string error;
        for (unsigned type : {0u, 1u, 2u, 3u, 6u, 8u})
        {
            CHECK(original_explosion_lights(type, {10, 20, 30}, {0, 2, 0}, out, error));
            CHECK(out.size() == (type == 8 ? 1u : 2u));
            CHECK(out[0].position.x == 10 && out[0].position.y == (type == 8 ? 50 : 30));
            CHECK(out[0].delay == 0);
            if (type == 8)
            {
                CHECK(out[0].duration == 3 && out[0].radius1 == 100 && out[0].radius2 == 350);
                CHECK(near(out[0].color1.x, .75f) && near(out[0].color2.z, .5f));
                continue;
            }
            CHECK(out[0].radius1 == (type == 0 || type == 6 ? 150 : type == 1 ? 200 : 250));
            CHECK(out[0].duration == 1.25f && out[0].radius2 == 0 && out[0].color1.z == 0 && out[0].color2.z == 1);
            CHECK(out[1].duration == 25 && out[1].radius2 == 0); // Reused descriptor, not Radius2=Radius1.
            CHECK(out[1].radius1 == (type == 0 || type == 6 ? 50 : type == 1 ? 75 : 100));
            CHECK(near(out[1].color1.x, .2f) && near(out[1].color2.x, .5f));
        }
        CHECK(!original_explosion_lights(24, {}, {0, 1, 0}, out, error) && out.empty() && !error.empty());
        CHECK(!original_explosion_lights(3, {}, {}, out, error) && out.empty());
        return true;
    }
    bool failures()
    {
        std::string error;
        CExplosionLightFX fx;
        CHECK(!fx.CreateObject(0, error) && !fx.Update(0) && !fx.error.empty());
        for (unsigned kind = 0; kind < 10; ++kind)
        {
            ExplosionLightSettings s;
            switch (kind)
            {
                case 0:
                    s.duration = 0;
                    break;
                case 1:
                    s.duration = 601;
                    break;
                case 2:
                    s.delay = -1;
                    break;
                case 3:
                    s.delay = 601;
                    break;
                case 4:
                    s.radius1 = -1;
                    break;
                case 5:
                    s.radius2 = 10001;
                    break;
                case 6:
                    s.color1.x = -.1f;
                    break;
                case 7:
                    s.color2.y = 1.1f;
                    break;
                case 8:
                    s.position.z = std::numeric_limits<float>::infinity();
                    break;
                case 9:
                    s.delay = std::numeric_limits<float>::quiet_NaN();
                    break;
            }
            CHECK(!fx.Init(s, error) && !error.empty() && !fx.CreateObject(0, error));
        }
        CHECK(fx.Init({}, error) && !fx.CreateObject(-1, error) && fx.CreateObject(1, error));
        CHECK(!fx.CreateObject(2, error) && !fx.Update(.5) && !fx.error.empty());
        CHECK(!fx.Update(std::numeric_limits<double>::infinity()));
        ExplosionLights lights;
        CHECK(!lights.publish(3, {}, {0, 1, 0}, std::string("a\0b", 3), 0, error) && lights.reports.empty());
        CHECK(lights.publish(24, {}, {0, 1, 0}, "bad", 0, error) && !error.empty() && lights.failed == 1);
        CHECK(lights.publish(3, {}, {0, 1, 0}, "bad clock", -1, error) && !error.empty() && lights.failed == 3);
        lights.clear();
        for (unsigned i = 0; i < 127; ++i)
            CHECK(lights.publish(3, {}, {0, 1, 0}, "pair", 0, error) && error.empty());
        CHECK(lights.publish(8, {}, {0, 1, 0}, "one", 0, error) && error.empty());
        CHECK(lights.active.size() == 255);
        CHECK(lights.publish(3, {}, {0, 1, 0}, "partial", 0, error) && !error.empty());
        CHECK(lights.active.size() == 256 && lights.started == 256 && lights.failed == 1);
        CHECK(lights.reports.size() == 64 && lights.reports.back().lights.size() == 1);
        const auto last = lights.active.back().id, event = lights.reports.back().id;
        CHECK(lights.advance(30) && lights.active.empty() && lights.expired == 256);
        lights.clear();
        CHECK(lights.publish(3, {}, {0, 1, 0}, "new", 10, error) && error.empty());
        CHECK(lights.active.front().id > last && lights.reports.front().id > event);
        CHECK(!lights.advance(9) && lights.active.empty() && lights.failed == 2 && !lights.update_error.empty());
        lights.clear();
        CHECK(lights.update_error.empty() && !lights.started && !lights.failed && !lights.expired);
        return true;
    }
    bool session()
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize()); // Deliberately no model, sprite or WAV resources.
        auto& lights = g.server.explosion_lights;
        auto h = g.objects.create(g.objects.find_class("CClientExplosionSFX"), {}, Creation::World,
                                  {{"Name", std::string("source")},
                                   {"Pos", DVector{10, 20, 30}},
                                   {"Direction", DVector{0, 3, 0}},
                                   {"ExplosionType", 3}});
        CHECK(h);
        DVector moved{100, 200, 300};
        CHECK(g.server.TeleportObject(h, &moved) == DE_OK && command(g, h) && command(g, h));
        CHECK(lights.active.size() == 4 && lights.reports.size() == 2 && lights.started == 4 && !lights.failed);
        const auto& report = g.server.scene_explosions.back();
        CHECK(report.light_event && report.light_error.empty() && !report.client_fx_available);
        CHECK(!report.model_error.empty() && !report.sprite_error.empty() && !report.sound_error.empty());
        CHECK(report.targets.null_damager && report.targets.delivered == 0);
        for (const auto& fx : lights.active)
            CHECK(fx.settings.position.x == 10 && fx.settings.position.y == 30);
        CHECK(g.step(.11) && !g.objects.get(h) && lights.active.size() == 4);
        CHECK(near(lights.active[0].radius, 228) && near(lights.active[1].radius, 99.56f));
        CHECK(near(lights.active[0].color.y, .544f) && near(lights.active[1].color.y, .20132f));
        CHECK(!g.server.PublishExplosionLights(h, 3, {}, {0, 1, 0}, error) && !error.empty());
        CHECK(lights.reports.size() == 2);
        CHECK(g.step(1.14) && lights.active.size() == 2 && lights.expired == 2);
        CHECK(g.step(1) && lights.active.size() == 2 && lights.active.front().radius < 100);
        CHECK(g.unload_world() && lights.active.empty() && lights.reports.empty());
        CHECK(!g.server.string_count() && !g.server.message_count());
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
            std::vector<ObjectId> emitters;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                if (dynamic_cast<CClientExplosionSFX*>(r->game.get()))
                {
                    emitters.push_back(id);
                    CHECK(command(g, r->handle));
                }
            }
            auto& lights = g.server.explosion_lights;
            CHECK(emitters.size() == 9 && lights.active.size() == 18 && lights.started == 18 && !lights.failed);
            CHECK(g.step(.11));
            for (auto id : emitters)
                CHECK(!g.objects.get(id));
            for (unsigned i = 0; i < 18; i += 2)
            {
                CHECK(near(lights.active[i].radius, 228) && near(lights.active[i + 1].radius, 99.56f));
                CHECK(near(lights.active[i].color.y, .544f) && near(lights.active[i + 1].color.x, .20132f));
            }
            CHECK(lights.active.front().id > previous);
            previous = lights.active.back().id;
            // Advance the real session in bounded steps; long residual lights survive models/sprites.
            for (unsigned i = 0; i < 130; ++i)
                CHECK(g.step(.01));
            CHECK(lights.active.size() == 9 && lights.expired == 9);
            for (const auto& fx : lights.active)
                CHECK(fx.settings.duration == 25 && fx.radius > 0 && fx.radius < 100);
            CHECK(g.server.explosion_models.active.empty() && g.server.explosion_sprites.active.size() == 18);
            std::cout << "SubwayA cycle " << cycle
                      << ": 18 lights, nine flashes expired, nine residuals alive; IDs through " << previous << '\n';
            CHECK(g.unload_world() && lights.active.empty() && lights.reports.empty());
            CHECK(g.objects.snapshot().empty() && !g.server.string_count() && !g.server.message_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string name = argv[1];
    return (name == "lifecycle"  ? lifecycle()
            : name == "recipes"  ? recipes()
            : name == "failures" ? failures()
            : name == "session"  ? session()
                                 : name == "private" && argc == 3 && private_world(argv[2]))
               ? 0
               : 1;
}
