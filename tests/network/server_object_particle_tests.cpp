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
        for (bool publish : {false, true})
        {
            Config config;
            config.data_dir = path_from_utf8(data);
            MultiplayerRules rules;
            rules.end = MatchEnd::Time;
            rules.minutes = 0;
            ServerSession session;
            CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error, rules));
            auto& game = const_cast<GameSession&>(session.game());
            Joined first, late;
            CHECK(session.join({}, first, error));
            const auto handle = game.objects.get(first.object)->handle;
            const auto logical = game.server.audio.voices().size();
            std::vector<ObjectParticleRecipe> expected;
            WorldSnapshot frame;
            const auto recipes = [&]() -> std::vector<ObjectParticleRecipe>
            {
                for (const auto& object : frame.objects)
                    if (object.id == first.object)
                        return object.object_particles;
                return {};
            };
            if (publish)
            {
                CHECK(!game.server.PublishObjectParticles(handle, 34, error)); // Fragment-only recipe.
                for (unsigned type :
                     {1, 2, 3, 4, 6, 8, 9, 12, 13, 14, 15, 16, 17, 18, 19, 21, 24, 27, 31, 32, 33, 36, 37, 38, 39})
                    CHECK(game.server.PublishObjectParticles(handle, type, error));
                // Repeated publication to one owner is distinct, not replacement.
                CHECK(game.server.PublishObjectParticles(handle, 12, error));
                CHECK(game.server.PublishObjectParticles(handle, 19, error));
                CHECK(session.snapshot(first.player, frame, error));
                expected = recipes();
                CHECK(expected.size() == 27);
                for (std::size_t i = 1; i < expected.size(); ++i)
                    CHECK(expected[i].id > expected[i - 1].id);
                std::vector<std::uint8_t> bytes;
                WorldSnapshot decoded;
                CHECK(encode_world_snapshot(frame, bytes) && decode_world_snapshot(bytes, decoded));
                CHECK(game.client_effects.object_particles.active.empty() &&
                      !game.client_effects.object_particles.started &&
                      game.client_effects.object_particles.reports.empty());
                CHECK(game.server.audio.voices().size() == logical);
            }
            for (unsigned i = 0; i < reference.size(); ++i)
            {
                float value = 0;
                CHECK(game.server.RandomFloat(0.f, 100.f, value));
                if (!publish)
                    reference[i] = value;
                else
                    CHECK(reference[i] == value);
            }
            CHECK(session.join({}, late, error));
            CHECK(session.snapshot(late.player, frame, error));
            CHECK(same_object_particles(expected, recipes()));
            for (unsigned i = 0; i < 362; ++i)
                CHECK(session.tick(error));
            CHECK(session.snapshot(late.player, frame, error));
            std::erase_if(expected, [](const auto& p) { return object_particle_duration(p.type) > 0; });
            CHECK(same_object_particles(expected, recipes()));
            if (publish)
            {
                // Capacity is global, including many publications on one owner.
                for (std::size_t i = expected.size(); i < 128; ++i)
                    CHECK(game.server.PublishObjectParticles(handle, 24, error));
                CHECK(!game.server.PublishObjectParticles(handle, 24, error));
                const auto other = game.objects.get(late.object)->handle;
                CHECK(!game.server.PublishObjectParticles(other, 24, error));
                CHECK(session.snapshot(late.player, frame, error) && recipes().size() == 128);
                CHECK(session.leave(first.player));
                CHECK(!game.server.PublishObjectParticles(handle, 24, error));
                CHECK(game.server.PublishObjectParticles(other, 24, error));
                CHECK(session.snapshot(late.player, frame, error) && recipes().empty());
                CHECK(game.client_effects.object_particles.active.empty() &&
                      !game.client_effects.object_particles.started);
            }
            CHECK(session.clear_players());
            for (unsigned i = 0; i < 100; ++i)
                CHECK(session.tick(error));
            CHECK(session.rotation_due() && session.rotate(error));
            CHECK(session.join({}, late, error) && late.world > first.world);
            CHECK(session.snapshot(late.player, frame, error));
            for (const auto& object : frame.objects)
                CHECK(object.object_particles.empty());
            CHECK(session.clear_players());
        }
        std::cout << "Server ObjectFX: 25 recipes/repeated spells, late baseline, finite expiry, global 128 bound, "
                     "owner departure/rotation, no server cosmetic particles/logical audio or RNG change passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
