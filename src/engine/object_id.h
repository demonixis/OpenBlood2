#pragma once
#include <cstdint>

namespace OpenBlood2
{
    struct ObjectId
    {
        std::uint32_t slot = UINT32_MAX, generation = 0;
        bool operator==(const ObjectId&) const = default;
        explicit operator bool() const
        {
            return slot != UINT32_MAX;
        }
    };
} // namespace OpenBlood2
