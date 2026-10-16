#include "app/sprite_scene.h"
#include "explosion_sprite_fixture.h"
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
        return std::abs(a - b) < .0001f;
    }
    bool command(GameSession& game, HOBJECT h)
    {
        auto text = game.server.CreateString("TRIGGER");
        auto message = game.server.StartMessageToObject(nullptr, h, 1000);
        const bool ok = message && game.server.WriteToMessageHString(message, text) == DE_OK &&
                        game.server.EndMessage(message) == DE_OK;
        game.server.FreeString(text);
        return ok;
    }
    bool formulas(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_explosion_sprite::mount(resources, root, error));
        SpriteStore store(resources);
        for (unsigned wave : {0u, 1u, 2u, 255u})
            for (unsigned fade = 0; fade <= 5; ++fade)
            {
                CExplosionSpriteFX fx;
                ExplosionSpriteSettings s;
                s.scale1 = {1, 2, 0};
                s.scale2 = {5, 6, 0};
                s.duration = 1;
                s.alpha = .4f;
                s.waveform = wave;
                s.fade = fade;
                CHECK(fx.Init(s, error) && fx.CreateObject(store, 10, error));
                CHECK(near(fx.alpha, .4f) && fx.track.frame == 0 && fx.updates == 0);
                CHECK(fx.Update(10.25));
                const float expected[] = {.4f, .3f, .85f, .1f, .55f, .2f};
                CHECK(near(fx.alpha, expected[fade]));
                CHECK(near(fx.scale.x, wave == 1 ? 3.f : wave == 2 ? 1.25f : 2.f));
                CHECK(near(fx.scale.y, fx.scale.x + 1) && fx.scale.z == 0 && fx.track.milliseconds == 50 &&
                      fx.track.frame == 0);
                CHECK(fx.Update(10.75));
                if (fade == 5)
                    CHECK(near(fx.alpha, .2f));
                CHECK(!fx.Update(11) && fx.error.empty() && near(fx.scale.x, 5));
                CHECK(near(fx.alpha, fade == 0 || fade == 2 || fade == 3 ? .4f : fade == 4 ? 1.f : 0.f));
            }
        CExplosionSpriteFX fx;
        ExplosionSpriteSettings s;
        s.scale1 = s.scale2 = {1, 1, 0};
        s.duration = 1;
        s.fade = 1;
        CHECK(fx.Init(s, error) && fx.CreateObject(store, 0, error) && fx.Update(0));
        CHECK(fx.Update(.0005) && fx.updates == 1 && near(fx.track.milliseconds, .5f));
        CHECK(fx.Update(.0011) && fx.updates == 2);
        CHECK(!fx.Update(1.25) && fx.error.empty() && near(fx.alpha, -.25f));
        s.align = 1;
        s.normal = {2, 0, 0};
        CHECK(fx.Init(s, error) && fx.CreateObject(store, 0, error));
        auto forward = rotate_vector(fx.rotation, {0, 0, 1});
        CHECK(near(forward.x, 1) && near(forward.y, 0) && near(forward.z, 0));
        CHECK((fx.flags & FLAG_ROTATEABLESPRITE) != 0);
        s.normal = {0, 1, 0};
        CHECK(fx.Init(s, error) && fx.CreateObject(store, 0, error));
        forward = rotate_vector(fx.rotation, {0, 0, 1});
        CHECK(near(forward.y, 1));
        std::vector<ExplosionSpriteSettings> parts;
        std::string missing;
        for (unsigned type : {0u, 1u, 2u, 3u, 6u, 8u})
        {
            CHECK(original_explosion_sprites(type, {10, 20, 30}, {0, 2, 0}, parts, missing, error));
            CHECK(parts.size() == (type == 8 ? 1u : 2u) && !missing.empty());
            CHECK(near(parts[0].position.y, type == 6 ? 20.f : type == 8 ? 50.f : 45.f));
            CHECK(near(parts[0].duration, type == 6 ? .5f : type == 8 ? 3.f : 1.5f));
            CHECK(parts[0].align == (type == 6 ? 1 : 0));
            if (type != 8)
                CHECK(parts[1].resource == "Sprites\\smokepuff2.spr" && parts[1].waveform == 1 &&
                      near(parts[1].alpha, .85f));
        }
        CHECK(!original_explosion_sprites(999, {}, {0, 1, 0}, parts, missing, error) && parts.empty());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_explosion_sprite::mount(resources, root, error, false));
        SpriteStore store(resources);
        for (int kind = 0; kind < 8; ++kind)
        {
            ExplosionSpriteSettings s;
            CExplosionSpriteFX fx;
            if (kind == 0)
                s.duration = 0;
            if (kind == 1)
                s.duration = std::numeric_limits<float>::quiet_NaN();
            if (kind == 2)
                s.fade = 6;
            if (kind == 3)
                s.align = 2;
            if (kind == 4)
                s.alpha = 2;
            if (kind == 5)
                s.scale1.x = -1;
            if (kind == 6)
                s.resource = std::string("a\0b", 3);
            if (kind == 7)
                s.duration = 601;
            CHECK(!fx.Init(s, error) && !error.empty() && !fx.sprite);
        }
        CExplosionSpriteFX fx;
        ExplosionSpriteSettings s;
        s.resource = "";
        CHECK(fx.Init(s, error) && !fx.CreateObject(store, 0, error));
        s.resource.reset();
        s.align = 1;
        s.normal = {};
        CHECK(fx.Init(s, error) && !fx.CreateObject(store, 0, error));
        s.align = 0;
        CHECK(fx.Init(s, error) && fx.CreateObject(store, 1, error));
        CHECK(!fx.Update(.9) && !fx.error.empty());
        ExplosionSprites effects;
        CHECK(effects.publish(1, {}, {0, 2, 0}, "partial", store, 0, error));
        CHECK(effects.active.size() == 1 && effects.started == 1 && effects.failed == 1 && !error.empty());
        CHECK(effects.reports.back().sprites.size() == 1 &&
              effects.reports.back().unavailable == "model, particle ring, lights");
        CHECK(effects.publish(24, {}, {0, 1, 0}, "unsupported", store, 0, error) && !error.empty());
        CHECK(effects.publish(1, {}, {}, "zero normal", store, 0, error) && !error.empty());
        const auto old_id = effects.active.front().id;
        effects.clear();
        CHECK(effects.active.empty() && effects.reports.empty());
        CHECK(effects.publish(8, {}, {0, 1, 0}, "new", store, 0, error) && error.empty() &&
              effects.active.front().id > old_id);
        for (unsigned i = 1; i < 256; ++i)
            CHECK(effects.publish(8, {}, {0, 1, 0}, "budget", store, 0, error) && error.empty());
        CHECK(effects.active.size() == 256 && effects.reports.size() == 64);
        CHECK(effects.publish(8, {}, {0, 1, 0}, "full", store, 0, error) && !error.empty() &&
              effects.active.size() == 256);
        CHECK(effects.advance(3) && effects.active.empty() && effects.expired == 256);
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession game;
        std::string error;
        CHECK(game.initialize());
        CHECK(test_explosion_sprite::mount(game.resources, root, error));
        auto h = game.objects.create(game.objects.find_class("CClientExplosionSFX"), {}, Creation::World,
                                     {{"Name", std::string("emitter")},
                                      {"Pos", DVector{10, 20, 30}},
                                      {"Direction", DVector{0, 3, 0}},
                                      {"ExplosionType", 1},
                                      {"WaveFile", std::string("")}});
        CHECK(h && command(game, h) && command(game, h));
        auto& effects = game.server.explosion_sprites;
        CHECK(effects.active.size() == 4 && effects.started == 4 && effects.reports.size() == 2);
        CHECK(effects.reports[0].id != effects.reports[1].id);
        CHECK(near(effects.active[0].settings.position.y, 45) && near(effects.active[0].settings.normal.y, 1));
        CHECK(game.server.scene_explosions.back().sprite_error.empty() &&
              !game.server.scene_explosions.back().client_fx_available);
        CHECK(game.server.scene_explosions.back().targets.delivered == 0);
        CHECK(game.step(.11) && !game.objects.get(h) && effects.active.size() == 4);
        CHECK(effects.active[0].track.frame == 1 && near(effects.active[0].alpha, 1 - .11f / 1.5f));
        CHECK(near(effects.active[1].scale.x, .5f + 1.5f * std::sqrt(.11f / 5)));
        SpriteView view;
        CHECK(update_sprite_scene(game, view, error) && view.draws.size() == 4);
        for (const auto& entry : view.entries)
            CHECK(entry.client_effect && entry.error.empty() && entry.alpha < 1);
        const auto clock = effects.active[0].track.milliseconds;
        CHECK(update_sprite_scene(game, view, error) && effects.active[0].track.milliseconds == clock);
        CHECK(game.step(1.39) && effects.active.size() == 2 && effects.expired == 2);
        CHECK(game.step(3.5) && effects.active.empty() && effects.expired == 4);
        CHECK(game.unload_world() && effects.reports.empty() && effects.started == 0);
        CHECK(game.server.string_count() == 0 && game.server.message_count() == 0);
        return true;
    }
    bool private_world(const std::filesystem::path& root)
    {
        GameSession game;
        std::string error;
        Config config;
        config.data_dir = root;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        SpriteView view;
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error));
            unsigned count = 0;
            for (auto id : game.objects.snapshot())
            {
                auto* r = game.objects.get(id);
                if (dynamic_cast<CClientExplosionSFX*>(r->game.get()))
                {
                    CHECK(command(game, r->handle));
                    ++count;
                    const auto& report = game.server.explosion_sprites.reports.back();
                    std::cout << "type=" << report.type << " sprites=" << report.sprites.size()
                              << " error=" << report.error << " missing=" << report.unavailable << "\n";
                    CHECK(report.error.empty() && report.sprites.size() == 2);
                }
            }
            CHECK(count == 9 && game.server.explosion_sprites.active.size() == 18 && game.step(.11));
            CHECK(update_sprite_scene(game, view, error) && view.draws.size() == 30);
            for (const auto& entry : view.entries)
                CHECK(entry.error.empty());
            CHECK(game.server.explosion_sprites.active.front().id > previous);
            previous = game.server.explosion_sprites.active.back().id;
            CHECK(game.step(.3));
            CHECK(update_sprite_scene(game, view, error) && view.draws.size() == 30);
            CHECK(game.unload_world() && game.server.explosion_sprites.active.empty() &&
                  game.server.explosion_sprites.reports.empty());
            view.clear();
            CHECK(update_sprite_scene(game, view, error) && view.entries.empty());
            CHECK(game.server.string_count() == 0 && game.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    return (name == "formulas"   ? formulas(argv[2])
            : name == "failures" ? failures(argv[2])
            : name == "session"  ? session(argv[2])
                                 : name == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
