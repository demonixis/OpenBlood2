#pragma once
#include "game/legacy/basedefs_de.h"
#include <string>
#include <string_view>
namespace OpenBlood2
{
    inline constexpr DDWORD add_ammo_message = 1009, add_powerup_message = 3504;
    enum class PickupKind
    {
        Ammo,
        Powerup
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
        std::string name, class_name, sound;
        PickupMessage value;
    };
    bool split_spawn(std::string_view, std::string& class_name, std::string& properties);
} // namespace OpenBlood2
