#pragma once
#include "game/key_messages.h"
#include <array>
#include <optional>
#include <vector>

namespace OpenBlood2
{
    // Owned source-level subset of CInventoryMgr::Save, not a retail save-file ABI.
    // No engine handles, weapon pointers, client views or resource references.
    struct InventoryState
    {
        std::array<DBYTE, 13> right{}, left{};
        std::array<float, 13> ammo{};
        std::vector<KeyItem> keys;
        std::optional<DBYTE> medkit_charge;
        DBYTE strength = 1, magic = 1, current_slot = 0;
        bool infinite_ammo = false;
        float damage_multiplier = 1;
    };
    enum class InventoryLoadType
    {
        Restore = 0,
        KeepAlive = 1
    };
} // namespace OpenBlood2
