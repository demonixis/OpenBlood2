#include "game/game_settings.h"
#include "server/network.h"
#include <iostream>

using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << "\n";                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    struct Fixture
    {
        ServerSession session;
        ServerNetwork server{session};
        std::array<Transport, 16> clients;
        std::array<PeerId, 16> peers{};
        std::array<Joined, 16> joined{};
        ServerNetwork::Clock::time_point now = ServerNetwork::Clock::now();
        std::string error;
        bool start(const char* path)
        {
            Config config;
            config.data_dir = path;
            CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
            CHECK(server.listen("127.0.0.1", 0, 16, error));
            return true;
        }
        bool wait(unsigned index, Event& event, EventKind kind)
        {
            const auto deadline = ServerNetwork::Clock::now() + std::chrono::seconds(5);
            while (ServerNetwork::Clock::now() < deadline)
            {
                CHECK(server.pump(now, error));
                const auto result = clients[index].poll(event, 1, error);
                CHECK(result != PollResult::Failed);
                if (result == PollResult::Event && event.kind == EventKind::Disconnected && kind != event.kind)
                {
                    error = "Peer " + std::to_string(index) + " disconnected: " + reject_text(event.reason) + " / " +
                            server.last_rejection;
                    return false;
                }
                if (result == PollResult::Event && event.kind == kind)
                    return true;
            }
            error = "UDP event deadline";
            return false;
        }
        bool connect(unsigned index)
        {
            const auto expected = server.peer_count() + 1;
            clients[index].close();
            CHECK(clients[index].open_client(error));
            peers[index] = clients[index].connect("127.0.0.1", server.port(), error);
            CHECK(peers[index]);
            Event event;
            CHECK(wait(index, event, EventKind::Connected));
            // The client's Connected event precedes flushing its final ENet ACK.
            // Wait for the authoritative Connected event before advancing fake time.
            clients[index].flush();
            const auto deadline = ServerNetwork::Clock::now() + std::chrono::seconds(5);
            while (server.peer_count() < expected && ServerNetwork::Clock::now() < deadline)
            {
                CHECK(server.pump(now, error));
                CHECK(clients[index].poll(event, 1, error) != PollResult::Failed);
            }
            CHECK(server.peer_count() == expected);
            return true;
        }
        bool send(unsigned index, AdmissionMessage message)
        {
            std::vector<std::uint8_t> bytes;
            CHECK(encode_admission(message, bytes));
            CHECK(clients[index].send(peers[index], Channel::Control, bytes, error));
            clients[index].flush();
            return true;
        }
        bool offer(unsigned index)
        {
            CHECK(send(index, Hello{Edition::Retail, session.fingerprint()}));
            Event event;
            CHECK(wait(index, event, EventKind::Received));
            AdmissionMessage message;
            CHECK(decode_admission(event.bytes, message) == DecodeAdmission::Ok);
            const auto* offer = std::get_if<WorldOffer>(&message);
            CHECK(offer && offer->world == session.world_id() && offer->path == session.game().world_path);
            return true;
        }
        bool join(unsigned index)
        {
            JoinProfile profile;
            profile.name = "Player " + std::to_string(index);
            CHECK(send(index, Ready{session.world_id(), profile}));
            Event event;
            CHECK(wait(index, event, EventKind::Received));
            AdmissionMessage message;
            CHECK(decode_admission(event.bytes, message) == DecodeAdmission::Ok);
            CHECK(std::holds_alternative<Joined>(message));
            joined[index] = std::get<Joined>(message);
            CHECK(joined[index].world == session.world_id() && session.game().objects.get(joined[index].object));
            return true;
        }
        bool rejected(unsigned index, Reject reason)
        {
            Event event;
            CHECK(wait(index, event, EventKind::Disconnected));
            CHECK(event.reason == std::uint32_t(reason));
            return true;
        }
    };
    bool run(const char* path)
    {
        Fixture f;
        auto& error = f.error;
        CHECK(f.start(path));
        const auto initial = f.session.game().objects.snapshot().size();
        // No original player or owned child before content verification AND Ready.
        CHECK(f.connect(0));
        CHECK(f.send(0, Hello{Edition::Demo, f.session.fingerprint()}));
        CHECK(f.rejected(0, Reject::Edition));
        CHECK(f.connect(0));
        auto wrong = f.session.fingerprint();
        wrong[0] ^= 1;
        CHECK(f.send(0, Hello{Edition::Retail, wrong}));
        CHECK(f.rejected(0, Reject::Content));
        CHECK(f.connect(0));
        CHECK(f.send(0, Ready{f.session.world_id(), {}}));
        CHECK(f.rejected(0, Reject::Order));
        CHECK(f.connect(0));
        std::vector<std::uint8_t> bytes;
        CHECK(encode_admission(Hello{Edition::Retail, f.session.fingerprint()}, bytes));
        bytes[4] ^= 1;
        CHECK(f.clients[0].send(f.peers[0], Channel::Control, bytes, error));
        f.clients[0].flush();
        CHECK(f.rejected(0, Reject::Protocol));
        CHECK(f.connect(0));
        CHECK(f.offer(0));
        CHECK(!f.session.player_count() && f.session.game().server.client_count() == 0);
        CHECK(f.session.game().objects.snapshot().size() == initial);
        CHECK(f.send(0, Ready{f.session.world_id() + 1, {}}));
        CHECK(f.rejected(0, Reject::World));
        CHECK(f.connect(0));
        CHECK(f.offer(0));
        CHECK(f.send(0, Hello{Edition::Retail, f.session.fingerprint()}));
        CHECK(f.rejected(0, Reject::Order));
        CHECK(!f.session.player_count() && f.session.game().objects.snapshot().size() == initial);

        // Monotonic timeout policies are independent of the simulation/ENet wall clocks.
        CHECK(f.connect(0));
        f.now += std::chrono::seconds(5);
        CHECK(f.rejected(0, Reject::Timeout));
        CHECK(f.connect(0));
        CHECK(f.offer(0));
        f.now += std::chrono::seconds(30);
        CHECK(f.rejected(0, Reject::Timeout));
        CHECK(f.connect(0));
        CHECK(f.offer(0));
        CHECK(f.join(0));
        const auto expired = f.joined[0].object;
        CHECK(!f.server.pump(f.now - std::chrono::seconds(1), error));
        CHECK(f.session.player_count() == 1 && f.session.game().objects.get(expired));
        f.now += std::chrono::seconds(15);
        CHECK(f.rejected(0, Reject::Timeout));
        CHECK(!f.session.player_count() && !f.session.game().objects.get(expired));

        for (unsigned i = 0; i < 16; ++i)
        {
            CHECK(f.connect(i));
            CHECK(f.offer(i));
            CHECK(f.join(i));
        }
        CHECK(f.session.player_count() == 16 && f.session.game().server.client_count() == 16);
        Joined excess;
        CHECK(!f.session.join({}, excess, error));
        for (unsigned tick = 0; tick < 180; ++tick)
        {
            CHECK(f.session.tick(error));
            f.now += std::chrono::milliseconds(16);
            CHECK(f.server.pump(f.now, error));
            for (unsigned i = 0; i < 16; ++i)
            {
                Event event;
                auto status = f.clients[i].poll(event, 0, error);
                CHECK(status != PollResult::Failed);
                if (status == PollResult::Event && event.kind == EventKind::Received)
                {
                    AdmissionMessage message;
                    CHECK(decode_admission(event.bytes, message) == DecodeAdmission::Ok);
                    CHECK(std::holds_alternative<Heartbeat>(message));
                    CHECK(f.send(i, Heartbeat{f.session.world_id()}));
                }
                CHECK(f.session.game().objects.get(f.joined[i].object));
            }
        }
        // A duplicate Ready cannot allocate a second character; reconnect gets new identities.
        const auto old = f.joined[0];
        CHECK(f.send(0, Ready{f.session.world_id(), {}}));
        CHECK(f.rejected(0, Reject::Order));
        CHECK(f.session.player_count() == 15 && !f.session.game().objects.get(old.object));
        CHECK(f.connect(0));
        CHECK(f.offer(0));
        CHECK(f.join(0));
        CHECK(f.joined[0].player != old.player && f.joined[0].object != old.object);
        CHECK(f.server.close());
        CHECK(f.session.player_count() == 0 && f.session.game().server.client_count() == 0);
        for (const auto& player : f.joined)
            CHECK(!f.session.game().objects.get(player.object));
        CHECK(!f.session.game().server.client_audio && f.session.game().client_session.audio.mixer.voices().empty());
        std::cout << "Verified admission gates, 3 monotonic timeout phases, 16 ENet peers / 180 ticks, "
                     "duplicate/reconnect and clean shutdown\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
