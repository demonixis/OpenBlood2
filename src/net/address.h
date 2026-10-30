#pragma once
#include "net/transport.h"
namespace OpenBlood2::Net
{
    struct ServerAddress
    {
        std::string host;
        std::uint16_t port = default_port;
    };
    bool parse_address(std::string_view, std::uint16_t default_value, ServerAddress&, std::string& error);
} // namespace OpenBlood2::Net
