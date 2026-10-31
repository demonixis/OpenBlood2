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
    bool run(const char* path)
    {
        std::string error;
        Config config;
        config.data_dir = path;
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined first, second;
        CHECK(session.join({}, first, error) && session.join({}, second, error));
        NetworkPlayerEvents inbox;
        CHECK(inbox.begin(first));
        std::vector<PrivateEvent> events, other;
        CHECK(session.take_private_events(first.player, events, error) &&
              session.take_private_events(second.player, other, error));
        unsigned announcers = 0;
        const auto deliver = [&]()
        {
            for (const auto& event : events)
            {
                std::vector<std::uint8_t> bytes;
                PrivateEvent received;
                CHECK(encode_private(event, bytes) && decode_private(bytes, received) == DecodePrivate::Ok);
                CHECK(inbox.receive(received, error) == NetworkPlayerEvents::Reception::Applied);
                announcers += std::holds_alternative<BloodbathStart>(received.value);
            }
            std::vector<PrivateEvent> received;
            inbox.take(received);
            CHECK(received.size() == events.size());
            return true;
        };
        CHECK(!events.empty() && !other.empty() && deliver() && announcers == 1 && session.subsumed_events);
        for (const auto& event : other)
            CHECK(event.owner == second);
        auto& game = const_cast<GameSession&>(session.game());
        auto* a = dynamic_cast<CPlayerObj*>(game.objects.get(first.object)->game.get());
        const auto other_handle = game.objects.get(second.object)->handle;
        CHECK(a);
        CHECK(game.server.SendBossHealth({.5f}) == DE_OK);
        CHECK(game.server.SendObjectives({4, "title", "text"}) == DE_OK);
        CHECK(game.server.SendConversation({"voice.wav", "words", 4}) == DE_OK);
        CHECK(session.take_private_events(first.player, events, error) && events.size() == 3 && deliver());
        CHECK(session.take_private_events(second.player, other, error) && other.size() == 3);
        for (const auto& event : other)
            CHECK(event.owner == second);
        CHECK(std::holds_alternative<BossHealth>(other[0].value));
        CHECK(std::holds_alternative<MissionObjectives>(other[1].value));
        CHECK(std::holds_alternative<Conversation>(other[2].value));
        for (unsigned i = 0; i < private_queue_limit; ++i)
            CHECK(game.server.SendToClient(a->GetClient(), PlayerText{"full"}) == DE_OK);
        CHECK(game.server.SendBossHealth({.25f}) == DE_ERROR);
        CHECK(session.take_private_events(second.player, other, error) && other.size() == 1);
        CHECK(std::get<BossHealth>(other[0].value).fraction == .25f);
        CHECK(session.take_private_events(first.player, events, error) && events.size() == private_queue_limit &&
              deliver());
        PlayerStats stats;
        stats.health = 37;
        stats.weapon = 1;
        stats.ammo = 4;
        CHECK(game.server.SendToClient(a->GetClient(), stats) == DE_OK);
        InventoryItems items;
        items.current = InventoryItemView{1, "medi.pcx", "medih.pcx", "", 301, 25};
        items.show = true;
        for (int i = 0; i < 3; ++i)
            CHECK(game.server.SendToClient(a->GetClient(), items) == DE_OK);
        CHECK(game.server.SendToClient(a->GetClient(), OrbView{other_handle, false}) == DE_OK);
        CHECK(game.server.SendToClient(a->GetClient(), SeeingEyeView{other_handle, false, 10}) == DE_OK);
        CHECK(game.server.SendToClient(a->GetClient(), CTFEvent{CTFEventKind::Captured, 2, 3, other_handle}) ==
              DE_INVALIDPARAMS);
        CHECK(session.leave(second.player) && !game.objects.get(second.object));
        CHECK(session.take_private_events(first.player, events, error) && events.size() == 7);
        CHECK(std::get<OrbEvent>(events[4].value).orb == second.object);
        CHECK(std::get<EyeEvent>(events[5].value).eye == second.object);
        CHECK(std::get<PlayerRoster>(events[6].value).players.size() == 1);
        CHECK(deliver() && inbox.hud().stats.health == 37 && inbox.hud().item_shows == 3 &&
              inbox.hud().item_messages == 3);
        CHECK(game.server.SendToClient(a->GetClient(), OrbView{other_handle, true}) == DE_INVALIDPARAMS);
        std::vector<PlayerEvent> legacy;
        CHECK(game.server.take_client_events(a->GetClient(), legacy) && legacy.empty());
        for (unsigned i = 0; i < private_queue_limit; ++i)
            CHECK(game.server.SendToClient(a->GetClient(), PlayerText{"bounded"}) == DE_OK);
        CHECK(game.server.SendToClient(a->GetClient(), PlayerText{"overflow"}) == DE_ERROR);
        CHECK(session.take_private_events(first.player, events, error) && events.size() == private_queue_limit &&
              deliver());
        CHECK(game.server.SendToClient(a->GetClient(), PlayerText{"after drain"}) == DE_OK);
        CHECK(session.take_private_events(first.player, events, error) && events.size() == 1 && deliver());
        CHECK(session.clear_players());
        std::cout << "Retail private events: startup, addressed HUD/items, IDs after destruction, bounded queue and "
                     "cleanup passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
