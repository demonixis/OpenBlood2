#include "game/game_settings.h"
#include "game/original/ClientParticleStreamSFX.h"
#include "game/original/ParticleSystem.h"
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
    bool rotation(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        MultiplayerRules rules;
        rules.end = MatchEnd::Time;
        rules.minutes = 0;
        rules.following_maps = {"multi/bb_crypt"};
        ServerSession session;
        CHECK(session.start(config, "multi/bb_bridge", GAMETYPE_BLOODBATH, error, rules));
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            Joined player;
            CHECK(session.join({}, player, error) && player.world > previous);
            previous = player.world;
            WorldSnapshot frame;
            CHECK(session.snapshot(player.player, frame, error));
            const auto count = std::count_if(frame.objects.begin(), frame.objects.end(),
                                             [](const auto& object) { return object.particle_stream.has_value(); });
            CHECK(count == (cycle == 1 ? 0 : 2));
            CHECK(session.game().client_effects.particle_streams.active.empty());
            CHECK(session.leave(player.player));
            if (cycle != 2)
            {
                for (unsigned tick = 0; tick < 452; ++tick)
                    CHECK(session.tick(error));
                CHECK(session.rotation_due() && session.rotate(error));
            }
        }
        std::cout << "Stream rotation: Bridge/Crypt/Bridge, fresh epochs, 2/0/2 baseline emitters and no server "
                     "particles passed\n";
        return true;
    }
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        Resources resources;
        CHECK(resources.mount_game(config, error));
        unsigned maps = 0, emitters = 0, systems = 0;
        for (const auto& [path, resource] : resources.index())
        {
            const bool ctf = path.starts_with("worlds/multi/ctf_");
            if ((!ctf && !path.starts_with("worlds/multi/bb_")) || !path.ends_with(".dat"))
                continue;
            ServerSession session;
            CHECK(session.start(config, path, ctf ? GAMETYPE_CTF : GAMETYPE_BLOODBATH, error));
            auto& game = const_cast<GameSession&>(session.game());
            CHECK(game.client_effects.particle_streams.active.empty() && !game.client_effects.particle_streams.started);
            for (unsigned i = 0; i < 30; ++i)
                CHECK(session.tick(error));
            JoinProfile profile;
            profile.name = "First";
            Joined a, b;
            CHECK(session.join(profile, a, error));
            WorldSnapshot first;
            CHECK(session.snapshot(a.player, first, error));
            unsigned count = 0, system_count = 0;
            for (const auto& o : first.objects)
            {
                const auto* object = game.objects.get(o.id);
                CHECK(object);
                const auto* stream = dynamic_cast<const CClientParticleStreamSFX*>(object->game.get());
                const auto* system = dynamic_cast<const ParticleSystem*>(object->game.get());
                CHECK(bool(o.particle_stream) == bool(stream || system));
                if (o.particle_stream)
                {
                    CHECK(same_particle_stream(*o.particle_stream, stream ? stream->published : system->settings));
                    ++count;
                    system_count += o.particle_stream->system;
                }
            }
            for (unsigned i = 0; i < 30; ++i)
                CHECK(session.tick(error));
            profile.name = "Late";
            CHECK(session.join(profile, b, error));
            WorldSnapshot late;
            CHECK(session.snapshot(b.player, late, error));
            unsigned late_count = 0;
            for (const auto& o : late.objects)
                late_count += o.particle_stream.has_value();
            CHECK(count == late_count);
            std::vector<std::uint8_t> bytes;
            WorldSnapshot decoded;
            CHECK(encode_world_snapshot(late, bytes) && decode_world_snapshot(bytes, decoded));
            CHECK(game.client_effects.particle_streams.active.empty() && !game.client_effects.particle_streams.started);
            // Live creation is also membership metadata; duplicate publication cannot replace it.
            const auto* player = game.objects.get(a.object);
            CHECK(player);
            ParticleStreamSettings settings;
            settings.on = true;
            settings.count = 2;
            CHECK(game.server.PublishParticleStream(player->handle, settings, error));
            CHECK(!game.server.PublishParticleStream(player->handle, settings, error));
            CHECK(session.snapshot(b.player, late, error));
            auto found =
                std::find_if(late.objects.begin(), late.objects.end(), [&](const auto& o) { return o.id == a.object; });
            CHECK(found != late.objects.end() && found->particle_stream &&
                  same_particle_stream(*found->particle_stream, settings));
            CHECK(session.leave(a.player) && session.snapshot(b.player, late, error));
            CHECK(std::none_of(late.objects.begin(), late.objects.end(),
                               [&](const auto& o) { return o.id == a.object; }));
            const auto* late_player = game.objects.get(b.object);
            CHECK(late_player);
            settings.system = true;
            settings.particles_per_second = 300;
            CHECK(game.server.PublishParticleStream(late_player->handle, settings, error));
            CHECK(session.snapshot(b.player, late, error));
            found =
                std::find_if(late.objects.begin(), late.objects.end(), [&](const auto& o) { return o.id == b.object; });
            CHECK(found != late.objects.end() && found->particle_stream && found->particle_stream->system);
            CHECK(session.leave(b.player));
            emitters += count;
            systems += system_count;
            ++maps;
            std::cout << path << ": emitters=" << count << ", systems=" << system_count
                      << ", late baseline and dynamic owner retirement passed\n"
                      << std::flush;
        }
        CHECK(maps == 22 && emitters > 0);
        std::cout << maps << " maps, " << emitters << " authored emitters (" << systems
                  << " systems), no server cosmetic streams\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 3 && std::string_view(argv[2]) == "rotation")
        return rotation(argv[1]) ? 0 : 1;
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
