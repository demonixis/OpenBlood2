#pragma once
#include "engine/audio.h"
#include "game/legacy/basedefs_de.h"
#include <optional>
#include <string>
#include <string_view>
namespace OpenBlood2
{
    inline constexpr DDWORD add_ammo_message = 1009, add_powerup_message = 3504, add_weapon_message = 1004,
                            add_item_message = 1006;
    enum class PickupKind
    {
        Ammo,
        Powerup,
        Weapon,
        Item
    };
    struct PickupMessage
    {
        PickupKind kind = PickupKind::Ammo;
        DBYTE type = 0;
        float amount = 0;
        bool operator==(const PickupMessage&) const = default;
    };
    struct SpawnReport
    {
        std::string source, request, outcome;
        HOBJECT child = nullptr; // Checked token only; never an owning/raw object pointer.
        DVector position{}, velocity{};
    };
    struct PickupReport
    {
        std::string name, class_name, sound, sound_error, key;
        std::optional<PickupMessage> value;
        DVector position{};
        SoundId voice = 0;
    };
    bool split_spawn(std::string_view, std::string& class_name, std::string& properties);
} // namespace OpenBlood2
