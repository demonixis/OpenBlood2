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
            std::vector<BallisticEvent> events;
            CHECK(!session.take_ballistic_events(a.player, events, error));
            if (mode)
            {
                CHECK(game.server.PublishShotgunEffects(source->handle, shot, error));
                CHECK(session.begin_ballistics(a.player) && !session.begin_ballistics(a.player));
                CHECK(session.take_ballistic_events(a.player, events, error) && events.empty());
                CHECK(game.server.PublishShotgunEffects(source->handle, shot, error));
                if (mode == 2)
                    CHECK(session.begin_ballistics(b.player));
                CHECK(session.take_ballistic_events(a.player, events, error) && events.size() == 1);
                CHECK(events[0].world == a.world && events[0].player == a.player && events[0].sequence == 1 &&
                      events[0].source == a.object);
                if (mode == 2)
                    CHECK(session.take_ballistic_events(b.player, events, error) && events.empty());
                for (unsigned i = 0; i <= ballistic_queue_limit; ++i)
                {
                    CHECK(game.server.PublishShotgunEffects(source->handle, shot, error));
                    CHECK(session.take_ballistic_events(a.player, events, error) && events.size() == 1);
                    CHECK(events[0].sequence == i + 2);
                }
                if (mode == 2)
                    CHECK(!session.take_ballistic_events(b.player, events, error));
                CHECK(session.player_count() == 2 && game.client_effects.shotgun_effects.started == 0 &&
                      game.client_effects.shotgun_effects.casings.empty() &&
                      game.client_effects.shotgun_effects.reports.empty());
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
        std::cout << "Authoritative ballistics: baseline-only recipients, no historical replay, private sequences, "
                     "256-entry isolated overflow, no server cosmetic pools/logical sounds and unchanged game RNG "
                     "with zero/one/two consumers passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
