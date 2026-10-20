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
        float projectile_speed = 1000; // Original FIREMSG_SPECIAL_FLOAT for inventory bombs only.
        DBYTE voodoo_hit = 0;          // Original FIREMSG_SPECIAL_DWORD, bounded to the five primary animations.
    };
    enum class DamageType : DBYTE
    {
        Normal = 0,
        Fire = 1,
        Singularity = 3, // Singularity generator (both projectiles).
        Melee = 4,
        Explode = 5,
        Suffocate = 6,
        Death = 7,
        FreeFall = 8,
        Blind = 9,
        Slow = 10,
        DropWeapon = 11,
        Vision = 12,
        Electric = 13, // Laser and lightning (Tesla alternate, TeslaBall bolts).
        Acid = 14,
        Leech = 15
    };
    struct Damage
    {
        DVector direction{}, position{};
        float amount = 0;
        DamageType type = DamageType::Normal;
        HOBJECT attacker = nullptr;
        bool area_effect = false; // Source DAMAGE_FLAG_AREAEFFECT, separate from the base type.
    };
    inline constexpr DDWORD immobilize_message = 3505; // Original MID_IMMOBILIZE.
    inline constexpr DDWORD damage_message = 1001;     // Source AppHeaders/generic_msg_de.h MID_DAMAGE.
    struct SegmentHit
    {
        DVector point{}, normal{};
        HOBJECT object = nullptr;
        std::uint32_t polygon = UINT32_MAX;
        double fraction = 1;
        std::uint16_t texture_flags = 0; // Baked material identifier, distinct from polygon render flags.
    };
    enum class SegmentObjects
    {
        SolidOnly,
        All,
        SolidOrRayHit
    };
    enum class SegmentStart
    {
        IncludeInterior,
        SkipInterior
    };
    enum class SegmentFilter
    {
        None,
        AIShot,                 // Original visibility filter plus IGNORE_DESTRUCTABLE models.
        AIVisibility,           // Original IGNORE_LIQUID | IGNORE_GLASS object filter.
        IgnoreCharacters,       // Original CMovement clearance filter.
        IgnoreVolumes,          // Original LiquidFilterFn excludes every VolumeBrush subclass.
        ProjectileContact,      // Original projectile ignores its shooter and exact projectile class.
        IgnoreAncientTentacles, // Original AncientOneFilterFn, both beam queries.
        IgnoreOwnerClass        // Original IgnoreTypeFilterFn: owner class and derived classes, visual beam only.
    };
    enum class SegmentResult
    {
        Invalid,
        Miss,
        Hit,
        Unavailable
    };
} // namespace OpenBlood2
