#include "app/model_scene.h"
#include "explosion_model_fixture.h"
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
    bool formulas(const std::filesystem::path& root)
    {
        Resources resources;
        ModelStore store(resources);
        std::string error;
        std::minstd_rand random(1);
        CHECK(test_explosion_model::mount(resources, root, error));
        for (unsigned wave : {0u, 1u, 2u, 3u, 4u, 5u, 255u})
            for (unsigned fade = 0; fade < 6; ++fade)
            {
                ExplosionModelSettings s;
                s.scale1 = {1, 2, 3};
                s.scale2 = {5, 6, 7};
                s.duration = 1;
                s.alpha = .4f;
                s.waveform = wave;
                s.fade = fade;
                CExplosionModelFX fx;
                CHECK(fx.Init(s, error) && fx.CreateObject(store, resources, 10, random, error));
                CHECK(fx.flags & FLAG_NOLIGHT);
                CHECK(near(fx.alpha, .4f));
                CHECK(fx.Update(10));
                CHECK(near(fx.alpha, fade == 2 ? 1.f : fade == 3 || fade == 5 ? 0.f : .4f));
                CHECK(fx.Update(10.25));
                const float alphas[] = {.4f, .3f, .85f, .1f, .55f, .2f};
                CHECK(near(fx.alpha, alphas[fade]));
                const float amount = wave == 1   ? .5f
                                     : wave == 2 ? .0625f
                                     : wave == 3 ? .5f
                                     : wave == 4 ? std::sqrt(.5f)
                                     : wave == 5 ? .25f
                                                 : .25f;
                CHECK(near(fx.scale.x, 1 + 4 * amount));
                CHECK(near(fx.scale.y, fx.scale.x + 1));
                CHECK(fx.Update(10.75));
                if (wave >= 3 && wave <= 5)
                    CHECK(near(fx.scale.x, 1 + 4 * amount));
                CHECK(!fx.Update(11) && fx.error.empty());
                CHECK(near(fx.scale.x, wave >= 3 && wave <= 5 ? 1.f : 5.f));
                CHECK(near(fx.alpha, fade == 0 || fade == 2 || fade == 3 ? .4f : fade == 4 ? 1.f : 0.f));
            }
        ExplosionModelSettings s;
        s.scale1 = s.scale2 = {1, 1, 1};
        s.rotations = {std::numbers::pi_v<float> / 2, 0, 0};
        CExplosionModelFX fx;
        CHECK(fx.Init(s, error) && fx.CreateObject(store, resources, 0, random, error));
        CHECK(fx.Update(0));
        auto f = rotate_vector(fx.rotation, {0, 0, 1});
        CHECK(near(f.x, 1) && near(f.z, 0));
        CHECK(fx.Update(.0005) && fx.updates == 1);
        CHECK(fx.Update(.01) && fx.updates == 2);
        f = rotate_vector(fx.rotation, {0, 0, 1});
        CHECK(near(f.z, -1));
        s.rotations = {std::numbers::pi_v<float> / 2, std::numbers::pi_v<float> / 2, std::numbers::pi_v<float> / 2};
        CHECK(fx.Init(s, error) && fx.CreateObject(store, resources, 0, random, error) && fx.Update(0));
        f = rotate_vector(fx.rotation, {0, 0, 1});
        CHECK(near(f.x, 0) && near(f.y, 1) && near(f.z, 0));
        s.rotations = {};
        s.random_rotation = 2;
        s.normal = {2, 0, 0};
        CHECK(fx.Init(s, error) && fx.CreateObject(store, resources, 0, random, error));
        f = rotate_vector(fx.rotation, {0, 0, 1});
        CHECK(near(f.x, 1) && near(f.y, 0) && near(f.z, 0));
        s.random_rotation = 1;
        std::minstd_rand a(7), b(7);
        CExplosionModelFX one, two;
        CHECK(one.Init(s, error) && two.Init(s, error) && one.CreateObject(store, resources, 0, a, error) &&
              two.CreateObject(store, resources, 0, b, error));
        std::minstd_rand expected(7);
        expected();
        expected();
        expected(); // Source random Euler orientation consumes exactly three draws.
        CHECK(a == expected && b == expected);
        CHECK(one.rotation.m_Vec.x == two.rotation.m_Vec.x && a() == b());
        return true;
    }
    bool recipes(const std::filesystem::path&)
    {
        std::minstd_rand random(1);
        std::string error;
        std::optional<ExplosionModelSettings> s;
        for (unsigned type : {0u, 1u, 2u, 3u, 6u, 8u})
        {
            CHECK(original_explosion_model(type, {10, 20, 30}, {0, 2, 0}, random, s, error));
            if (type == 6)
            {
                CHECK(!s);
                continue;
            }
            CHECK(s && near(s->position.y, type == 8 ? 50.f : 30.f));
            CHECK(near(s->scale1.x, type == 0 ? 25.f : type == 2 ? 100.f : 50.f));
            CHECK(near(s->scale2.x, type == 0 ? 200.f : type == 2 ? 600.f : type == 8 ? 1000.f : 400.f));
            CHECK(s->waveform <= 2 && s->fade == 1 && s->random_rotation == 1 && near(s->alpha, .75f));
            CHECK(s->model.has_value() == (type == 8));
        }
        CHECK(!original_explosion_model(24, {}, {0, 1, 0}, random, s, error) && !s && !error.empty());
        CHECK(!original_explosion_model(3, {}, {}, random, s, error));
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources resources;
        ModelStore store(resources);
        std::string error;
        std::minstd_rand random(1);
        CHECK(test_explosion_model::mount(resources, root, error));
        for (unsigned kind = 0; kind < 7; ++kind)
        {
            CExplosionModelFX fx;
            ExplosionModelSettings s;
            if (kind == 0)
                s.duration = 0;
            if (kind == 1)
                s.duration = 601;
            if (kind == 2)
                s.alpha = 2;
            if (kind == 3)
                s.fade = 6;
            if (kind == 4)
                s.random_rotation = 3;
            if (kind == 5)
                s.scale1.x = -1;
            if (kind == 6)
                s.model = std::string("a\0b", 3);
            CHECK(!fx.Init(s, error));
        }
        ExplosionModelSettings s;
        CExplosionModelFX fx;
        s.model = "";
        CHECK(fx.Init(s, error) && !fx.CreateObject(store, resources, 0, random, error));
        s.model.reset();
        s.skin = "absent.dtx";
        CHECK(fx.Init(s, error) && !fx.CreateObject(store, resources, 0, random, error));
        s.skin.reset();
        s.random_rotation = 2;
        s.normal = {};
        CHECK(fx.Init(s, error) && !fx.CreateObject(store, resources, 0, random, error));
        s.random_rotation = 0;
        CHECK(fx.Init(s, error) && fx.CreateObject(store, resources, 1, random, error));
        CHECK(!fx.Update(.9) && !fx.error.empty());
        ExplosionModels effects;
        CHECK(effects.publish(6, {}, {0, 1, 0}, "none", store, resources, 0, error) && error.empty() &&
              effects.active.empty());
        for (unsigned i = 0; i < 128; ++i)
            CHECK(effects.publish(3, {}, {0, 1, 0}, "budget", store, resources, 0, error) && error.empty());
        CHECK(effects.active.size() == 128 && effects.reports.size() == 64);
        CHECK(effects.publish(3, {}, {0, 1, 0}, "full", store, resources, 0, error) && !error.empty() &&
              effects.failed == 1);
        auto previous = effects.active.back().id;
        CHECK(effects.advance(.5) && effects.active.empty() && effects.expired == 128);
        effects.clear();
        CHECK(effects.publish(3, {}, {0, 1, 0}, "new", store, resources, 1, error) && error.empty());
        CHECK(effects.active[0].id > previous);
        CHECK(!effects.advance(.5) && effects.active.empty() && !effects.update_error.empty());
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize() && test_explosion_model::mount(g.resources, root, error));
        auto h = g.objects.create(g.objects.find_class("CClientExplosionSFX"), {}, Creation::World,
                                  {{"Name", std::string("emitter")},
                                   {"Pos", DVector{10, 20, 30}},
                                   {"Direction", DVector{0, 3, 0}},
                                   {"ExplosionType", 3},
                                   {"WaveFile", std::string("")}});
        CHECK(h && command(g, h) && command(g, h));
        auto& effects = g.server.explosion_models;
        CHECK(effects.active.size() == 2 && effects.started == 2 && g.server.explosion_sprites.active.size() == 4);
        CHECK(g.server.scene_explosions.back().model_error.empty() &&
              !g.server.scene_explosions.back().client_fx_available);
        CHECK(g.step(.11) && !g.objects.get(h) && effects.active.size() == 2);
        ModelView scene;
        CHECK(update_explosion_model_scene(g, scene, error) && scene.world_draws.size() == 2);
        for (const auto& e : scene.entries)
            CHECK(e.error.empty() && e.client_effect && e.triangles == 1 && near(e.alpha, .75f * (1 - .11f / .5f)));
        auto clock = effects.active[0].animation.time;
        CHECK(update_explosion_model_scene(g, scene, error) && clock == effects.active[0].animation.time);
        CHECK(g.step(.39) && effects.active.empty() && effects.expired == 2 &&
              g.server.explosion_sprites.active.size() == 4);
        CHECK(update_explosion_model_scene(g, scene, error) && scene.entries.empty());
        CHECK(g.unload_world() && effects.reports.empty() && g.server.string_count() == 0 &&
              g.server.message_count() == 0);
        return true;
    }
    bool private_world(const std::filesystem::path& root)
    {
        GameSession g;
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        ModelView scene;
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            unsigned n = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                if (dynamic_cast<CClientExplosionSFX*>(r->game.get()))
                {
                    CHECK(command(g, r->handle));
                    ++n;
                }
            }
            auto& effects = g.server.explosion_models;
            if (effects.failed)
                for (const auto& report : effects.reports)
                    std::cerr << report.error << "\n";
            CHECK(n == 9 && effects.started == 9 && effects.failed == 0 && g.step(.11));
            CHECK(update_explosion_model_scene(g, scene, error) && scene.world_draws.size() == 9);
            for (const auto& e : scene.entries)
            {
                std::cout << e.label << " triangles=" << e.triangles << " skin=" << e.skin << " alpha=" << e.alpha
                          << " error=" << e.error << "\n";
                CHECK(e.error.empty() && e.triangles > 0 && e.alpha < .75f);
            }
            CHECK(effects.active.front().id > previous);
            previous = effects.active.back().id;
            CHECK(g.step(.4) && effects.active.empty() && effects.expired == 9);
            CHECK(g.unload_world() && effects.reports.empty());
            scene.clear();
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 &&
                  update_explosion_model_scene(g, scene, error) && scene.entries.empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string name = argv[1];
    return (name == "formulas"   ? formulas(argv[2])
            : name == "recipes"  ? recipes(argv[2])
            : name == "failures" ? failures(argv[2])
            : name == "session"  ? session(argv[2])
                                 : name == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
