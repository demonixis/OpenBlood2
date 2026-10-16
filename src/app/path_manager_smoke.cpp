#include "path_manager_smoke.h"
namespace OpenBlood2
{
    bool prepare_path_manager_smoke(GameSession& g, PathList& snapshot)
    {
        if (g.server_shell.paths.size() != 7)
            return false;
        snapshot = g.server_shell.paths.entries();
        return check_path_manager_smoke(g, snapshot, false);
    }
    bool check_path_manager_smoke(GameSession& g, const PathList& snapshot, bool reloaded)
    {
        if (snapshot.size() != 7 || g.server_shell.paths.size() != 7)
            return false;
        for (std::size_t i = 0; i < snapshot.size(); ++i)
        {
            const auto& old = snapshot[i];
            const auto& now = g.server_shell.paths.entries()[i];
            if (now.name != old.name || now.position.x != old.position.x || now.position.y != old.position.y ||
                now.position.z != old.position.z || now.action_target != old.action_target ||
                now.action_message != old.action_message || !g.server.IsObjectAlive(now.source) ||
                (reloaded && (now.source == old.source || g.objects.get(old.source))))
                return false;
        }
        const auto& name = snapshot.front().name;
        const auto digit = name.find_first_of("0123456789");
        if (digit == name.npos || digit == 0)
            return false;
        const auto prefix = name.substr(0, digit);
        PathList path;
        std::string error;
        if (!g.server_shell.paths.GetPath(prefix, path, error) || path.empty())
            return false;
        for (std::size_t i = 1; i < path.size(); ++i)
            if (g.objects.get(path[i - 1].source)->id.slot <= g.objects.get(path[i].source)->id.slot)
                return false;
        return true;
    }
} // namespace OpenBlood2
