#include "client/network_client.h"
#include "game/game_settings.h"
#include "game/original/CharacterProfiles.h"
#include "server/network.h"
#include <iostream>
#include <thread>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x ": " << error << '\n';                                 \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    constexpr std::array<int, 2> empty_scores{};
    bool state()
    {
        std::string error;
        const Joined owner{7, 1, {0, 1}};
        NetworkPlayerEvents client;
        CHECK(client.begin(owner, GAMETYPE_TEAMPLAY));
        PlayerRoster roster{
            {{1, owner.object, "Blue", 0, 2, 1}, {2, {1, 1}, "Red", 1, -1, 2}}, GAMETYPE_TEAMPLAY, {7, -3}};
        PrivateEvent event{owner, 1, 0, roster};
        std::vector<std::uint8_t> bytes;
        CHECK(encode_private(event, bytes));
        PrivateEvent decoded;
        CHECK(decode_private(bytes, decoded) == DecodePrivate::Ok &&
              client.receive(decoded, error) == NetworkPlayerEvents::Reception::Applied);
        CHECK(client.team() == 1 && client.team_frags() == roster.team_frags && client.mode() == GAMETYPE_TEAMPLAY);
        event = {owner, 2, 1, FragEvent{{1, 1}, owner.object, 3, 0, {8, -3}}};
        CHECK(encode_private(event, bytes) && decode_private(bytes, decoded) == DecodePrivate::Ok &&
              client.receive(decoded, error) == NetworkPlayerEvents::Reception::Applied);
        CHECK(client.team_frags()[0] == 8 && client.player(owner.object)->frags == 3);
        CHECK(client.receive(decoded, error) == NetworkPlayerEvents::Reception::Ignored && client.team_frags()[0] == 8);
        const auto saved = client.roster();
        roster.players[0].team = 2;
        event = {owner, 3, 1, roster};
        CHECK(client.receive(event, error) == NetworkPlayerEvents::Reception::Refused && client.roster() == saved &&
              client.team_frags()[0] == 8 && client.sequence() == 2);
        roster.players[0].team = 1;
        roster.mode = GAMETYPE_CTF;
        event.value = roster;
        CHECK(client.receive(event, error) == NetworkPlayerEvents::Reception::Refused && client.sequence() == 2);
        roster.mode = GAMETYPE_TEAMPLAY;
        roster.players.pop_back();
        roster.players[0].frags = 3;
        roster.team_frags = {8, -3};
        event.value = roster;
        CHECK(client.receive(event, error) == NetworkPlayerEvents::Reception::Applied && client.team_frags()[1] == -3 &&
              client.roster().size() == 1);
        CHECK(client.begin({8, 3, {0, 2}}, GAMETYPE_TEAMPLAY) && client.team() == 0 &&
              client.team_frags() == empty_scores);
        std::cout << "Team roster/absolute totals, duplicate/immutable-membership/mode refusal, departure and epoch "
                     "reset passed\n";
        return true;
    }
    bool session(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        MultiplayerRules rules;
        rules.frags = 2;
        rules.friendly_fire = false;
        rules.negative_team_frags = false;
        rules.following_maps = {"multi/bb_fields"};
        ServerSession server;
        CHECK(server.start(config, "multi/bb_crypt", GAMETYPE_TEAMPLAY, error, rules));
        JoinProfile blue, red;
        blue.team = 1;
        red.team = 2;
        Joined a, b, c;
        CHECK(server.join(blue, a, error) && server.join(blue, b, error) && server.join(red, c, error));
        CHECK(server.player_team(a.player) == 1 && server.player_team(b.player) == 1 &&
              server.player_team(c.player) == 2 && !server.player_team(999));
        auto& game = const_cast<GameSession&>(server.game());
        CHECK(!game.server.friendly_fire && !game.server.negative_team_frags);
        auto* first = dynamic_cast<CPlayerObj*>(game.objects.get(a.object)->game.get());
        auto* second = dynamic_cast<CPlayerObj*>(game.objects.get(b.object)->game.get());
        auto* enemy = dynamic_cast<CPlayerObj*>(game.objects.get(c.object)->game.get());
        CHECK(first && second && enemy && first->m_nSkin == 5 && enemy->m_nSkin == 6);
        NetworkPlayerEvents client;
        CHECK(client.begin(c, GAMETYPE_TEAMPLAY));
        const auto drain = [&]()
        {
            std::vector<PrivateEvent> events, consumed;
            CHECK(server.take_private_events(c.player, events, error));
            for (const auto& event : events)
                CHECK(client.receive(event, error) == NetworkPlayerEvents::Reception::Applied);
            client.take(consumed);
            return true;
        };
        CHECK(drain() && client.team() == 2);
        enemy->m_damage.last_damage.attacker = first->m_hObject;
        enemy->UpdateFragStatus();
        CHECK(drain() && client.team_frags()[0] == 1 && server.tick(error) && !server.changing_level());
        CHECK(server.leave(a.player) && drain() && client.team_frags()[0] == 1 && !server.player_team(a.player));
        enemy->m_damage.last_damage.attacker = second->m_hObject;
        enemy->UpdateFragStatus();
        CHECK(drain() && client.team_frags()[0] == 2 && second->m_Frags == 1 && server.tick(error) &&
              server.changing_level());
        CHECK(server.clear_players());
        while (!server.rotation_due())
            CHECK(server.tick(error));
        CHECK(server.rotate(error) && server.game().world_path == "worlds/multi/bb_fields.dat" &&
              game.server.team_frags == empty_scores);
        CHECK(!game.server.friendly_fire && !game.server.negative_team_frags);
        CHECK(server.join(red, c, error) && server.player_team(c.player) == 2 && server.clear_players());
        std::cout << "Team admission/forced skins, private absolute totals, departed scores reach team limit with no "
                     "individual winner, fresh rotation/rules passed\n";
        return true;
    }
    bool admission_edges(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        MultiplayerRules rules;
        rules.frags = 0;
        ServerSession server;
        CHECK(server.start(config, "multi/bb_crypt", GAMETYPE_TEAMPLAY, error, rules));
        CHECK(!server.player_count() && server.tick(error) && server.changing_level());
        while (!server.rotation_due())
            CHECK(server.tick(error));
        CHECK(server.rotate(error) && !server.player_count());
        ServerNetwork network(server);
        CHECK(network.listen("127.0.0.1", 0, 1, error));
        Transport client;
        CHECK(client.open_client(error));
        const auto peer = client.connect("127.0.0.1", network.port(), error);
        CHECK(peer);
        const auto send = [&](const AdmissionMessage& message)
        {
            std::vector<std::uint8_t> bytes;
            CHECK(encode_admission(message, bytes) && client.send(peer, Channel::Control, bytes, error));
            client.flush();
            return true;
        };
        unsigned offers = 0;
        bool joined = false, refused = false;
        auto now = ServerNetwork::Clock::now();
        for (unsigned tick = 0; tick < 1000 && !refused; ++tick)
        {
            now += std::chrono::microseconds(16667);
            CHECK(server.tick(error) && network.pump(now, error));
            for (unsigned work = 0; work < 64; ++work)
            {
                Event event;
                const auto result = client.poll(event, 0, error);
                CHECK(result != PollResult::Failed);
                if (result == PollResult::Idle)
                    break;
                if (event.kind == EventKind::Connected)
                    CHECK(send(Hello{Edition::Retail, server.fingerprint()}));
                else if (event.kind == EventKind::Disconnected)
                {
                    CHECK(event.reason == std::uint32_t(Reject::Player));
                    refused = true;
                }
                else if (event.channel == Channel::Control)
                {
                    AdmissionMessage message;
                    CHECK(decode_admission(event.bytes, message) == DecodeAdmission::Ok);
                    if (const auto* offer = std::get_if<WorldOffer>(&message))
                    {
                        Ready ready{offer->world, {}};
                        if (++offers == 2)
                        {
                            CHECK(joined);
                            ready.profile.team = 1;
                        }
                        CHECK(send(ready));
                    }
                    else if (const auto* entry = std::get_if<Joined>(&message))
                    {
                        CHECK(!joined && offers == 1 && server.player_team(entry->player));
                        joined = true; // Intentionally remain in Baseline; rotation must still retain the profile.
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(refused && offers == 2 && joined && network.admitted == 1 && network.rejected == 1 &&
              !server.player_count());
        CHECK(network.close());
        std::cout << "Empty Team zero-frag rotation and changed Ready team rejected across a retained Baseline-phase "
                     "peer passed\n";
        return true;
    }
    bool endpoints(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        MultiplayerRules rules;
        rules.end = MatchEnd::Time;
        rules.minutes = 0;
        rules.following_maps = {"multi/bb_fields"};
        ServerSession server;
        CHECK(server.start(config, "multi/bb_crypt", GAMETYPE_TEAMPLAY, error, rules));
        ServerNetwork network(server);
        CHECK(network.listen("127.0.0.1", 0, 3, error));
        NetworkClient clients[2];
        for (unsigned i = 0; i < 2; ++i)
        {
            JoinProfile profile;
            profile.name = i ? "Second" : "First";
            profile.character = i; // auto teams
            CHECK(clients[i].prepare(config, error) &&
                  clients[i].connect("127.0.0.1", network.port(), profile, NetworkClient::Clock::now(), error));
        }
        std::array<std::uint64_t, 2> worlds{}, players{};
        std::array<unsigned, 2> entries{};
        std::array<DBYTE, 2> teams{};
        auto now = NetworkClient::Clock::now();
        for (unsigned tick = 0; tick < 1800; ++tick)
        {
            now += std::chrono::microseconds(16667);
            CHECK(server.tick(error) && network.pump(now, error));
            for (unsigned i = 0; i < 2; ++i)
            {
                auto& c = clients[i];
                CHECK(c.update(now, {}, true, error) && c.phase() != NetworkClient::Phase::Closed);
                if (c.ready() && c.players().team() && c.joined().world != worlds[i])
                {
                    CHECK(c.offer().mode == GAMETYPE_TEAMPLAY && c.joined().world > worlds[i] &&
                          c.joined().player > players[i]);
                    CHECK(c.players().team_frags() == empty_scores && c.players().mode() == GAMETYPE_TEAMPLAY);
                    CHECK(!teams[i] || c.players().team() == teams[i]);
                    teams[i] = c.players().team();
                    worlds[i] = c.joined().world;
                    players[i] = c.joined().player;
                    ++entries[i];
                    const auto* player = c.world().get(c.joined().object);
                    std::string actual, expected;
                    CHECK(player && resource_path(player->skin, actual) &&
                          resource_path(character_skin(i, teams[i] + 4), expected) && actual == expected);
                }
                std::vector<PrivateEvent> consumed;
                c.take_events(consumed);
            }
            if (entries[0] >= 3 && entries[1] >= 3)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(entries[0] >= 3 && entries[1] >= 3 && teams[0] != teams[1] && network.admitted >= 6 &&
              server.rotations >= 2);
        CHECK(network.close());
        std::cout << "Two independent auto-team clients retain assigned teams across Crypt -> Fields -> Crypt with "
                     "fresh player/replica epochs\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "state")
        return state() ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "session")
        return session(argv[2]) ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "admission_edges")
        return admission_edges(argv[2]) ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "endpoints")
        return endpoints(argv[2]) ? 0 : 1;
    return 2;
}
