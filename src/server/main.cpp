#include "game/game_settings.h"
#include "net/transport.h"
#include "server/network.h"
#include "server/session.h"
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace
{
    volatile std::sig_atomic_t stopping = 0;
    void stop(int)
    {
        stopping = 1;
    }
    bool number(std::string_view text, unsigned& out)
    {
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
        return ec == std::errc{} && end == text.data() + text.size();
    }
} // namespace
int main(int argc, char** argv)
{
    using namespace OpenBlood2;
    Options options;
    std::string world = "multi/bb_bloodarena", address = "0.0.0.0";
    unsigned mode = GAMETYPE_BLOODBATH, port = Net::default_port, capacity = unsigned(Net::max_players), limit = 0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--help")
        {
            std::cout << "OpenBlood2 dedicated server (M9: Bloodbath movement, combat and respawn enabled)\n"
                      << "  --data-dir PATH  External retail installation (or BLOOD2_DATA_DIR)\n"
                      << "  --world NAME     Retail multi/bb_* or multi/ctf_* map\n"
                      << "  --mode MODE      bloodbath, team, ctf\n"
                      << "  --bind ADDRESS   IPv4 bind address (default 0.0.0.0)\n"
                      << "  --port N         UDP port (default 27888; 0 selects an ephemeral port)\n"
                      << "  --max-players N  1..16 (default 16)\n"
                      << "  --ticks N        Stop after N simulation ticks (0: until SIGINT/SIGTERM)\n";
            return 0;
        }
        if (++i >= argc)
        {
            std::cerr << "Missing value for " << arg << '\n';
            return 2;
        }
        const std::string_view value = argv[i];
        bool valid = true;
        if (arg == "--data-dir")
            options.data_dir = path_from_utf8(value);
        else if (arg == "--world")
            world = value;
        else if (arg == "--bind")
            address = value;
        else if (arg == "--port")
            valid = number(value, port) && port <= UINT16_MAX;
        else if (arg == "--max-players")
            valid = number(value, capacity) && capacity && capacity <= Net::max_players;
        else if (arg == "--ticks")
            valid = number(value, limit);
        else if (arg == "--mode")
        {
            if (value == "bloodbath")
                mode = GAMETYPE_BLOODBATH;
            else if (value == "team")
                mode = GAMETYPE_TEAMPLAY;
            else if (value == "ctf")
                mode = GAMETYPE_CTF;
            else
                valid = false;
        }
        else
            valid = false;
        if (!valid)
        {
            std::cerr << "Invalid server option: " << arg << '\n';
            return 2;
        }
    }
    std::string error;
    Config config;
    const auto* environment = std::getenv("BLOOD2_DATA_DIR");
    if (!load_config(options, environment ? environment : "", config, error))
    {
        std::cerr << error << '\n';
        return 2;
    }
    ServerSession session;
    if (!session.start(config, world, mode, error))
    {
        std::cerr << error << '\n';
        return 5;
    }
    ServerNetwork network(session);
    if (!network.listen(address, std::uint16_t(port), capacity, error))
    {
        std::cerr << error << '\n';
        return 5;
    }
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    std::cout << "Server simulation: " << session.game().world_path << ", UDP " << network.port() << ", data "
              << digest_text(session.fingerprint())
              << "\nBloodbath admission and 20 Hz world replication enabled; movement and player commands enabled; "
                 "Bloodbath combat and respawn enabled.\n"
              << std::flush;
    ServerClock clock;
    auto previous = std::chrono::steady_clock::now();
    std::uint64_t ticks_done = 0;
    while (!stopping && (!limit || ticks_done < limit))
    {
        const auto now = std::chrono::steady_clock::now();
        unsigned ticks = 0;
        if (!clock.advance(std::chrono::duration<double>(now - previous).count(), ticks))
            return 6;
        previous = now;
        for (unsigned i = 0; i < ticks && (!limit || ticks_done < limit); ++i, ++ticks_done)
        {
            if (!session.tick(error))
            {
                std::cerr << error << '\n';
                return 6;
            }
            for (const auto& warning : session.take_warnings())
                std::cerr << warning << '\n';
        }
        if (!network.pump(std::chrono::steady_clock::now(), error))
        {
            std::cerr << error << '\n';
            return 6;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!network.close())
        return 6;
    std::cout << "Server stopped: ticks=" << ticks_done << ", discarded_ticks=" << clock.discarded_ticks()
              << ", admitted=" << network.admitted << ", departed=" << network.departed
              << ", rejected=" << network.rejected << ", timed_out=" << network.timed_out
              << ", membership_updates=" << network.membership_updates << ", state_updates=" << network.state_updates
              << ", state_drops=" << network.state_drops << ", state_bytes=" << network.state_bytes
              << ", commands_applied=" << session.commands_applied << ", commands_blocked=" << session.commands_blocked
              << ", commands_ignored=" << session.commands_ignored << '\n';
    return 0;
}
