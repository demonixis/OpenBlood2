// Original Blood II ClientShellDLL/Music.cpp:24-37,111-199.
// Request order preserved; DLL calls replaced with an explicitly read-only plan.
#include "Music.h"
#include "engine/model_commands.h"
#include "engine/resources.h"
#include <array>

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
