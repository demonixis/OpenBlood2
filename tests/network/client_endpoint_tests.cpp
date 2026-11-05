#include "client/network_client.h"
#include <iostream>
#include <thread>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
using Clock = NetworkClient::Clock;
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
        {
            // A prepared endpoint can wait arbitrarily without consuming its
            // connection budget. No sleeping, busy work or longer timeout.
            Transport server;
            CHECK(server.listen("127.0.0.1", 0, 1, error));
            NetworkClient client;
            const auto early = Clock::time_point{};
            CHECK(!client.connect("127.0.0.1", server.port(), {}, early, error));
            CHECK(client.phase() == NetworkClient::Phase::Idle && client.prepare(config, error));
            CHECK(!client.prepare(config, error) && client.phase() == NetworkClient::Phase::Prepared);
            const auto connected = early + std::chrono::hours(1);
            CHECK(!client.update(connected, {}, false, error) && client.phase() == NetworkClient::Phase::Prepared);
            CHECK(!client.disconnect(connected));
            auto invalid = JoinProfile{};
            invalid.name.clear();
            CHECK(!client.connect("127.0.0.1", server.port(), invalid, connected, error));
            CHECK(client.phase() == NetworkClient::Phase::Prepared);
            CHECK(client.connect("127.0.0.1", server.port(), {}, connected, error));
            CHECK(client.update(connected + std::chrono::milliseconds(4999), {}, false, error));
            CHECK(client.phase() == NetworkClient::Phase::Connecting);
            // The server deliberately never polls/acknowledges this connection.
            CHECK(!client.update(connected + std::chrono::seconds(5), {}, false, error));
            CHECK(client.phase() == NetworkClient::Phase::Failed && client.reason() == "Server connection timed out");
        }
        for (unsigned test = 0; test < 5; ++test)
        {
            Transport server;
            CHECK(server.listen("127.0.0.1", 0, 1, error));
            NetworkClient client;
            CHECK(client.prepare(config, error));
            const auto began = Clock::now();
            CHECK(client.connect("127.0.0.1", server.port(), {}, began, error));
            PeerId peer = 0;
            bool hello = false;
            while (!hello && Clock::now() < began + std::chrono::seconds(5))
            {
                CHECK(client.update(Clock::now(), {}, false, error));
                Event event;
                const auto status = server.poll(event, 1, error);
                CHECK(status != PollResult::Failed);
                if (status != PollResult::Event)
                    continue;
                if (event.kind == EventKind::Connected)
                    peer = event.peer;
                if (event.kind == EventKind::Received)
                {
                    AdmissionMessage value;
                    CHECK(decode_admission(event.bytes, value) == DecodeAdmission::Ok &&
                          std::holds_alternative<Hello>(value));
                    hello = true;
                }
            }
            CHECK(peer && hello && client.phase() == NetworkClient::Phase::Offer);
            if (test == 0)
            {
                const auto before = client.phase();
                CHECK(!client.update(began - std::chrono::seconds(1), {}, false, error) && client.phase() == before);
                CHECK(!client.update(Clock::now() + std::chrono::seconds(31), {}, false, error));
                CHECK(client.phase() == NetworkClient::Phase::Failed &&
                      client.reason() == "Server connection timed out");
                continue;
            }
            if (test == 1)
            {
                const std::uint8_t malformed[]{1, 2, 3};
                CHECK(server.send(peer, Channel::Control, malformed, error));
            }
            else if (test == 2)
            {
                std::vector<std::uint8_t> bytes;
                CHECK(encode_admission(Heartbeat{1}, bytes) && server.send(peer, Channel::State, bytes, error));
            }
            else if (test == 3)
            {
                std::vector<std::uint8_t> bytes;
                CHECK(encode_command(CommandAck{{1, 1, {0, 1}, 1}, 1, true}, bytes) &&
                      server.send(peer, Channel::Control, bytes, error));
            }
            else
                CHECK(server.disconnect(peer, std::uint32_t(Reject::Content)));
            server.flush();
            bool progressed = false;
            const auto until = Clock::now() + std::chrono::seconds(3);
            while (Clock::now() < until)
            {
                if (!client.update(Clock::now(), {}, false, error))
                {
                    CHECK(test < 4 && client.phase() == NetworkClient::Phase::Failed);
                    progressed = true;
                    break;
                }
                if (client.phase() == NetworkClient::Phase::Closed)
                {
                    CHECK(test == 4 && client.reason() == reject_text(std::uint32_t(Reject::Content)));
                    progressed = true;
                    break;
                }
                Event ignored;
                CHECK(server.poll(ignored, 1, error) != PollResult::Failed);
            }
            CHECK(progressed);
        }
        std::cout << "NetworkClient: separate preparation, exact five-second connection budget, real ENet handshake, "
                     "bad clock, timeout, malformed/order refusals and rejection "
                     "reason passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
