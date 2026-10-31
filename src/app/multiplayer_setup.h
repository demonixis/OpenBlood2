#pragma once
#include "engine/config.h"
#include "engine/resources.h"
#include <array>
namespace OpenBlood2
{
    // Menu values become the same checked host/connect options used by the CLI.
    struct MultiplayerSetup
    {
        std::array<char, 101> name{"Player"};
        std::array<char, 256> address{"127.0.0.1:27888"};
        int character = 0, team = 0, mode = 0, map = 0, port = 27888;
        int end = 0, frags = 25, minutes = 15, flag_value = 5;
        bool friendly_fire = true, negative_team_frags = true, only_flag_scores = false, use_team_size = false;
        std::vector<std::string> maps, rotation;
        bool load(const Resources&, const Config&, std::string& error);
        unsigned game_type() const;
        std::vector<std::string> mode_maps() const;
        bool request(bool host, Options&, std::string& error) const;
    };
} // namespace OpenBlood2
