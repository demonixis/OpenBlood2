#include "game/server_adapter.h"
namespace OpenBlood2
{
    DRESULT ServerAdapter::SendObjectives(const MissionObjectives& value)
    {
        trace_service("SendObjectives");
        // Original null-client broadcast, bounded to the supported local client.
        for (const auto& [client, state] : clients_)
        {
            (void)state;
            if (GetClientUserData(client))
                return SendToClient(client, value);
        }
        return DE_NOTFOUND;
    }
    bool ServerAdapter::GetClientResourceString(std::uint32_t id, std::string& value)
    {
        trace_service("GetClientResourceString");
        value.clear();
        if (!client_strings_attempted)
        {
            client_strings_attempted = true;
            std::vector<std::uint8_t> bytes;
            if (!client_resources)
                client_strings_error = "Client resource archive is unavailable";
            else if (const auto* resource = client_resources->find("cres.dll");
                     resource && resource->size > 32 * 1024 * 1024)
                client_strings_error = "cres.dll exceeds the 32 MiB string reader limit";
            else if (client_resources->read("cres.dll", bytes, client_strings_error))
                read_string_resources(bytes, client_strings, client_strings_error);
        }
        auto it = client_strings.strings.find(id);
        if (it == client_strings.strings.end())
        {
            objects_.fail(ObjectError::Unsupported, client_strings_error.empty()
                                                        ? "Missing client string resource " + std::to_string(id)
                                                        : client_strings_error);
            return false;
        }
        value = it->second;
        return true;
    }
} // namespace OpenBlood2
