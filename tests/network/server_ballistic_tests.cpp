#include "effect_fixture.h"
#include "game/game_settings.h"
#include "server/session.h"
#include <iostream>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        std::string error;
        std::array<float, 8> reference{};
        for (unsigned mode = 0; mode < 3; ++mode)
        {
            Config config;
            config.data_dir = path_from_utf8(data);
            ServerSession session;
            CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
            Joined a, b;
            JoinProfile profile;
            profile.name = "A";
            CHECK(session.join(profile, a, error));
            profile.name = "B";
            CHECK(session.join(profile, b, error));
            auto& game = const_cast<GameSession&>(session.game());
            auto* source = game.objects.get(a.object);
            const auto logical = game.server.audio.voices().size();
            ShotgunEffectSettings shot;
            shot.source = source->state.m_Pos;
            shot.destination = shot.source;
            shot.destination.y -= 10;
            shot.forward = {0, 0, 1};
            shot.normal = {0, 1, 0};
            shot.eject = true;
            shot.ammo = AMMO_BULLET;
            std::vector<EffectEvent> events;
            CHECK(!session.take_effect_events(a.player, events, error));
            if (mode)
            {
                CHECK(game.server.PublishShotgunEffects(source->handle, shot, error));
                CHECK(session.begin_effects(a.player) && !session.begin_effects(a.player));
                CHECK(session.take_effect_events(a.player, events, error) && events.empty());
                CHECK(game.server.PublishShotgunEffects(source->handle, shot, error));
                if (mode == 2)
                    CHECK(session.begin_effects(b.player));
                CHECK(session.take_effect_events(a.player, events, error) && events.size() == 1);
                CHECK(events[0].world == a.world && events[0].player == a.player && events[0].sequence == 1 &&
                      events[0].source == a.object);
                if (mode == 2)
                    CHECK(session.take_effect_events(b.player, events, error) && events.empty());
                for (unsigned i = 0; i <= effect_queue_limit; ++i)
                {
                    CHECK(game.server.PublishShotgunEffects(source->handle, shot, error));
                    CHECK(session.take_effect_events(a.player, events, error) && events.size() == 1);
                    CHECK(events[0].sequence == i + 2);
                }
                if (mode == 2)
                    CHECK(!session.take_effect_events(b.player, events, error));
                auto flash = std::get<WeaponSpriteSettings>(effect_fixture::sample(1).recipe);
                const auto spark = std::get<SparkSettings>(effect_fixture::sample(2).recipe);
                const auto blood = std::get<BloodSpurtRecipe>(effect_fixture::sample(3).recipe);
                const auto splat = std::get<BloodSplatSettings>(effect_fixture::sample(4).recipe);
                CHECK(game.server.PublishWeaponSprites(source->handle, flash, error));
                CHECK(game.server.PublishSparks(source->handle, spark, error));
                CHECK(game.server.PublishClientBloodSpurt(blood.settings, "weapon entry", true, error));
                CHECK(game.server.PublishBloodSplat(source->handle, splat, error));
                CHECK(game.server.PublishBloodSpurt(source->handle, blood.settings, error));
                CHECK(game.server.PublishClientBloodSplat(splat, "client splat", error));
                CHECK(session.take_effect_events(a.player, events, error) && events.size() == 6);
                const std::size_t kinds[]{1, 2, 3, 4, 3, 4};
                for (unsigned i = 0; i < 6; ++i)
                {
                    CHECK(events[i].recipe.index() == kinds[i]);
                    CHECK(events[i].source == ((i == 2 || i == 5) ? ObjectId{} : a.object));
                    if (i)
                        CHECK(events[i].sequence == events[i - 1].sequence + 1);
                }
                CHECK(std::get<BloodSpurtRecipe>(events[2].recipe).immediate &&
                      !std::get<BloodSpurtRecipe>(events[4].recipe).immediate);
                CHECK(!game.client_effects.weapon_sprites.started && !game.client_effects.sparks.started &&
                      !game.client_effects.blood_spurts.started && !game.client_effects.blood_splats.started);
                CHECK(game.server.PublishExplosionModels(source->handle, 3, shot.source, shot.normal, error));
                CHECK(game.server.PublishExplosionSprites(source->handle, 3, shot.source, shot.normal, error));
                CHECK(game.server.PublishExplosionRings(source->handle, 3, shot.source, shot.normal, error));
                CHECK(game.server.PublishExplosionFragments(source->handle, 3, shot.source, shot.normal, error));
                CHECK(game.server.PublishExplosionFlames(source->handle, 8, shot.source, shot.normal, error));
                CHECK(game.server.PublishExplosionLights(source->handle, 3, shot.source, shot.normal, error));
                CHECK(game.server.PublishFleshGibs(source->handle, shot.source, {0, 100, 0}, {5, 5, 5}, 1, 4, error));
                DebrisEffect debris;
                debris.source = "destroyed brush";
                debris.position = shot.source;
                debris.velocity = {0, 100, 0};
                debris.dimensions = {5, 5, 5};
                debris.flags = 3;
                debris.amount = 4;
                game.server.RecordDebrisEffect(debris);
                CHECK(game.server.debris_effects.back().event && game.server.debris_effects.back().error.empty());
                CHECK(game.server.PublishLaserBeam(source->handle, shot.source, shot.destination, error, 5));
                CHECK(game.server.PublishLightning(source->handle,
                                                   LightningSettings{shot.source, shot.destination, 2, 1, 2}, error));
                CHECK(session.take_effect_events(a.player, events, error) && events.size() == 10);
                for (unsigned i = 0; i < 10; ++i)
                {
                    CHECK(events[i].recipe.index() == (i < 6 ? 5 : i));
                    CHECK(events[i].source == (i == 7 ? ObjectId{} : a.object));
                    if (i)
                        CHECK(events[i].sequence == events[i - 1].sequence + 1);
                    if (i < 6)
                        CHECK(unsigned(std::get<ExplosionRecipe>(events[i].recipe).component) == i);
                }
                CHECK(game.client_effects.explosion_models.reports.empty() &&
                      game.client_effects.explosion_sprites.reports.empty() &&
                      game.client_effects.explosion_rings.reports.empty() &&
                      game.client_effects.explosion_fragments.reports.empty() &&
                      game.client_effects.explosion_flames.reports.empty() &&
                      game.client_effects.explosion_lights.reports.empty() &&
                      game.client_effects.flesh_gibs.reports.empty() &&
                      game.client_effects.destruction_debris.reports.empty() &&
                      game.client_effects.laser_beams.reports.empty() && game.client_effects.lightning.reports.empty());
                // Count and encoded-byte limits are independent. Large bounded
                // resource names need no server-side cosmetic asset binding.
                flash.flash->resource = std::string(1000, 'f');
                flash.smoke->resource = std::string(1000, 's');
                std::vector<std::uint8_t> bytes;
                EffectEvent large{a.world, a.player, 1, a.object, game.server.GetObjectName(source->handle), flash};
                CHECK(encode_effect(large, bytes));
                const auto count = effect_queue_bytes / bytes.size();
                CHECK(count < effect_queue_limit);
                for (std::size_t i = 0; i <= count; ++i)
                    CHECK(game.server.PublishWeaponSprites(source->handle, flash, error));
                CHECK(!session.take_effect_events(a.player, events, error));
                CHECK(session.player_count() == 2 && game.client_effects.shotgun_effects.started == 0 &&
                      game.client_effects.shotgun_effects.casings.empty() &&
                      game.client_effects.shotgun_effects.reports.empty());
                ParticleStreamSettings stream;
                stream.on = true;
                stream.count = 128;
                CHECK(game.server.PublishParticleStream(source->handle, stream, error));
                CHECK(game.client_effects.particle_streams.active.empty() &&
                      !game.client_effects.particle_streams.started);
                CHECK(game.server.audio.voices().size() == logical);
            }
            for (unsigned i = 0; i < reference.size(); ++i)
            {
                float value = 0;
                CHECK(game.server.RandomFloat(0.f, 100.f, value));
                if (!mode)
                    reference[i] = value;
                else
                    CHECK(value == reference[i]);
            }
            CHECK(session.clear_players());
        }
        std::cout
            << "Authoritative impacts: mixed families/owner forms, ordered stream, baseline-only recipients, no "
               "historical replay, private sequences, "
               "2048-entry/2MiB isolated overflow, no server cosmetic pools/logical sounds and unchanged game RNG "
               "with zero/one/two consumers passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
