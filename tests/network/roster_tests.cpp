#include "client/network_player_events.h"
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
            std::cerr << __LINE__ << ": " #x " : " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined a, b, c, d;
        JoinProfile first, second;
        first.name = "Caleb %s";
        second.name = std::string(100, 'x');
        second.character = 2;
        CHECK(session.join(first, a, error) && session.join(second, b, error));
        NetworkPlayerEvents clients[4];
        CHECK(clients[0].begin(a) && clients[1].begin(b));
        const auto deliver = [&](const Joined& recipient, NetworkPlayerEvents& client)
        {
            std::vector<PrivateEvent> events, consumed;
            CHECK(session.take_private_events(recipient.player, events, error));
            for (const auto& event : events)
            {
                std::vector<std::uint8_t> bytes;
                PrivateEvent decoded;
                CHECK(encode_private(event, bytes) && decode_private(bytes, decoded) == DecodePrivate::Ok);
                CHECK(client.receive(decoded, error) == NetworkPlayerEvents::Reception::Applied);
            }
            client.take(consumed);
            return true;
        };
        CHECK(deliver(a, clients[0]) && deliver(b, clients[1]));
        CHECK(clients[0].roster() == clients[1].roster() && clients[0].roster().size() == 2);
        CHECK(clients[0].player(a.object)->name == first.name && clients[0].player(b.object)->name == second.name &&
              clients[1].player(b.object)->character == 2);
        auto& game = const_cast<GameSession&>(session.game());
        auto* attacker = dynamic_cast<CPlayerObj*>(game.objects.get(a.object)->game.get());
        auto* victim = dynamic_cast<CPlayerObj*>(game.objects.get(b.object)->game.get());
        CHECK(attacker && victim);
        victim->m_damage.last_damage.attacker = attacker->m_hObject;
        victim->UpdateFragStatus();
        CHECK(attacker->m_Frags == 1 && victim->death_error.empty());
        CHECK(session.join({}, c, error) && clients[2].begin(c));
        CHECK(deliver(a, clients[0]) && deliver(b, clients[1]) && deliver(c, clients[2]));
        CHECK(clients[0].roster() == clients[2].roster() && clients[2].player(a.object)->frags == 1);
        attacker->m_damage.last_damage.attacker = attacker->m_hObject;
        attacker->UpdateFragStatus();
        CHECK(attacker->m_Frags == 0 && attacker->death_error.empty());
        CHECK(session.leave(b.player) && !game.objects.get(b.object));
        CHECK(deliver(a, clients[0]) && deliver(c, clients[2]));
        CHECK(!clients[0].player(b.object) && clients[0].player(a.object)->frags == 0);
        CHECK(session.join(second, d, error) && clients[3].begin(d) && d.player > c.player && d.object != b.object);
        CHECK(deliver(a, clients[0]) && deliver(c, clients[2]) && deliver(d, clients[3]));
        CHECK(clients[3].roster() == clients[0].roster() && !clients[0].player(b.object) &&
              clients[0].player(d.object)->name == second.name && clients[0].player(d.object)->frags == 0);
        auto invalid = first;
        invalid.name = "invalid\nname";
        Joined refused;
        CHECK(!session.join(invalid, refused, error) && session.player_count() == 3);
        CHECK(deliver(a, clients[0]) && clients[0].roster().size() == 3);
        // Saturating one reliable inbox cannot keep a departed gameplay body
        // alive or silently omit its removal. The affected recipient fails closed.
        for (unsigned i = 0; i < private_queue_limit; ++i)
            CHECK(game.server.SendToClient(attacker->GetClient(), PlayerText{"full"}) == DE_OK);
        CHECK(session.leave(d.player) && !game.objects.get(d.object));
        std::vector<PrivateEvent> events;
        CHECK(!session.take_private_events(a.player, events, error) &&
              error == "Reliable player roster publication failed");
        CHECK(deliver(c, clients[2]) && clients[2].roster().size() == 2 && !clients[2].player(d.object));
        CHECK(session.leave(a.player) && deliver(c, clients[2]) && clients[2].roster().size() == 1);
        CHECK(session.clear_players() && session.player_count() == 0 && session.movement_steps == 0);
        std::cout << "Retail roster: named identities, original frag/suicide scores, late arrival, ordered departure, "
                     "fresh reconnect, refused admission and saturated recipient cleanup passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
