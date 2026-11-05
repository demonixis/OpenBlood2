#pragma once
#include <cstdint>

namespace OpenBlood2
{
    // Original NetDefs.h LEVEL_* and PickupObject.h ConvertNetItemMultiplier.
    enum class PickupLevel : std::uint8_t
    {
        None,
        Half,
        Normal,
        Double,
        Insane
    };
    constexpr float pickup_multiplier(PickupLevel level)
    {
        switch (level)
        {
            case PickupLevel::None:
                return 0;
            case PickupLevel::Half:
                return .5f;
            case PickupLevel::Double:
                return 2;
            case PickupLevel::Insane:
                return 3;
            default:
                return 1;
        }
    }
    struct PickupRule
    {
        PickupLevel level = PickupLevel::Normal;
        unsigned respawn_seconds = 0;
    };
    struct MultiplayerPickups
    {
        // Shared original host/dedicated defaults. Values are applied at creation;
        // string-spawned drops subsequently force their respawn time to zero.
        PickupRule ammo{PickupLevel::Normal, 15};
        PickupRule armor{PickupLevel::Normal, 45};
        PickupRule health{PickupLevel::Normal, 30};
        PickupRule powerups{PickupLevel::Normal, 60};
    };
} // namespace OpenBlood2
