#include "client/effect_services.h"
#include "game/original/ClientWeaponSFX.h"
#include "game/server_adapter.h"
#include <iostream>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    bool selective_effects(const char* path)
    {
        GameSession game;
        Config config;
        config.data_dir = path;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        auto& floor = game.world.models.emplace_back();
        floor.points = {{-400, 0, -400}, {-400, 0, 400}, {400, 0, 400}, {400, 0, -400}};
        floor.surfaces.resize(1);
        floor.surfaces.front().flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(game.collision.build(floor, error));
        game.server.collision_ready = true;

        const auto type = game.objects.find_class("CClientWeaponSFX");
        CHECK(type && game.objects.is_kind_of(type, game.objects.find_class("CClientSFX")));
        const auto handle = game.objects.create(type, {});
        auto* emitter = dynamic_cast<CClientWeaponSFX*>(game.server.HandleToObject(handle));
        CHECK(emitter);
        auto& ballistics = game.server.shotgun_effects;
        auto& sprites = game.server.weapon_sprites;

        LegacyWeaponEffect settings;
        settings.shot.source = {0, 200, 0};
        settings.shot.destination = {0, 180, 100};
        settings.shot.forward = {0, 0, 1};
        settings.shot.normal = {0, -1, 0};
        settings.shot.ammo = AMMO_BULLET;
        settings.shot.surface = SURFTYPE_WOOD;
        settings.flags = WFX_FLASH | WFX_MARK | WFX_TRACER;
        CHECK(emitter->Setup(settings, error));
        CHECK(sprites.flashes.size() == 1 && sprites.smoke.empty());
        CHECK(ballistics.casings.empty() && ballistics.tracers.size() == 1 && ballistics.sprites.size() == 1);
        CHECK(ballistics.sprites.front().resource == "Sprites/blthole5.spr");
        CHECK(ballistics.sprites.front().scale.x == .05f && ballistics.tracers.front().forward.y == -1);

        settings.flags = WFX_MUZZLESMOKE | WFX_SMOKE;
        CHECK(emitter->Setup(settings, error));
        CHECK(sprites.flashes.size() == 1 && sprites.smoke.size() == 2 && ballistics.sprites.size() == 1);
        CHECK(ballistics.casings.empty() && ballistics.tracers.size() == 1);
        CHECK(sprites.reports.back().settings.smoke->color.y == .8f);

        settings.flags = WFX_MUZZLELIGHT | WFX_PARTICLETRAIL | WFX_IMPACTLIGHT;
        CHECK(emitter->Setup(settings, error) && sprites.smoke.size() == 2);
        settings.flags = WFX_EXPLOSION;
        settings.explosion = 255;
        CHECK(!emitter->Setup(settings, error) && error == "Explosion recipe unavailable");
        settings.flags = 0x80000000;
        CHECK(!emitter->Setup(settings, error) && !error.empty());
        CHECK(game.objects.advance(.02) && !game.server.IsObjectAlive(handle));
        CHECK(ballistics.sprites.size() == 1 && ballistics.tracers.size() == 1);
        ballistics.advance(.11, game.server.effect_services());
        CHECK(ballistics.tracers.empty() && ballistics.sprites.size() == 1);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    return argc != 2 ? 2 : selective_effects(argv[1]) ? 0 : 1;
}
