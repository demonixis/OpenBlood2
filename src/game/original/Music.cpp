// Original Blood II ClientShellDLL/Music.cpp:24-37,111-199.
// Request order preserved; DLL calls replaced with an explicitly read-only plan.
#include "Music.h"
#include "engine/audio.h"
#include "engine/model_commands.h"
#include "engine/resources.h"
#include "game/key_messages.h"
#include "game/music_catalog.h"
#include <array>

bool CMusic::SelectLevel(std::uint8_t from, std::uint8_t to, OpenBlood2::MusicLevelRequest& out)
{
    out = {};
    if (from > 3 || to > 3 || (from == to && to != 0))
        return false;
    // Original PlayMusicLevel/TransitionToLevel, lines 280-394. Rows are the
    // requested level, columns the current level. Harddriving -> silence uses cts.
    constexpr const char* transitions[4][4] = {{"", "ats.sec", "cts.sec", "cts.sec"},
                                               {"sta.sec", "", "cta.sec", "hta.sec"},
                                               {"stc.sec", "atc.sec", "", "htc.sec"},
                                               {"sth.sec", "ath.sec", "cth.sec", ""}};
    constexpr const char* lists[] = {"SilenceList", "AmbientList", "CruisingList", "HarddrivingList"};
    out = {from, to, lists[to], transitions[to][from], from == 0 && to != 0, true};
    return true;
}

bool CMusic::BuildAssetPlan(const std::map<std::string, std::string>& properties, OpenBlood2::MusicAssetPlan& out,
                            std::string& error)
{
    using namespace OpenBlood2;
    out = {};
    error.clear();
    MusicAssetPlan next;
    auto tokens = [&](const char* property, std::vector<std::string>& result)
    {
        const auto found = properties.find(property);
        if (found == properties.end() || found->second.size() > 500 || found->second.find('\0') != std::string::npos)
            return false;
        // Original Parse consumes only the first command; do not invent additional lists.
        std::string_view text = found->second;
        bool quoted = false;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '"')
                quoted = !quoted;
            if (text[i] == ';' && !quoted)
            {
                text = text.substr(0, i);
                break;
            }
        }
        std::vector<std::vector<std::string>> commands;
        if (!model_commands(text, commands))
            return false;
        result = commands.empty() ? std::vector<std::string>{} : std::move(commands.front());
        for (const auto& word : result)
            if (word.size() >= 80)
                return false;
        return true;
    };
    auto add = [&](std::string_view role, std::string_view file, MusicAssetKind kind)
    {
        std::string path;
        if (!resource_path(file, path) || path.find('/') != path.npos)
            return false;
        const auto suffix = kind == MusicAssetKind::Section ? ".sec" : kind == MusicAssetKind::Style ? ".sty" : ".dls";
        if (!path.ends_with(suffix))
            return false;
        next.requests.push_back({std::string(role), std::string(file), kind});
        return true;
    };
    const auto directory = properties.find("MusicDirectory");
    std::vector<std::string> files;
    if (directory == properties.end() || !resource_path(directory->second, next.directory) ||
        !tokens("InstrumentFiles", files) || files.size() != 2 ||
        !add("InstrumentFiles[0]", files[0], MusicAssetKind::Instruments) ||
        !add("InstrumentFiles[1]", files[1], MusicAssetKind::Style))
    {
        error = "Music plan: invalid directory or InstrumentFiles (expected DLS and initial style)";
        return false;
    }
    // The hard-driving ending really names cts.sec in the supplied source.
    constexpr std::array transitions{std::array{"sta.sec", "ats.sec", "cta.sec", "hta.sec"},
                                     std::array{"stc.sec", "cts.sec", "atc.sec", "htc.sec"},
                                     std::array{"sth.sec", "cts.sec", "ath.sec", "cth.sec"}};
    for (const auto& group : transitions)
        for (int i = 3; i >= 0; --i)
            add("Transition", group[i], MusicAssetKind::Section);
    add("SilenceList", "s.sec", MusicAssetKind::Section);
    for (const char* list : {"AmbientList", "CruisingList", "HarddrivingList"})
    {
        if (!tokens(list, files))
        {
            error = std::string("Music plan: missing/malformed ") + list;
            return false;
        }
        for (const auto& file : files)
            if (!add(list, file, MusicAssetKind::Section))
            {
                error = std::string("Music plan: invalid section in ") + list;
                return false;
            }
    }
    out = std::move(next);
    return true;
}

// ObjectDLL/Music.cpp HandleMusicMessage: literal-space tokens, atoi-style
// byte conversion, case-insensitive commands. Reject undefined overflow safely.
bool CMusic::ParseTrigger(std::string_view text, OpenBlood2::MusicCommand& out, std::string& error)
{
    using namespace OpenBlood2;
    out = {};
    error.clear();
    if (text.size() > 500 || text.find('\0') != text.npos)
    {
        error = "Music trigger length/NUL limit";
        return false;
    }
    auto token = [&]()
    {
        const auto begin = text.find_first_not_of(' ');
        if (begin == text.npos)
        {
            text = {};
            return std::string_view{};
        }
        text.remove_prefix(begin);
        auto end = text.find(' ');
        const auto word = text.substr(0, end);
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        return word;
    };
    const auto command = token();
    auto value = token();
    if (same_name(command, "level"))
    {
        // atoi accepts leading C whitespace/sign, a decimal prefix, or no digits (0).
        while (!value.empty() && (value.front() == '\t' || value.front() == '\n' || value.front() == '\r' ||
                                  value.front() == '\v' || value.front() == '\f'))
            value.remove_prefix(1);
        bool negative = !value.empty() && value.front() == '-';
        if (!value.empty() && (value.front() == '-' || value.front() == '+'))
            value.remove_prefix(1);
        std::uint64_t number = 0;
        for (const char c : value)
        {
            if (c < '0' || c > '9')
                break;
            number = number * 10 + unsigned(c - '0');
            if (number > std::uint64_t(INT32_MAX) + negative)
            {
                error = "Music LEVEL exceeds defined original atoi range";
                return false;
            }
        }
        out.level = std::uint8_t(negative ? -std::int64_t(number) : std::int64_t(number));
        return true;
    }
    if (same_name(command, "motif"))
        out.type = MusicCommandType::Motif;
    else if (same_name(command, "break"))
        out.type = MusicCommandType::Break;
    else
    {
        error = "Unknown or absent original music subcommand";
        return false;
    }
    out.name = value;
    if (!valid_music_command(out))
    {
        error = "Music name missing or exceeds safe original client buffer";
        out = {};
        return false;
    }
    return true;
}

namespace OpenBlood2
{
    bool valid_music_command(const MusicCommand& command)
    {
        if (command.type == MusicCommandType::Level)
            return command.name.empty(); // All byte levels are delivered; the client decides validity.
        return command.type <= MusicCommandType::Break && !command.name.empty() && command.name.size() < 50 &&
               command.name.find('\0') == command.name.npos;
    }
} // namespace OpenBlood2

bool CMusic::PlayMusicLevel(std::uint8_t level, OpenBlood2::MusicCatalog& catalog, OpenBlood2::AudioMixer& audio)
{
    error.clear();
    OpenBlood2::MusicLevelRequest request;
    if (!SelectLevel(logical_level, level, request))
    {
        ++refused;
        return false;
    }
    // Original TransitionToLevel assigns before the fallible PlayList call.
    // The same-silence direct PlayList branch leaves the identical value intact.
    logical_level = level;
    ++requests;
    if (!catalog.request_level(request, audio, error))
    {
        ++failures;
        return false;
    }
    ++accepted; // Admission only; receiver errors and audible level are separate.
    return true;
}
void CMusic::HandleMusicMessage(const OpenBlood2::MusicCommand& command, OpenBlood2::MusicCatalog& catalog,
                                OpenBlood2::AudioMixer& audio)
{
    ++received;
    last_command = command;
    error.clear();
    if (!OpenBlood2::valid_music_command(command))
    {
        error = "Invalid typed music command";
        ++failures;
    }
    else if (command.type == OpenBlood2::MusicCommandType::Level)
        PlayMusicLevel(command.level, catalog, audio);
    else
        ++source_noops; // Original PlayMotif/StopMotif/PlayBreak contain no engine call.
}
void CMusic::EnterWorld(OpenBlood2::MusicCatalog& catalog, OpenBlood2::AudioMixer& audio)
{
    *this = {};
    PlayMusicLevel(0, catalog, audio); // Original fresh-world SilenceList request.
}
void CMusic::ExitWorld(OpenBlood2::AudioMixer& audio)
{
    audio.music_loop.stop(audio.instruments);
    *this = {};
}
