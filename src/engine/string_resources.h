#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
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
} // namespace OpenBlood2
