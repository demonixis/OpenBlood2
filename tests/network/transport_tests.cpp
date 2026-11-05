#include "net/transport.h"
#include <array>
#include <chrono>
#include <iostream>
#include <thread>

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
    struct Fixture
    {
        Transport server;
        std::array<Transport, max_players> clients;
        std::array<PeerId, max_players> local{}, remote{};
        std::string error;
        bool open(unsigned count)
        {
            CHECK(server.listen("127.0.0.1", 0, count, error));
            CHECK(server.port());
            for (unsigned i = 0; i < count; ++i)
            {
                CHECK(clients[i].open_client(error));
                CHECK((local[i] = clients[i].connect("127.0.0.1", server.port(), error)));
                bool client_ready = false;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while ((!remote[i] || !client_ready) && std::chrono::steady_clock::now() < deadline)
                {
                    Event event;
                    const auto s = server.poll(event, 0, error);
                    CHECK(s != PollResult::Failed);
                    if (s == PollResult::Event && event.kind == EventKind::Connected)
                        remote[i] = event.peer;
                    const auto c = clients[i].poll(event, 1, error);
                    CHECK(c != PollResult::Failed);
                    if (c == PollResult::Event && event.kind == EventKind::Connected)
                        client_ready = true;
                }
                CHECK(remote[i] && client_ready);
            }
            return true;
        }
        bool receive(Transport& sender, Transport& receiver, Event& event)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline)
            {
                Event ignored;
                CHECK(sender.poll(ignored, 0, error) != PollResult::Failed);
                const auto state = receiver.poll(event, 1, error);
                CHECK(state != PollResult::Failed);
                if (state == PollResult::Event)
                    return true;
            }
            error = "Timed out waiting for UDP event";
            return false;
        }
    };
    bool run(std::string_view test)
    {
        Fixture f;
        auto& error = f.error;
        if (test == "bounds")
        {
            CHECK(!f.server.listen("127.0.0.1", 0, 0, error));
            CHECK(!f.server.listen("127.0.0.1", 0, max_players + 1, error));
            CHECK(!f.server.listen(std::string_view("127.0.0.1\0evil", 14), 0, 1, error));
            CHECK(f.open(1));
            const std::array<std::uint8_t, 3> bytes{1, 2, 3};
            CHECK(!f.server.send(0, Channel::Control, bytes, error));
            CHECK(!f.server.send(f.remote[0], Channel::Count, bytes, error));
            CHECK(!f.server.send(f.remote[0], Channel::Control, {}, error));
            std::vector<std::uint8_t> large(packet_limit(Channel::State) + 1, 7);
            CHECK(!f.server.send(f.remote[0], Channel::State, large, error));
            CHECK(!f.server.listen("127.0.0.1", 0, 1, error));
            CHECK(!f.server.connect("127.0.0.1", f.server.port(), error));
            Event event;
            CHECK(f.server.poll(event, 1001, error) == PollResult::Failed);
            return true;
        }
        CHECK(f.open(test == "capacity" ? unsigned(max_players) : 2));
        if (test == "channels" || test == "capacity")
        {
            const unsigned count = test == "capacity" ? unsigned(max_players) : 2;
            for (unsigned i = 0; i < count; ++i)
                for (Channel channel : {Channel::Control, Channel::Baseline, Channel::State})
                {
                    std::vector<std::uint8_t> bytes(packet_limit(channel), std::uint8_t(i + 1));
                    CHECK(f.server.send(f.remote[i], channel, bytes, error));
                    Event event;
                    CHECK(f.receive(f.server, f.clients[i], event));
                    CHECK(event.kind == EventKind::Received && event.channel == channel && event.bytes == bytes);
                    CHECK(event.peer == f.local[i]);
                    const std::array<std::uint8_t, 2> reply{std::uint8_t(i), std::uint8_t(channel)};
                    CHECK(f.clients[i].send(f.local[i], Channel::Control, reply, error));
                    CHECK(f.receive(f.clients[i], f.server, event));
                    CHECK(event.kind == EventKind::Received && event.peer == f.remote[i] &&
                          event.bytes == std::vector<std::uint8_t>(reply.begin(), reply.end()));
                }
            return true;
        }
        if (test == "reconnect")
        {
            const auto old = f.remote[0];
            CHECK(f.clients[0].disconnect(f.local[0], 42));
            Event event;
            CHECK(f.receive(f.clients[0], f.server, event));
            CHECK(event.kind == EventKind::Disconnected && event.peer == old && event.reason == 42);
            const std::array<std::uint8_t, 1> bytes{5};
            CHECK(!f.server.send(old, Channel::Control, bytes, error));
            f.clients[0].close();
            CHECK(f.clients[0].open_client(error));
            const auto fresh = f.clients[0].connect("127.0.0.1", f.server.port(), error);
            CHECK(fresh && fresh != f.local[0]);
            CHECK(f.receive(f.clients[0], f.server, event));
            CHECK(event.kind == EventKind::Connected && event.peer != old);
            CHECK(!f.server.send(old, Channel::Control, bytes, error));
            return true;
        }
        if (test == "queue")
        {
            std::vector<std::uint8_t> bytes(packet_limit(Channel::Baseline), 2);
            for (std::size_t i = 0; i < Transport::queue_limit / bytes.size(); ++i)
                CHECK(f.server.send(f.remote[0], Channel::Baseline, bytes, error));
            CHECK(!f.server.send(f.remote[0], Channel::Baseline, bytes, error));
            PeerStats stats;
            CHECK(f.server.stats(f.remote[0], stats) && stats.queued_bytes == Transport::queue_limit);
            CHECK(f.server.send(f.remote[1], Channel::Baseline, bytes, error));
            // Closing with outstanding fragmented packets must release every budget safely.
            f.server.close();
            CHECK(!f.server.stats(f.remote[0], stats));
            return true;
        }
        return false;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
