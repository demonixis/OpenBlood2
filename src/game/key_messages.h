#pragma once
#include "game/legacy/basedefs_de.h"
#include <string>
#include <string_view>

namespace OpenBlood2
{
    // Original message IDs, owned in-process payloads, not the old packet ABI.
    inline constexpr DDWORD key_pickup_message = 3500, key_query_message = 3501, key_response_message = 3502;
    inline constexpr DDWORD picked_up_message = 1005;
    struct KeyItem
    {
        std::string name, display_name, icon, highlighted_icon;
        DBYTE uses = 0;
    };
    struct KeyMessage
    {
        KeyItem item;
        bool have = false;
    };
    bool same_name(std::string_view, std::string_view);
} // namespace OpenBlood2
