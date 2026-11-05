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
    MultiplayerRules rules; // Original dedicated default: 10 minutes, frags mode.
    std::string world = "multi/bb_bloodarena", address = "0.0.0.0";
    unsigned mode = GAMETYPE_BLOODBATH, port = Net::default_port, capacity = unsigned(Net::max_players), limit = 0;
    bool lan = true;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--help")
        {
            std::cout << "OpenBlood2 dedicated server (M9: Bloodbath, Team and CTF enabled)\n"
                      << "  --data-dir PATH  External retail installation (or BLOOD2_DATA_DIR)\n"
                      << "  --world NAME     Retail multi/bb_* or multi/ctf_* map\n"
                      << "  --mode MODE      bloodbath, team or ctf\n"
                      << "  --end RULE       frags (default), time, either, never\n"
                      << "  --frag-limit N   Default 25; zero is immediate (Bloodbath needs a player)\n"
                      << "  --time-limit N   Minutes (dedicated default 10); zero is immediate\n"
                      << "  --friendly-fire on|off       Team damage (default on)\n"
                      << "  --negative-team-frags on|off Team-kill penalty (default on)\n"
                      << "  --only-flag-scores on|off    CTF enemy kills give no points (default off)\n"
                      << "  --use-team-size on|off       Multiply capture by opposing members (default off)\n"
                      << "  --flag-value N              Capture points, 0..1000000 (default 5)\n"
                      << "  --map NAME       Append to rotation, up to 50 maps including --world\n"
                      << "  --bind ADDRESS   IPv4 bind address (default 0.0.0.0)\n"
                      << "  --port N         UDP port (default 27888; 0 selects an ephemeral port; not 27889)\n"
                      << "  --no-lan         Do not answer LAN discovery on UDP 27889\n"
                      << "  --max-players N  1..16 (default 16)\n"
                      << "  --ticks N        Stop after N simulation ticks (0: until SIGINT/SIGTERM)\n";
            return 0;
        }
        if (arg == "--no-lan")
        {
            lan = false;
            continue;
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
        else if (arg == "--end" || arg == "--frag-limit" || arg == "--time-limit" || arg == "--map" ||
                 arg == "--friendly-fire" || arg == "--negative-team-frags" || arg == "--only-flag-scores" ||
                 arg == "--use-team-size" || arg == "--flag-value")
            valid = parse_match_option(arg, value, rules);
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
    if (!session.start(config, world, mode, error, rules))
    {
        std::cerr << error << '\n';
        return 5;
    }
    ServerNetwork network(session);
    network.lan_discovery = lan;
    if (!network.listen(address, std::uint16_t(port), capacity, error))
    {
        std::cerr << error << '\n';
        return 5;
    }
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    if (!network.discovery_error.empty())
        std::cerr << network.discovery_error << '\n';
    std::cout << "Server simulation: " << session.game().world_path << ", UDP " << network.port() << ", data "
              << digest_text(session.fingerprint())
              << "\nMultiplayer admission and 20 Hz world replication enabled; movement and player commands enabled; "
                 "Bloodbath/Team combat and CTF capture/respawn enabled.\n"
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
              << ", commands_ignored=" << session.commands_ignored << ", rotations=" << session.rotations << '\n';
    return 0;
}
