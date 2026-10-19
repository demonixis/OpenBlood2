#include "startup/protocol.h"
#include "build_info.h"
#include <map>

namespace OpenBlood2
{
    std::string launcher_info()
    {
        return "protocol=1\ngame=OpenBlood2\nversion=" + std::string(build::version) + "\nrevision=" + build::revision +
               "\npreferences=2\nprofiles=rez1\neditions=retail,demo\n";
    }
    bool parse_launcher_info(std::string_view text, LauncherInfo& output, std::string& error)
    {
        error.clear();
        if (text.empty() || text.size() > 4096 || text.back() != '\n')
        {
            error = "Invalid launcher protocol length or termination";
            return false;
        }
        std::map<std::string, std::string> fields;
        while (!text.empty())
        {
            const auto end = text.find('\n');
            auto line = text.substr(0, end);
            text.remove_prefix(end + 1);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            const auto equals = line.find('=');
            if (line.size() > 256 || equals == line.npos || equals == 0 || equals + 1 == line.size())
            {
                error = "Malformed launcher protocol field";
                return false;
            }
            for (const unsigned char c : line)
                if (c < 32 || c > 126)
                {
                    error = "Invalid launcher protocol characters";
                    return false;
                }
            if (!fields.emplace(std::string(line.substr(0, equals)), std::string(line.substr(equals + 1))).second)
            {
                error = "Duplicate launcher protocol field";
                return false;
            }
        }
        if (fields.size() != 7 || fields["protocol"] != "1" || fields["game"] != "OpenBlood2" ||
            (fields["profiles"] != "0" && fields["profiles"] != "rez1") || fields["editions"] != "retail,demo" ||
            (fields["preferences"] != "1" && fields["preferences"] != "2") || !fields.contains("version") ||
            fields["version"].empty() || !fields.contains("revision") || fields["revision"].empty() ||
            fields.size() != 7)
        {
            error = "Unsupported or incomplete game launcher protocol";
            return false;
        }
        LauncherInfo next;
        next.overlays = fields["profiles"] == "rez1";
        next.version = fields["version"];
        next.revision = fields["revision"];
        next.preferences = fields["preferences"] == "2" ? 2 : 1;
        output = std::move(next);
        return true;
    }
} // namespace OpenBlood2
