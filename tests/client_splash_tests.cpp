#include "client/effect_snapshot.h"
#include "client/effect_services.h"
#include "game/original/ClientSplashSFX.h"
#include "game/server_adapter.h"
#include <iostream>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    bool detached_splash(const char* path)
    {
        GameSession game;
        Config config;
        config.data_dir = path;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        const auto type = game.objects.find_class("CClientSplashSFX");
        CHECK(type && game.objects.is_kind_of(type, game.objects.find_class("CClientSFX")));
        const auto handle = game.objects.create(type, {});
        auto* publisher = dynamic_cast<CClientSplashSFX*>(game.server.HandleToObject(handle));
        auto& effects = game.server.shotgun_effects;
        CHECK(publisher && effects.splashes.empty());
        SplashSettings settings;
        settings.position = {22, 33, 44};
        settings.texture = "SpriteTextures/blooddrop2.dtx";
        settings.radius = 128;
        settings.position_radius = 0;
        settings.direction = {0, 2, 0};
        settings.height = 50;
        settings.density = 2.5f;
        settings.spread = 0;
        settings.color1 = {255, 0, 0};
        settings.color2 = {0, 0, 255};
        settings.ripple = true;
        settings.ripple_scale = {.13f, .18f, 0};
        settings.gravity = -250;
        settings.spray_time = .2f;
        settings.duration = .5f;

        auto invalid = settings;
        invalid.texture = "SpriteTextures/not-a-splash.dtx";
        CHECK(!publisher->Setup(invalid, error) && !error.empty());
        CHECK(effects.splashes.empty() && effects.sprites.empty());
        const auto id = publisher->Setup(settings, error);
        CHECK(id && error.empty() && effects.splashes.size() == 1 && effects.sprites.size() == 1);
        CHECK(effects.splashes.front().particles.size() == 3);
        CHECK(effects.sprites.front().scale.y == .18f && effects.sprites.front().alpha == .25f);
        CHECK(game.objects.advance(.02) && !game.server.IsObjectAlive(handle));
        effects.advance(.1, game.server.effect_services());
        CHECK(effects.splashes.front().particles.size() == 6);
        const auto& first = effects.splashes.front().particles.front();
        CHECK(first.position.y == 10 && first.velocity.y == 75 && (first.color.x == 1 || first.color.z == 1));

        ClientEffectsFrame frame;
        append_runtime_effects(game.client_effects, game.client_session.world, frame);
        CHECK(frame.billboards.size() == 2 && frame.billboards.back().id.kind == EffectKind::BallisticSplash);
        const auto& visual = std::get<EffectParticles>(frame.billboards.back().value);
        CHECK(visual.texture == settings.texture && visual.origin.y == 33 && visual.half_extent == .2f);
        effects.advance(.25, game.server.effect_services());
        CHECK(effects.splashes.front().particles.size() == 6);
        effects.advance(.8, game.server.effect_services());
        CHECK(effects.splashes.empty());
        effects.advance(1.2, game.server.effect_services());
        CHECK(effects.sprites.empty());
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    return argc != 2 ? 2 : detached_splash(argv[1]) ? 0 : 1;
}
