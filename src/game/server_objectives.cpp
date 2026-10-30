#include "game/server_adapter.h"
namespace OpenBlood2
{
    DRESULT ServerAdapter::SendConversation(const Conversation& value)
    {
        if (!trace_service("SendConversation"))
            return DE_ERROR;
        return broadcast_client_event(value);
    }
    DRESULT ServerAdapter::SendObjectives(const MissionObjectives& value)
    {
        if (!trace_service("SendObjectives"))
            return DE_ERROR;
        // Original null-client broadcast reaches every associated player.
        return broadcast_client_event(value);
    }
    void ServerAdapter::load_client_strings()
    {
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
    }
    bool ServerAdapter::FindClientResourceString(std::uint32_t id, std::string& value)
    {
        trace_service("FindClientResourceString");
        value.clear();
        load_client_strings();
        const auto it = client_strings.strings.find(id);
        if (it == client_strings.strings.end())
            return false;
        value = it->second;
        return true;
    }
    bool ServerAdapter::GetClientResourceString(std::uint32_t id, std::string& value)
    {
        trace_service("GetClientResourceString");
        value.clear();
        load_client_strings();
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
