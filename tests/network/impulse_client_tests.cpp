#include "client/network_client.h"
#include "game/game_settings.h"
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
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        Config config;
        config.data_dir = path_from_utf8(data);
        std::string error;
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        ServerNetwork server(session);
        CHECK(server.listen("127.0.0.1", 0, 2, error));
        NetworkClient client;
        CHECK(client.prepare(config, error));
        auto now = NetworkClient::Clock::now();
        CHECK(client.connect("127.0.0.1", server.port(), {}, now, error));
        const auto step = [&]() -> bool
        {
            CHECK(session.tick(error));
            now += std::chrono::microseconds(16667);
            CHECK(server.pump(now, error) && client.update(now, {}, true, error));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return true;
        };
        for (unsigned i = 0; i < 600 && !client.ready(); ++i)
            CHECK(step());
        CHECK(client.ready());
        for (unsigned i = 0; i < 30; ++i)
            CHECK(step());
        auto* record = session.game().objects.get(client.joined().object);
        auto* player = dynamic_cast<CPlayerObj*>(record->game.get());
        CHECK(player);
        const auto from = client.movement().movement().body.position;
        const auto updates = client.movement().manager().updates;
        const auto corrections = client.movement().corrections;
        player->m_damage.ApplyDamagePhysics(3, {1, 0, 0});
        player->m_damage.ApplyDamagePhysics(20, {0, 1, 0});
        player->door_push_pending = true;
        player->door_push_velocity = {0, 40, 0};
        unsigned steps = 0;
        for (; steps < 30 && client.movement().impulse_sequence() != 1; ++steps)
            CHECK(step());
        CHECK(client.movement().impulse_sequence() == 1);
        CHECK(client.movement().manager().updates == updates + steps);
        auto peak = client.movement().movement().body.position.y;
        for (unsigned i = 0; i < 12; ++i)
        {
            CHECK(step());
            peak = std::max(peak, client.movement().movement().body.position.y);
            CHECK(client.movement().impulse_sequence() == 1);
        }
        // bb_crypt's authored gravity is -2000: this small push has already
        // fallen near its starting height after twelve frames. Check the arc.
        CHECK(peak > from.y + 5 && client.movement().manager().updates == updates + steps + 12);
        std::cout << "Received impulse arc: rise=" << peak - from.y << " units, " << steps + 12 << " client updates\n";
        // State ACK delivery is asynchronous/best-effort. Allow subsequent
        // publications instead of requiring a correction in the short jump arc.
        for (unsigned i = 0; i < 180 && client.movement().corrections == corrections; ++i)
            CHECK(step());
        std::cout << "Correction progress " << corrections << " -> " << client.movement().corrections
                  << ", State packets=" << client.state_packets << '\n';
        CHECK(client.movement().corrections > corrections);
        std::optional<MovementAck> ack;
        CHECK(session.movement_ack(client.joined().player, ack) && ack && ack->impulse_sequence == 1);
        std::vector<MovementImpulse> pending;
        CHECK(session.movement_impulses(client.joined().player, 0, pending, error) && pending.empty());
        const auto force = client.movement().setup().owner.force_revision;
        // Pending damage from the previous life cannot survive the reliable force.
        player->m_damage.ApplyDamagePhysics(3, {0, 0, 1});
        CHECK(player->TeleportClientToServerPos());
        for (unsigned i = 0; i < 30 && client.movement().setup().owner.force_revision == force; ++i)
            CHECK(step());
        CHECK(client.movement().setup().owner.force_revision > force && client.movement().impulse_sequence() == 0);
        player->m_damage.ApplyDamagePhysics(1, {1, 0, 0});
        for (unsigned i = 0; i < 30 && !client.movement().impulse_sequence(); ++i)
            CHECK(step());
        CHECK(client.movement().impulse_sequence() == 1);
        CHECK(client.disconnect(now));
        for (unsigned i = 0; i < 60 && session.player_count(); ++i)
            CHECK(step());
        CHECK(!session.player_count() && server.close());
        std::cout << "NetworkClient over real ENet: accumulated impulses, once-only CMoveMgr, ACK retirement, "
                     "fresh force and clean departure passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
