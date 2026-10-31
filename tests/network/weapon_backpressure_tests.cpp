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
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
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
        ServerSession server;
        CHECK(server.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        ServerNetwork network(server);
        CHECK(network.listen("127.0.0.1", 0, 1, error));
        NetworkClient client;
        CHECK(client.prepare(config, error));
        auto now = NetworkClient::Clock::now();
        CHECK(client.connect("127.0.0.1", network.port(), {}, now, error));
        const auto step = [&](bool simulate, bool fire, bool move = false)
        {
            now += std::chrono::microseconds(16667);
            CHECK(!simulate || server.tick(error));
            CHECK(network.pump(now, error));
            MovementInput input;
            input.forward = move;
            CHECK(client.update(now, input, true, error, {fire, false, false, true}));
            std::vector<PrivateEvent> events;
            client.take_events(events);
            CHECK(client.phase() != NetworkClient::Phase::Failed && client.phase() != NetworkClient::Phase::Closed);
            const auto ack = client.weapon_ack() ? client.weapon_ack()->sequence : 0;
            CHECK(client.weapon_frames - ack <= weapon_frame_queue_limit);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return true;
        };
        for (unsigned i = 0; i < 600 && !client.ready(); ++i)
            CHECK(step(true, false));
        CHECK(client.ready());
        for (unsigned i = 0; i < 120; ++i)
            CHECK(step(true, false));
        CHECK(!client.weapon_wait_ticks && client.weapon_ack() && !client.weapon_ack()->fired[0]);

        // Stop simulation but continue real ENet pumping. Fill the reliable
        // window while held fire/movement are sampled, then release during the
        // stall. Neither extra weapon time nor skipped input may be banked.
        for (unsigned i = 0; i < 180; ++i)
            CHECK(step(false, true, true));
        CHECK(client.weapon_wait_ticks > 100);
        const auto frames = client.weapon_frames, updates = client.weapons().updates, reports = client.reports;
        const auto time = client.weapons().time();
        const auto waited = client.weapon_wait_ticks;
        for (unsigned i = 0; i < 60; ++i)
            CHECK(step(false, false));
        CHECK(client.weapon_frames == frames && client.weapons().updates == updates && client.weapons().time() == time);
        CHECK(client.weapon_wait_ticks == waited + 60 && client.reports > reports);

        // A still-slow server gradually frees credit. The already admitted
        // firing frames remain valid; new frames use the current released input.
        for (unsigned i = 0; i < 240; ++i)
            CHECK(step(i % 3 == 0, false));
        CHECK(client.weapon_frames > frames && client.weapon_wait_ticks > waited + 60);
        for (unsigned i = 0; i < 180; ++i)
            CHECK(step(true, false));
        CHECK(client.weapon_ack() && client.weapon_ack()->fired[0] > 0 && !client.weapon_ack()->fired[1]);
        const auto fired = client.weapon_ack()->fired;
        const auto resumed = client.weapon_frames, waits = client.weapon_wait_ticks;
        for (unsigned i = 0; i < 180; ++i)
            CHECK(step(true, false));
        CHECK(client.weapon_ack()->fired == fired && client.weapon_frames == resumed + 180);
        CHECK(client.weapon_wait_ticks == waits && !network.rejected && !network.timed_out);
        CHECK(client.disconnect(now));
        for (unsigned i = 0; i < 120 && client.phase() != NetworkClient::Phase::Closed; ++i)
        {
            now += std::chrono::microseconds(16667);
            CHECK(server.tick(error) && network.pump(now, error) && client.update(now, {}, false, error));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(client.phase() == NetworkClient::Phase::Closed && !server.player_count() && network.close());
        std::cout << "Real ENet weapon backpressure: 4s stopped and 4s one-third-speed simulation, 64-frame bound, "
                     "moving reports, frozen weapon timeline, held/released input and clean recovery; waits="
                  << waits << ", admitted shots=" << fired[0] << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
