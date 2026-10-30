#pragma once
#include "basedefs_de.h"
#include <array>
#include <optional>
#include <string>
namespace OpenBlood2
{
    // One SMSG_ITEMCHANGED item: original picture names (interface/statusbar/items/),
    // the cres.dll name resource (0 for keys) or a key's authored display name, and
    // GetItemCharges.
    struct InventoryItemView
    {
        DBYTE type = 0;
        std::string icon, highlighted_icon, name;
        std::uint32_t name_id = 0;
        DBYTE charges = 0;
        bool operator==(const InventoryItemView&) const = default;
    };
    // Original SMSG_ITEMCHANGED values: current, previous and next items around the
    // cursor; show is ITEMF_SHOWITEMS (a one-shot request to slide the tabs in).
    struct InventoryItems
    {
        std::optional<InventoryItemView> current, previous, next;
        bool show = false;
        bool operator==(const InventoryItems&) const = default;
    };
    struct PlayerStats
    {
        float health = 0, armor = 0, bullets = 0;
        float ammo = 0, alternate_ammo = 0;
        DBYTE weapon = 0, weapon_slot = 0, ammo_type = 0, alternate_ammo_type = 0;
        std::array<DBYTE, 10> weapon_slots{};
        float max_health = 100, max_armor = 100;
        bool operator==(const PlayerStats&) const = default;
    };
} // namespace OpenBlood2
