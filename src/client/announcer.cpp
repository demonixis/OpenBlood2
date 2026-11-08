// Selected original ClientShellDLL/TheVoice.cpp startup/combat tables and selection.
// Copyright (c) 1998 Monolith Productions Inc.
#include "client/announcer.h"
#include "client/announcer_data.h"
#include <algorithm>
#include <array>

namespace OpenBlood2
{
    namespace
    {
        std::uint8_t voice_flags(DBYTE attacker, DBYTE victim)
        {
            // Original SharedDefs: Ophelia=1, Gabriella=3; Caleb=0, Ishmael=2.
            return ((attacker == 1 || attacker == 3) ? 2 : 1) | ((victim == 1 || victim == 3) ? 8 : 4);
        }
        const AnnouncerData::Entry* select(std::span<const AnnouncerData::Entry> entries, std::uint16_t index,
                                           std::uint8_t flags, bool extra)
        {
            const auto* entry = &entries[index % entries.size()];
            int safety = -1;
            const auto matches = [&]
            {
                // Source GetVoiceEntry deliberately uses ANY shared bit. The
                // humiliation variant alone requires both attacker and victim.
                return extra ? (flags & (entry->flags & 12)) && (flags & (entry->flags & 3))
                             : bool(entry->flags & flags);
            };
            while (!matches())
            {
                ++index; // Original D_WORD wrap is significant, including at 65535.
                entry = &entries[index % entries.size()];
                if (safety++ > int(entries.size()))
                    return nullptr;
            }
            return entry;
        }
        bool format_names(std::string_view text, std::span<const std::string_view> names, std::string& out)
        {
            std::string next;
            unsigned arg = 0;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const auto c = text[i];
                if (!c)
                    return false;
                if (c != '%')
                    next += c;
                else if (++i < text.size() && text[i] == '%')
                    next += '%';
                else if (i < text.size() && text[i] == 's' && arg < names.size())
                    next += names[arg++];
                else
                    return false;
                if (next.size() >= 500)
                    return false;
            }
            out = std::move(next);
            return true;
        }

        std::size_t name_count(std::string_view text)
        {
            std::size_t count = 0;
            for (auto at = text.find('%'); at != text.npos && at + 1 < text.size(); at = text.find('%', at + 2))
                if (text[at + 1] == 's')
                    ++count;
            return count;
        }
    } // namespace

    bool ClientAnnouncer::translated(unsigned id, std::span<const std::string_view> names, std::string& out) const
    {
        if (!lookup_ || names.size() > 2)
            return false;
        const auto pattern = lookup_(id);
        if (pattern.empty() || pattern.size() >= 500 || pattern.find('\0') != pattern.npos)
            return false;
        std::array<bool, 2> used{};
        std::string next;
        for (std::size_t at = 0; at < pattern.size();)
        {
            const auto c = pattern[at++];
            if (c == '{')
            {
                if (at + 1 >= pattern.size() || pattern[at] < '0' || pattern[at] > '1' || pattern[at + 1] != '}')
                    return false;
                const auto index = std::size_t(pattern[at] - '0');
                if (index >= names.size())
                    return false;
                used[index] = true;
                next += names[index]; // Names are literal, even when they contain braces or percent signs.
                at += 2;
            }
            else if (c == '}')
                return false;
            else
                next += c;
            if (next.size() >= 500)
                return false;
        }
        for (std::size_t i = 0; i < names.size(); ++i)
            if (!used[i])
                return false;
        out = std::move(next);
        return true;
    }
    void ClientAnnouncer::load(const Resources& resources)
    {
        if (loaded_)
            return;
        loaded_ = true;
        std::vector<std::uint8_t> bytes;
        if (const auto* entry = resources.find("cres.dll"); entry && entry->size > 32 * 1024 * 1024)
            error = "cres.dll exceeds the 32 MiB string reader limit";
        else if (resources.read("cres.dll", bytes, error))
            read_string_resources(bytes, strings_, error);
    }
    std::string ClientAnnouncer::level_change_notice(const Resources& resources)
    {
        std::string text;
        if (translated(7011, {}, text))
            return text;
        load(resources);
        const auto found = strings_.strings.find(7011); // IDS_MP_CHANGINGLEVELS.
        if (found != strings_.strings.end() && !found->second.empty() && found->second.size() < 500 &&
            found->second.find('\0') == std::string::npos)
            return found->second;
        ++fallback_texts;
        if (error.empty())
            error = "Missing or invalid level change string 7011";
        return "End of level signaled.  Switching to next level...";
    }
    std::string ClientAnnouncer::team_name(DBYTE team, const Resources& resources)
    {
        if (team < 1 || team > 2)
            return {};
        std::string text;
        if (translated(7006 + team, {}, text) && text.size() <= 100)
            return text;
        return original_team_name(team, resources);
    }
    std::string ClientAnnouncer::original_team_name(DBYTE team, const Resources& resources)
    {
        if (!team)
            return {};
        load(resources);
        const auto found = strings_.strings.find(7006 + team); // IDS_TEAM1NAME/IDS_TEAM2NAME.
        if (found != strings_.strings.end() && !found->second.empty() && found->second.size() <= 100 &&
            found->second.find('\0') == std::string::npos)
            return found->second;
        ++fallback_texts;
        if (error.empty())
            error = "Missing or invalid team name string " + std::to_string(7006 + team);
        return "Team Blue"; // Original second-team fallback typo, only when its resource is absent.
    }
    bool ClientAnnouncer::flag_notice(CTFEventKind kind, std::string_view capturer, DBYTE team, bool local,
                                      const Resources& resources, std::string& out)
    {
        unsigned resource = 0;
        switch (kind)
        {
            case CTFEventKind::Grabbed:
                resource = 7000;
                break;
            case CTFEventKind::Returned:
                resource = 7001;
                break;
            case CTFEventKind::NeedFlag:
                resource = 7009;
                break;
            case CTFEventKind::Captured:
                if (capturer.empty() || capturer.size() > 100 || capturer.find('\0') != capturer.npos || team > 2)
                    return false;
                resource = local ? 7005 : team ? 7003 : 7004;
                break;
            default:
                return false; // Original dropped/team messages print no text.
        }
        const bool observer = kind == CTFEventKind::Captured && !local;
        if (lookup_ && observer)
        {
            const auto name = team_name(team, resources);
            const std::array<std::string_view, 2> names{capturer, name};
            if (translated(resource, std::span(names).first(team ? 2 : 1), out))
                return true;
        }
        else if (!observer && translated(resource, {}, out))
            return true;
        load(resources);
        const auto found = strings_.strings.find(resource);
        std::string pattern;
        // ClientShell first uses FormatString (%% -> %), then sprintf for observer names.
        // Insert names only in the second pass; player-controlled % characters stay literal.
        if (found == strings_.strings.end() || found->second.empty() || found->second.size() >= 500 ||
            !format_names(found->second, {}, pattern))
        {
            error = "Missing or invalid flag notice string " + std::to_string(resource);
            return false;
        }
        if (observer)
        {
            const auto team_text = original_team_name(team, resources);
            const std::array<std::string_view, 2> names{capturer, team_text};
            if (!format_names(pattern, std::span(names).first(team ? 2 : 1), out))
            {
                error = "Invalid flag capture notice format";
                return false;
            }
        }
        else
            out = std::move(pattern);
        return true;
    }
    bool ClientAnnouncer::player_notice(std::string_view name, bool joined, const Resources& resources,
                                        std::string& out, DBYTE team)
    {
        if (team > 2 || name.empty() || name.size() > 100 || name.find('\0') != name.npos)
            return false;
        const unsigned resource =
            joined ? (team ? 7006 : 25) : 26; // Original ClientRes.rc IDS_ADDPLAYER/IDS_REMOVEPLAYER.
        if (lookup_)
        {
            const auto name_team = joined && team ? team_name(team, resources) : std::string{};
            const std::array<std::string_view, 2> names{name, name_team};
            if (translated(resource, std::span(names).first(joined && team ? 2 : 1), out))
                return true;
        }
        load(resources);
        const auto team_text = joined && team ? original_team_name(team, resources) : std::string{};
        const auto format = [&](std::string_view pattern)
        {
            if (pattern.empty() || pattern.size() >= 500)
                return false;
            std::string next;
            for (std::size_t i = 0; i < pattern.size(); ++i)
            {
                if (!pattern[i])
                    return false;
                if (pattern.substr(i).starts_with("%1!s!"))
                {
                    next += name;
                    i += 4;
                }
                else if (!team_text.empty() && pattern.substr(i).starts_with("%2!s!"))
                {
                    next += team_text;
                    i += 4;
                }
                else if (pattern.substr(i).starts_with("%%"))
                {
                    next += '%';
                    ++i;
                }
                else if (pattern[i] == '%')
                    return false;
                else
                    next += pattern[i];
                if (next.size() >= 500)
                    return false;
            }
            out = std::move(next);
            return true;
        };
        const auto found = strings_.strings.find(resource);
        if (found != strings_.strings.end() && format(found->second))
            return true;
        ++fallback_texts;
        if (error.empty())
            error = "Missing or invalid player notice string " + std::to_string(resource);
        return format(joined ? (team ? "%1!s! has joined the game on %2!s!." : "%1!s! has joined the game.")
                             : "%1!s! has left the game.");
    }
    bool ClientAnnouncer::message(std::uint32_t resource, std::string_view fallback, std::string_view file,
                                  std::span<const std::string_view> names, const Resources& resources,
                                  Conversation& out, std::string* caption)
    {
        load(resources);
        Conversation next;
        next.character = 4;
        next.file = "Sounds\\Voice\\" + std::string(file);
        const auto found = strings_.strings.find(resource);
        if (found == strings_.strings.end() || found->second.empty() || found->second.size() >= 128 ||
            !format_names(found->second, names, next.text))
        {
            ++fallback_texts;
            if (error.empty())
                error = "Missing or invalid announcer string " + std::to_string(resource);
            if (!format_names(fallback, names, next.text))
                return false;
        }
        if (caption)
        {
            caption->clear();
            translated(resource, names.first(std::min(names.size(), name_count(fallback))), *caption);
        }
        ++messages;
        out = std::move(next);
        return true;
    }
    bool ClientAnnouncer::frag(std::uint16_t index, const AnnouncerPlayer& killer, const AnnouncerPlayer& victim,
                               bool suicide, const Resources& resources, Conversation& out, std::string* caption)
    {
        if (killer.character >= 4 || victim.character >= 4 || killer.name.empty() || victim.name.empty() ||
            killer.name.size() > 100 || victim.name.size() > 100 || killer.name.find('\0') != killer.name.npos ||
            victim.name.find('\0') != victim.name.npos)
        {
            error = "Invalid announcer player";
            return false;
        }
        const auto* entry = select(suicide ? std::span<const AnnouncerData::Entry>(AnnouncerData::suicide)
                                           : std::span<const AnnouncerData::Entry>(AnnouncerData::kill),
                                   index, voice_flags(killer.character, victim.character), false);
        const std::array names{killer.name, victim.name};
        return entry && message(entry->resource, entry->text, entry->file, std::span(names).first(suicide ? 1 : 2),
                                resources, out, caption);
    }
    bool ClientAnnouncer::finish(const FinishHimVoice& event, const Resources& resources, Conversation& out,
                                 std::string* caption)
    {
        if (event.attacker_character >= 4 || event.victim_character >= 4)
        {
            error = "Invalid humiliation announcer character";
            return false;
        }
        const auto* entry = select(AnnouncerData::humiliation, event.index,
                                   voice_flags(event.attacker_character, event.victim_character), true);
        return entry && message(entry->resource, entry->text, entry->file, {}, resources, out, caption);
    }
    Conversation ClientAnnouncer::bloodbath(const BloodbathStart& event, const Resources& resources,
                                            std::string* caption)
    {
        // TheVoice.cpp:134-141. Every entry has VOICEFLAG_ALL, so the source's
        // male/male startup flags select the first index without a search.
        static constexpr std::array<const char*, 8> fallback{
            {"Let The Bloodbath Begin!", "May The Bloodbath Continue!", "Determine The Master Of The Bloodbath",
             "The Carnage Continues!", "Let The Bloodbath Start!", "May The Killing Continue", "Begin The Bloodbath!",
             "Bloodbath, My Children!"}};
        load(resources);
        ++messages;
        const auto index = event.index % fallback.size();
        Conversation result;
        result.character = 4;
        result.file = "Sounds\\Voice\\Voice" + std::to_string(248 + index) + ".wav";
        const auto found = strings_.strings.find(4001 + static_cast<std::uint32_t>(index));
        if (found != strings_.strings.end() && !found->second.empty() && found->second.size() < 500)
            result.text = found->second;
        else
        {
            result.text = fallback[index]; // Original Ive fallback, not invented dialogue.
            ++fallback_texts;
            if (error.empty())
                error = "Missing or invalid announcer string " + std::to_string(4001 + index);
        }
        if (caption)
        {
            caption->clear();
            translated(4001 + unsigned(index), {}, *caption);
        }
        return result;
    }
} // namespace OpenBlood2
