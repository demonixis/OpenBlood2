#include "app/multiplayer_setup.h"
#include "game/game_settings.h"
#include "net/address.h"
#include "net/admission.h"
#include "net/discovery.h"
#include <algorithm>
namespace OpenBlood2
{
    bool MultiplayerSetup::load(const Resources& resources, const Config& config, std::string& error)
    {
        error.clear();
        maps.clear();
        rotation.clear();
        map = 0;
        if (config.demo || config.nightmares || !config.overlays.empty())
        {
            error = "Multiplayer requires The Chosen retail data without mods.";
            return false;
        }
        for (const auto& [path, entry] : resources.index())
            if ((path.starts_with("worlds/multi/bb_") || path.starts_with("worlds/multi/ctf_")) &&
                path.ends_with(".dat"))
                maps.push_back(path);
        std::sort(maps.begin(), maps.end());
        if (maps.empty())
        {
            error = "No multiplayer maps found in the selected game data.";
            return false;
        }
        return true;
    }
    unsigned MultiplayerSetup::game_type() const
    {
        return mode == 0 ? GAMETYPE_BLOODBATH : mode == 1 ? GAMETYPE_TEAMPLAY : GAMETYPE_CTF;
    }
    std::vector<std::string> MultiplayerSetup::mode_maps() const
    {
        std::vector<std::string> out;
        for (const auto& p : maps)
            if (p.starts_with(game_type() == GAMETYPE_CTF ? "worlds/multi/ctf_" : "worlds/multi/bb_"))
                out.push_back(p);
        return out;
    }
    bool MultiplayerSetup::request(bool host, Options& out, std::string& error, std::string_view* error_key) const
    {
        error.clear();
        if (error_key)
            *error_key = {};
        const auto fail = [&](std::string_view key, std::string message)
        {
            if (error_key)
                *error_key = key;
            error = std::move(message);
            return false;
        };
        if (name.back() || address.back() || character < 0 || character > 3 || team < 0 || team > 2)
            return fail("network.invalid_profile", "Choose a valid player name, character and team.");
        Net::JoinProfile profile;
        profile.name = name.data();
        profile.character = std::uint8_t(character);
        profile.team = std::uint8_t(team);
        if (!Net::valid_profile(profile))
            return fail("network.invalid_name", "Enter a player name (1 to 100 characters, no control characters).");
        Options next;
        next.network_name = profile.name;
        next.network_character = profile.character;
        next.network_team = profile.team;
        if (host)
        {
            const auto choices = mode_maps();
            if (mode < 0 || mode > 2 || map < 0 || std::size_t(map) >= choices.size() || port < 1 || port > 65535 ||
                port == Net::discovery_port || end < 0 || end > 3 || frags < 0 ||
                frags > int(MultiplayerRules::frag_limit) || minutes < 0 ||
                minutes > int(MultiplayerRules::minute_limit) ||
                flag_value < 0 || flag_value > 1000000 || rotation.size() > 49 ||
                std::any_of(rotation.begin(), rotation.end(), [&](const auto& p)
                            { return std::find(choices.begin(), choices.end(), p) == choices.end(); }))
                return fail("network.invalid_host", "Choose a valid map, port (not " + std::to_string(Net::discovery_port) +
                                                        ", reserved for LAN discovery), rotation and match limits.");
            next.host_world = choices[map];
            next.network_port = std::uint16_t(port);
            next.network_mode = std::uint8_t(game_type());
            next.network_rules = {MatchEnd(end + 1), std::uint32_t(frags), std::uint32_t(minutes),
                                  rotation,          friendly_fire,        negative_team_frags,
                                  only_flag_scores,  use_team_size,        std::uint32_t(flag_value)};
        }
        else
        {
            Net::ServerAddress parsed;
            if (!Net::parse_address(address.data(), Net::default_port, parsed, error))
                return fail("network.invalid_address", std::move(error));
            next.connect_address = parsed.host + ":" + std::to_string(parsed.port);
        }
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2
