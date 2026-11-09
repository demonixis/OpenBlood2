#pragma once
#include "game/ballistic_settings.h"

namespace OpenBlood2
{
    // Original Shared/SFXMsgIds.h WFX flags. Unsent extras have the source defaults.
    enum WeaponEffectFlag : std::uint32_t
    {
        WFX_MUZZLESMOKE = 0x00000001,
        WFX_MUZZLELIGHT = 0x00000002,
        WFX_EJECTSHELL = 0x00000004,
        WFX_LEFTHANDED = 0x00000008,
        WFX_TRACER = 0x00000010,
        WFX_PARTICLETRAIL = 0x00000020,
        WFX_MARK = 0x00010000,
        WFX_FLASH = 0x00020000,
        WFX_SPARKS = 0x00040000,
        WFX_SPLASH = 0x00080000,
        WFX_SMOKE = 0x00100000,
        WFX_SOUND = 0x00200000,
        WFX_BLOODSPLAT = 0x00400000,
        WFX_BLOODSPURT = 0x00800000,
        WFX_EXPLOSION = 0x01000000,
        WFX_FRAGMENTS = 0x02000000,
        WFX_IMPACTLIGHT = 0x04000000,
        WFX_SCREENSHAKE = 0x08000000
    };
    inline constexpr std::uint32_t weapon_ballistics =
        WFX_MUZZLESMOKE | WFX_EJECTSHELL | WFX_TRACER | WFX_MARK | WFX_SPLASH | WFX_FRAGMENTS;

    struct LegacyWeaponEffect
    {
        LegacyWeaponEffect()
        {
            shot.ammo = AMMO_NONE;
        }
        ShotgunEffectSettings shot;
        std::uint32_t flags = 0;
        float damage = 0;
        DBYTE explosion = 0;
    };
} // namespace OpenBlood2
