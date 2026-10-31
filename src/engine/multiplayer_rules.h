#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace OpenBlood2
{
    // Shared/NetDefs.h NGE_* and the original host/dedicated settings.
    enum class MatchEnd : std::uint8_t
    {
        Frags = 1,
        Time,
        Either,
        Never
    };
    struct MultiplayerRules
    {
        MatchEnd end = MatchEnd::Frags;
        std::uint32_t frags = 25, minutes = 10;
        // Bounded like every other option: about a week of play.
        static constexpr std::uint32_t frag_limit = 1000000, minute_limit = 10000;
        std::vector<std::string> following_maps; // First map is --world / --host; original cap: 50 total.
        bool friendly_fire = true, negative_team_frags = true;
        bool only_flag_scores = false, use_team_size = false;
        std::uint32_t flag_value = 5;
        bool valid() const
        {
            return end >= MatchEnd::Frags && end <= MatchEnd::Never && frags <= frag_limit && minutes <= minute_limit &&
                   flag_value <= frag_limit && following_maps.size() < 50;
        }
    };
    // Selected BloodServerShell::UpdateMultiplayer: TIME first, FRAGSANDTIME
    // means either threshold, and zero remains an immediate threshold.
    inline bool match_limit_reached(const MultiplayerRules& rules, std::uint64_t tick, std::span<const int> scores)
    {
        if ((rules.end == MatchEnd::Time || rules.end == MatchEnd::Either) &&
            tick >= std::uint64_t(rules.minutes) * 60 * 60)
            return true;
        return (rules.end == MatchEnd::Frags || rules.end == MatchEnd::Either) &&
               std::any_of(scores.begin(), scores.end(), [&](int score) { return score >= int(rules.frags); });
    }
    // Both frontends use the same spelling, ranges and settings object.
    inline bool parse_match_option(std::string_view option, std::string_view value, MultiplayerRules& rules)
    {
        if (option == "--friendly-fire" || option == "--negative-team-frags" || option == "--only-flag-scores" ||
            option == "--use-team-size")
        {
            if (value != "on" && value != "off")
                return false;
            if (option == "--friendly-fire")
                rules.friendly_fire = value == "on";
            else if (option == "--negative-team-frags")
                rules.negative_team_frags = value == "on";
            else if (option == "--only-flag-scores")
                rules.only_flag_scores = value == "on";
            else
                rules.use_team_size = value == "on";
            return true;
        }
        if (option == "--end")
        {
            if (value == "frags")
                rules.end = MatchEnd::Frags;
            else if (value == "time")
                rules.end = MatchEnd::Time;
            else if (value == "either")
                rules.end = MatchEnd::Either;
            else if (value == "never")
                rules.end = MatchEnd::Never;
            else
                return false;
            return true;
        }
        if (option == "--map")
        {
            if (rules.following_maps.size() >= 49 || value.empty() || value.size() > 100)
                return false;
            rules.following_maps.emplace_back(value);
            return true;
        }
        std::uint32_t number = 0;
        const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
        if (ec != std::errc{} || end != value.data() + value.size())
            return false;
        if (option == "--frag-limit" && number <= MultiplayerRules::frag_limit)
            rules.frags = number;
        else if (option == "--flag-value" && number <= MultiplayerRules::frag_limit)
            rules.flag_value = number;
        else if (option == "--time-limit" && number <= MultiplayerRules::minute_limit)
            rules.minutes = number;
        else
            return false;
        return true;
    }
} // namespace OpenBlood2
