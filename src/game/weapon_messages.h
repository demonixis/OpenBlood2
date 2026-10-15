#pragma once
#include "basedefs_de.h"

namespace OpenBlood2
{
    // Local equivalents of CMSG_WEAPONFIRE / MID_DAMAGE. No retail packet ABI.
    struct WeaponFire
    {
        DVector position{}, direction{0, 0, 1};
        DBYTE seed = 2, weapon = 1;
        bool alternate = false, left = false;
    };
    enum class DamageType : DBYTE
    {
        Normal = 0,
        Melee = 4
    };
    struct Damage
    {
        DVector direction{}, position{};
        float amount = 0;
        DamageType type = DamageType::Normal;
        HOBJECT attacker = nullptr;
    };
    inline constexpr DDWORD damage_message = 1001; // Source AppHeaders/generic_msg_de.h MID_DAMAGE.
    struct SegmentHit
    {
        DVector point{}, normal{};
        HOBJECT object = nullptr;
        std::uint32_t polygon = UINT32_MAX;
        double fraction = 1;
    };
    enum class SegmentResult
    {
        Invalid,
        Miss,
        Hit,
        Unavailable
    };
} // namespace OpenBlood2
