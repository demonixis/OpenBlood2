#pragma once
#include <string>
#include <string_view>

namespace OpenBlood2
{
    struct LauncherInfo
    {
        std::string version, revision;
        unsigned preferences = 0;
    };
    std::string launcher_info();
    bool parse_launcher_info(std::string_view text, LauncherInfo& output, std::string& error);
} // namespace OpenBlood2
