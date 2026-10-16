#include "game/game_settings.h"
#include "game/original/ClientLightFX.h"
#include "game/original/KeyFramer.h"
#include "player_fixture.h"
#include <functional>
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
    constexpr float pi = 3.14159265358979323846f;
    bool close(float a, float b)
    {
        return std::abs(a - b) < .001f;
    }
    bool close(DVector a, DVector b)
    {
        return close(a.x, b.x) && close(a.y, b.y) && close(a.z, b.z);
    }
    HOBJECT create(GameSession& g, const char* type = "ClientLightFX", Properties p = {})
    {
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, p);
    }
    ClientLightFX* light(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<ClientLightFX*>(g.server.HandleToObject(h));
    }
    bool command(GameSession& g, HOBJECT h, const char* value)
    {
        auto text = g.server.CreateString(value);
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(h), h, 1000);
        auto written = g.server.WriteToMessageHString(m, text);
        auto sent = g.server.EndMessage(m);
        g.server.FreeString(text);
        return written == DE_OK && sent == DE_OK;
    }
    bool hit(GameSession& g, HOBJECT h, float amount)
    {
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(h), h, damage_message);
        Damage d{{}, {}, amount, DamageType::Normal, h};
        return m && g.server.WriteDamage(m, d) == DE_OK && g.server.EndMessage(m) == DE_OK;
    }
    bool defaults(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto h = create(g);
        auto* s = light(g, h);
        auto* c = g.server.GetLightEffect(h);
        CHECK(s && c && !s->dynamic && s->on && s->lifetime == 0 && s->damage.m_fHitPoints == 1);
        CHECK(s->initial_delay >= .11f && s->initial_delay <= .6f && g.objects.get(h)->scheduled);
        CHECK(s->settings.radius.min == 200 && s->settings.radius.max == 500 && s->settings.radius.frequency == 4);
        CHECK(close(s->settings.color.x, 255.0f / 511) && close(c->color.x, .5f + 255.0f / 511) && c->radius == 500);
        CHECK((g.objects.get(h)->user_flags & 9) == 9 && !(g.objects.get(h)->state.m_Flags & FLAG_FORCECLIENTUPDATE));
        CHECK(!g.server.string_count());
        const char* names[] = {"SquareWaveLightFX",   "SawWaveLightFX",      "RampUpWaveLightFX",
                               "RampDownWaveLightFX", "SineWaveLightFX",     "Flicker1WaveLightFX",
                               "Flicker2WaveLightFX", "Flicker3WaveLightFX", "Flicker4WaveLightFX",
                               "StrobeWaveLightFX",   "SearchWaveLightFX",   "FlickerLight",
                               "GlowingLight"};
        for (unsigned i = 0; i < 13; ++i)
        {
            auto a = create(g, names[i]);
            CHECK(a);
            auto expected = i < 11 ? i + 1 : i == 11 ? 7 : 5;
            CHECK(light(g, a)->settings.intensity.kind == expected && light(g, a)->settings.radius.kind == expected);
        }
        auto runtime = g.objects.create(g.objects.find_class("GlowingLight"), {}, Creation::Runtime);
        CHECK(runtime);
        CHECK(light(g, runtime)->dynamic && light(g, runtime)->lifetime == -1 &&
              light(g, runtime)->initial_delay == .1f && light(g, runtime)->settings.radius.kind == WAVE_NONE &&
              light(g, runtime)->settings.radius.min == 0 && light(g, runtime)->settings.radius.frequency == 0);
        auto str = g.objects.create_props(g.objects.find_class("GlowingLight"), {},
                                          "IntensityPhase 90; RadiusPhase -90; RampUpSound sounds/a.wav; HitPoints 4");
        CHECK(str);
        CHECK(close(light(g, str)->settings.intensity.phase, pi / 2) &&
              close(light(g, str)->settings.radius.phase, -pi / 2) && light(g, str)->damage.m_fHitPoints == 4);
        CHECK(g.server.GetLightEffect(str)->settings.ramp_up_sound == "sounds/a.wav" && !g.server.string_count());
        CHECK(!g.objects.create(g.objects.find_class("ClientLightFX"), {}, Creation::Restore));
        CHECK(g.unload_world() && g.server.light_effects.empty() && !CDestructable::m_dwNumDestructables);
        return true;
    }
    bool waves(const std::filesystem::path&)
    {
        float v = 99;
        auto sample = [&](DBYTE w, float angle, float expected)
        { return light_wave_value(w, 10, 50, angle, v) && close(v, expected); };
        CHECK(sample(WAVE_NONE, 0, 50) && sample(WAVE_SQUARE, pi, 10) && sample(WAVE_SQUARE, pi + .01f, 50));
        CHECK(sample(WAVE_SAW, pi / 2, 30) && sample(WAVE_SAW, pi, 50) && sample(WAVE_SAW, 3 * pi / 2, 30));
        CHECK(sample(WAVE_RAMPUP, pi, 30) && sample(WAVE_RAMPDOWN, pi / 2, 40));
        CHECK(sample(WAVE_SINE, 0, 30) && sample(WAVE_SINE, pi / 2, 50) && sample(WAVE_SINE, 3 * pi / 2, 10));
        CHECK(sample(WAVE_FLICKER1, 0, 10) && sample(WAVE_FLICKER1, 2 * pi * 2.5f / 63, 50));
        CHECK(sample(WAVE_FLICKER2, 0, 20) && sample(WAVE_FLICKER2, 2 * pi * 2.5f / 63, 50));
        CHECK(sample(WAVE_FLICKER3, 2 * pi * 4.5f / 63, 40) && sample(WAVE_FLICKER4, 2 * pi * 16.5f / 127, 50));
        CHECK(sample(WAVE_STROBE, 0, 50) && sample(WAVE_STROBE, 2 * pi * 3.5f / 63, 40));
        CHECK(sample(WAVE_SEARCH, pi / 2, 50) && sample(WAVE_SEARCH, 3 * pi / 2, 10));
        for (DBYTE w = 0; w <= WAVE_SEARCH; ++w)
        {
            CHECK(light_wave_value(w, 10, 50, -pi / 2, v));
            float pos = 0;
            CHECK(light_wave_value(w, 10, 50, 3 * pi / 2, pos) && close(pos, v));
            CHECK(light_wave_value(w, 10, 50, -1e-30f, v) && std::isfinite(v));
        }
        v = 99;
        CHECK(!light_wave_value(12, 0, 1, 0, v) && v == 99);
        CHECK(!light_wave_value(WAVE_SINE, 0, 1, std::numeric_limits<float>::infinity(), v) && v == 99);
        GameSession g;
        CHECK(g.initialize());
        auto h = create(g, "ClientLightFX",
                        {{"Color", DVector{511, -511, 0}},
                         {"IntensityMin", -10.0f},
                         {"IntensityMax", 300.0f},
                         {"RadiusMin", -4.0f},
                         {"RadiusMax", 20000.0f},
                         {"LifeTime", -1.0f}});
        CHECK(h && light(g, h)->settings.intensity.min == 0 && light(g, h)->settings.intensity.max == 255 &&
              light(g, h)->settings.radius.min == 0 && light(g, h)->lifetime == 0);
        CHECK(close(g.server.GetLightEffect(h)->color, {1, 0, .5f}) && g.server.GetLightEffect(h)->radius == 10000);
        return true;
    }
    bool commands(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto h = create(g);
        CHECK(h && command(g, h, "off") && light(g, h)->on);
        CHECK(command(g, h, "OFFsuffix") && !light(g, h)->on && g.step(.1));
        auto* c = g.server.GetLightEffect(h);
        CHECK(c && !c->visible && c->radius == 200 && close(c->off_start_time, .1f));
        auto color = c->color;
        CHECK(command(g, h, "ONanything") && g.step(.1));
        c = g.server.GetLightEffect(h);
        CHECK(c->visible && c->radius == 200 && close(c->color, color)); // NONE does not restore max radius.
        CHECK(command(g, h, "TOGGLE extra") && !light(g, h)->on && command(g, h, "TOGGLE") && light(g, h)->on);
        auto wave = create(g, "GlowingLight", {{"RadiusMin", 0.0f}, {"RadiusMax", 100.0f}, {"RadiusFreq", 1.0f}});
        CHECK(wave);
        CHECK(command(g, wave, "OFF") && g.step(.5) && command(g, wave, "ON") && g.step(.1));
        CHECK(close(g.server.GetLightEffect(wave)->radius,
                    50 * (1 + std::sin(.8f)))); // Absolute session time, not time since ON.
        CHECK(!g.server.string_count() && !g.server.message_count());
        return true;
    }
    bool lifetime(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto runtime = g.objects.create(g.objects.find_class("ClientLightFX"), {}, Creation::Runtime);
        CHECK(runtime);
        light(g, runtime)->lifetime = .1f;
        CHECK(g.step(.099) && g.objects.get(runtime));
        CHECK(g.step(.0011) && !g.objects.get(runtime) && g.server.light_effects.empty());
        auto a = create(g, "ClientLightFX", {{"LifeTime", .2f}});
        auto b = create(g, "ClientLightFX", {{"LifeTime", .2f}, {"LightSwitch", false}});
        CHECK(a && b);
        CHECK(!g.objects.get(b)->scheduled && !(g.objects.get(b)->user_flags & 9));
        CHECK(command(g, a, "OFF") && command(g, b, "ON") && !g.objects.get(b)->scheduled);
        CHECK(g.step(.6) && !g.objects.get(a) && g.objects.get(b) && g.server.GetLightEffect(b)->visible);
        CHECK(g.step(.6) && g.objects.get(b));
        return true;
    }
    bool damage(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false, true, true));
        auto other = create(g, "ClientLightFX", {{"Name", std::string("other")}});
        auto h = create(g, "ClientLightFX",
                        {{"HitPoints", 5.0f},
                         {"DeathTriggerTarget", std::string("other")},
                         {"DeathTriggerMessage", std::string("KILL")},
                         {"SpawnObject", std::string("HealthPU")},
                         {"SpawnObjectVel", DVector{1, 2, 3}}});
        CHECK(h && other);
        CHECK(hit(g, h, 2) && light(g, h)->damage.m_fHitPoints == 3 && !light(g, h)->damage.IsDead());
        CHECK(hit(g, h, 3) && g.objects.get(h)->life == Life::Pending && g.objects.get(other)->life == Life::Pending);
        CHECK(!g.server.GetLightEffect(h) && !g.server.GetLightEffect(other));
        CHECK(g.objects.finish_frame() && g.server.light_effects.empty());
        unsigned pickups = 0;
        for (auto id : g.objects.snapshot())
            if (g.objects.get(id)->type->name == "HealthPU")
            {
                ++pickups;
                CHECK(close(g.objects.get(id)->velocity, {1, 2, 3}));
            }
        CHECK(pickups == 1);
        auto immune = create(g, "ClientLightFX", {{"CanDamage", false}});
        auto trigger = create(g, "ClientLightFX", {{"TriggerDestroyOnly", true}});
        CHECK(immune && trigger);
        CHECK(hit(g, immune, 10) && command(g, immune, "KILL") && g.objects.get(immune)->life == Life::Alive);
        CHECK(hit(g, trigger, 10) && g.objects.get(trigger)->life == Life::Alive && command(g, trigger, "kill") &&
              g.objects.get(trigger)->life == Life::Pending);
        CHECK(g.unload_world() && !CDestructable::m_dwNumDestructables && g.server.light_effects.empty() &&
              !g.server.string_count() && !g.server.message_count());
        return true;
    }
    bool validation(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto other = create(g, "BaseClass");
        LightSettings s;
        CHECK(!g.server.PublishLightEffect(other, s));
        for (const char* name : {"HitPoints", "LifeTime", "RadiusMin", "RadiusMax", "RadiusFreq", "RadiusPhase",
                                 "IntensityMin", "IntensityMax", "IntensityFreq", "IntensityPhase"})
            CHECK(!create(g, "ClientLightFX", {{name, std::numeric_limits<float>::quiet_NaN()}}));
        CHECK(!create(g, "ClientLightFX", {{"RadiusWaveform", std::int32_t(-1)}}));
        CHECK(!create(g, "ClientLightFX", {{"IntensityWaveform", std::int32_t(12)}}));
        CHECK(!create(g, "ClientLightFX", {{"SpawnObject", std::string("Unported")}}));
        auto h = create(g, "ClientLightFX",
                        {{"CastShadowsFlag", true},
                         {"SolidLightFlag", true},
                         {"OnlyLightWorldFlag", true},
                         {"DontLightBackfacingFlag", true},
                         {"FogLightFlag", true}});
        CHECK(h && light(g, h)->settings.flags == 0xf8);
        CHECK(!g.server.PublishLightEffect(h, s) && g.server.light_effects.size() == 1);
        float v = 4;
        CHECK(!g.server.RandomFloat(2, 1, v) && v == 4);
        CHECK(!g.server.RandomFloat(0, std::numeric_limits<float>::infinity(), v) && v == 4);
        CHECK(g.server.RandomFloat(2, 2, v) && v == 2);
        auto before = *g.server.GetLightEffect(h);
        CHECK(!before.Update(std::numeric_limits<float>::infinity(), {}, true) && before.updates == 0);
        s.flags = 1;
        CHECK(!valid_light_settings(s));
        s.flags = 0;
        s.ramp_up_sound = std::string("a\0b", 3);
        CHECK(!valid_light_settings(s));
        CHECK(g.objects.remove(h) && !g.server.GetLightEffect(h) && !g.server.PublishLightEffect(h, {}));
        CHECK(g.objects.finish_frame() && g.server.light_effects.empty() && !CDestructable::m_dwNumDestructables);
        return true;
    }
    bool motion(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto a = create(g, "GlowingLight", {{"Name", std::string("lights")}, {"Pos", DVector{10, 100, 0}}});
        auto b = create(g, "GlowingLight", {{"Name", std::string("lights")}, {"Pos", DVector{20, 100, 0}}});
        CHECK(a && b);
        CHECK(create(g, "Key", {{"Name", std::string("k0")}, {"Pos", DVector{0, 100, 0}}}));
        CHECK(create(g, "Key", {{"Name", std::string("k1")}, {"Pos", DVector{100, 100, 0}}, {"TimeStamp", 1.0f}}));
        auto f =
            create(g, "KeyFramer",
                   {{"ObjectName", std::string("lights")}, {"BaseKeyName", std::string("k")}, {"StartActive", true}});
        CHECK(f && g.step(.01) && g.step(.25));
        CHECK(close(g.server.GetLightEffect(a)->position, {35, 100, 0}) &&
              close(g.server.GetLightEffect(b)->position, {45, 100, 0}));
        CHECK(command(g, a, "KILL") && !g.server.GetLightEffect(a) && g.step(.25));
        CHECK(close(g.server.GetLightEffect(b)->position, {70, 100, 0}) && g.server.light_effects.size() == 1);
        auto* k = dynamic_cast<KeyFramer*>(g.server.HandleToObject(f));
        CHECK(k && k->targets.size() == 1 && k->targets[0].object == b);
        return true;
    }
    bool teardown(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        float delay = 0;
        for (int cycle = 0; cycle < 4; ++cycle)
        {
            auto a = create(g);
            CHECK(a);
            if (!cycle)
                delay = light(g, a)->initial_delay;
            else
                CHECK(delay == light(g, a)->initial_delay);
            auto b = create(g, "GlowingLight", {{"TypeSingle", false}});
            CHECK(b && g.objects.get(b)->life == Life::Pending && !g.server.GetLightEffect(b));
            CHECK(g.step(.01) && !g.objects.get(b) && g.server.light_effects.size() == 1 &&
                  CDestructable::m_dwNumDestructables == 1);
            CHECK(g.unload_world() && g.server.light_effects.empty() && !CDestructable::m_dwNumDestructables &&
                  !g.server.string_count() && !g.server.message_count() && !g.server.list_count());
        }
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        unsigned worlds = 0, count = 0, off = 0, filtered = 0;
        std::map<std::string, unsigned> classes;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World world;
            CHECK(g.resources.read(path, bytes, error));
            if (!read_world(bytes, world, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& o : world.objects)
            {
                auto* cls = g.objects.find_class(o.class_name);
                if (!cls)
                    continue;
                bool matches = false;
                for (auto* c = cls; c; c = c->parent)
                    matches |= c->name == "ClientLightFX";
                if (!matches)
                    continue;
                Properties props;
                for (const auto& p : o.properties)
                    props.emplace(p.name, p.value);
                auto h = create(g, o.class_name.c_str(), props);
                if (!h)
                    std::cerr << path << ' ' << o.class_name << ": " << g.objects.diagnostics().back() << '\n';
                CHECK(h);
                ++count;
                ++classes[o.class_name];
                off += !light(g, h)->on;
                filtered += g.objects.get(h)->life != Life::Alive;
            }
            CHECK(g.step(.01));
            for (auto id : g.objects.snapshot())
                if (auto h = g.objects.get(id)->handle; light(g, h))
                {
                    CHECK(g.server.GetLightEffect(h) && command(g, h, "OFF"));
                }
            CHECK(g.step(.02));
            for (auto id : g.objects.snapshot())
                if (auto h = g.objects.get(id)->handle; light(g, h))
                {
                    CHECK(!g.server.GetLightEffect(h)->visible && command(g, h, "ON"));
                }
            CHECK(g.step(.02) && g.objects.clear() && g.server.light_effects.empty() &&
                  !CDestructable::m_dwNumDestructables && !g.server.string_count());
        }
        std::cout << "worlds=" << worlds << " lights=" << count << " initially_off=" << off << " filtered=" << filtered
                  << '\n';
        for (const auto& [c, n] : classes)
            std::cout << c << '=' << n << '\n';
        CHECK(worlds == 57 && count == 72 && off == 2 && !filtered && classes["ClientLightFX"] == 16 &&
              classes["Flicker3WaveLightFX"] == 1 && classes["Flicker4WaveLightFX"] == 3 &&
              classes["FlickerLight"] == 9 && classes["GlowingLight"] == 42 && classes["StrobeWaveLightFX"] == 1);
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            CHECK(g.objects.snapshot().size() == 197 && g.server.light_effects.size() == 12);
            CHECK(g.step(1.0 / 60) && g.objects.snapshot().size() == 189 && g.objects.attachments().size() == 1);
            KeyFramer* path = nullptr;
            for (auto id : g.objects.snapshot())
                if (auto* f = dynamic_cast<KeyFramer*>(g.objects.get(id)->game.get());
                    f && f->object_name == "sublight")
                    path = f;
            CHECK(path && path->active && path->targets.size() == 12);
            auto h = path->targets[0].object;
            auto pos = g.server.GetLightEffect(h)->position;
            auto c0 = g.server.GetLightEffect(h)->color;
            auto r0 = g.server.GetLightEffect(h)->radius;
            CHECK(g.step(.1));
            auto* fx = g.server.GetLightEffect(h);
            CHECK(fx && fx->visible && fx->updates >= 2 && !close(pos, fx->position) &&
                  close(fx->position, g.objects.get(h)->state.m_Pos));
            std::cout << "Subway cycle=" << cycle << " targets=" << path->targets.size() << " radius=" << r0 << "->"
                      << fx->radius << " color=" << c0.x << "->" << fx->color.x << " moved=" << path->targets[0].moves
                      << '\n';
            CHECK(command(g, h, "OFF") && g.step(.1) && !g.server.GetLightEffect(h)->visible);
            CHECK(command(g, h, "ON") && g.step(.1) && g.server.GetLightEffect(h)->visible);
            CHECK(g.unload_world() && g.server.light_effects.empty() && !CDestructable::m_dwNumDestructables &&
                  !g.server.string_count() && !g.server.message_count() && !g.server.list_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"defaults", defaults}, {"waves", waves},       {"commands", commands},
        {"lifetime", lifetime}, {"damage", damage},     {"validation", validation},
        {"motion", motion},     {"teardown", teardown}, {"catalog", catalog}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
