#pragma once
namespace OpenBlood2
{
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
