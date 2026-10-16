#include "Sparam.h"
#include <algorithm>
#include <array>

SparamStatus Sparam_Get(std::string_view source, std::string_view id, std::string_view& value)
{
    if (source.size() > 4096 || source.find('\0') != source.npos || id.empty() || id.size() > 252 ||
        id.find_first_of("[]:") != id.npos || id.find('\0') != id.npos)
        return SparamStatus::Invalid;
    std::array<char, 256> storage{};
    storage[0] = '[';
    std::copy(id.begin(), id.end(), storage.begin() + 1);
    storage[id.size() + 1] = ':';
    const std::string_view marker(storage.data(), id.size() + 2);
    const auto at = source.find(marker);
    if (at == source.npos)
        return SparamStatus::Missing;
    const auto start = at + marker.size();
    const auto end = source.find(']', start);
    if (end == source.npos || end == start || end - start > 255)
        return SparamStatus::Invalid;
    value = source.substr(start, end - start);
    return SparamStatus::Found;
}
