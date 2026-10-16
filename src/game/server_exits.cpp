#include "game/server_adapter.h"

namespace OpenBlood2
{
    DRESULT ServerAdapter::SendWorldExit(const WorldExit& exit, const std::string& start_point)
    {
        trace_service("SendWorldExit");
        if (!session_shell || start_point.size() > 100 || start_point.find('\0') != std::string::npos ||
            exit.next_world.size() > 100 || exit.next_world.find('\0') != std::string::npos ||
            static_cast<DBYTE>(exit.type) > static_cast<DBYTE>(WorldExitType::Episode))
            return DE_INVALIDPARAMS;
        if (!start_point.empty())
            session_shell->SetStartPointName(start_point);
        // Original null-client broadcast, bounded to the one supported local client.
        for (const auto& [client, state] : clients_)
        {
            (void)state;
            if (GetClientUserData(client))
                return SendToClient(client, exit);
        }
        return DE_NOTFOUND;
    }
} // namespace OpenBlood2
