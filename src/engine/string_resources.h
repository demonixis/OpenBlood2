#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace OpenBlood2
{
    struct StringResources
    {
        std::uint16_t language = 0;
        std::map<std::uint32_t, std::string> strings;
    };
    // Read data only: no executable loading, native ABI or platform APIs.
    // language=0 requires a single resource language; ambiguous tables reject.
    bool read_string_resources(std::span<const std::uint8_t>, StringResources&, std::string& error,
                               std::uint16_t language = 0);
    // Raw bytes of a named custom resource (FindResource(module, name, type) with string
    // type and name, e.g. TEXT/CREDITS), read as data only. Type and name compare
    // case-insensitively; the entry must have exactly one language.
    bool read_named_resource(std::span<const std::uint8_t>, std::string_view type, std::string_view name,
                             std::vector<std::uint8_t>& out, std::string& error);
} // namespace OpenBlood2
