#include "net/address.h"
#include <algorithm>
#include <charconv>
namespace OpenBlood2::Net
{
    bool parse_address(std::string_view text, std::uint16_t default_value, ServerAddress& out, std::string& error)
    {
        error.clear();
        ServerAddress next;
        next.port = default_value;
        const auto colon = text.find(':');
        auto host = text.substr(0, colon);
        if (host.empty() || host.size() > 253 ||
            !std::all_of(host.begin(), host.end(),
                         [](unsigned char c)
                         {
                             return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                    c == '-' || c == '.';
                         }))
        {
            error = "Expected an IPv4 address or hostname, optionally followed by :port";
            return false;
        }
        if (colon != text.npos)
        {
            auto port = text.substr(colon + 1);
            const auto value = std::from_chars(port.data(), port.data() + port.size(), next.port);
            if (port.empty() || value.ec != std::errc{} || value.ptr != port.data() + port.size())
            {
                error = "Server port must be an integer from 1 to 65535";
                return false;
            }
        }
        if (!next.port)
        {
            error = "Server port must be nonzero";
            return false;
        }
        next.host = host;
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2::Net
