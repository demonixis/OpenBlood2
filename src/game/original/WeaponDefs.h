#pragma once
#include "basedefs_de.h"
// Selected original definitions; complete source fields, base-game weapon numbering.
inline constexpr DBYTE WEAP_BERETTA = 1, WEAP_SUBMACHINEGUN = 2, WEAP_FLAREGUN = 3, WEAP_ASSAULTRIFLE = 9,
                       WEAP_MELEE = 17, WEAP_NAGA_EYEBEAM = 38, WEAP_ZEALOT_ENERGYBLAST = 27, WEAP_SHIKARI_CLAW = 18,
                       WEAP_SHIKARI_SPIT = 19, WEAP_SOUL_CROWBAR = 20, WEAP_SOUL_AXE = 21, WEAP_SOUL_PIPE = 22,
                       WEAP_SOUL_HOOK = 23, WEAP_GIDEON_SHIELD = 41, WEAP_GIDEON_WIND = 42, WEAP_PROXIMITYBOMB = 49,
                       WEAP_REMOTEBOMB = 50;
inline constexpr DBYTE WEAP_TESLACANNON = 13, WEAP_VOODOO = 14;
inline constexpr DBYTE WEAP_SHOTGUN = 4, WEAP_SINGULARITY = 8;
inline constexpr DBYTE WEAP_SNIPERRIFLE = 5; // Pickup offer only; the weapon is not integrated.
inline constexpr DBYTE AMMO_PROXIMITYBOMB = 10, AMMO_REMOTEBOMB = 11;
inline constexpr bool inventory_bomb(DBYTE type)
{
    return type == WEAP_PROXIMITYBOMB || type == WEAP_REMOTEBOMB;
}
// Original InventoryMgr.cpp IsDemoWeapon: the player weapons of the _DEMO build.
inline constexpr bool IsDemoWeapon(DBYTE type)
{
    return type == WEAP_MELEE || type == WEAP_BERETTA || type == WEAP_SUBMACHINEGUN || type == WEAP_ASSAULTRIFLE ||
           type == WEAP_VOODOO || type == WEAP_TESLACANNON;
}
inline constexpr unsigned inventory_weapon_slots = 13;
inline constexpr DBYTE TYPE_RIFLE = 0, TYPE_AUTORIFLE = 1, TYPE_PISTOL = 2, TYPE_MELEE = 4, TYPE_MAGIC = 8,
                       AMMO_NONE = 0, AMMO_BULLET = 1, AMMO_SHELL = 2, AMMO_BMG = 3, AMMO_FLARE = 4, AMMO_DIEBUGDIE = 5,
                       AMMO_BATTERY = 8, AMMO_FOCUS = 9;
inline constexpr int IDS_WEAPON_BERETTA = 350, IDS_WEAPON_MAC10 = 354, IDS_WEAPON_FLAREGUN = 355,
                     IDS_WEAPON_KNIFE = 366, IDS_WEAPON_M16 = 353, IDS_WEAPON_SNIPER = 352, IDS_WEAPON_TESLA = 364,
                     IDS_WEAPON_SHOTGUN = 351, IDS_WEAPON_SINGULARITY = 365, IDS_WEAPON_VOODOO = 360;
// Weapon spread structure
typedef struct Spread_t
{
    DFLOAT h;
    DFLOAT v;
} Spread;

struct VectorInit
{
    DFLOAT x;
    DFLOAT y;
    DFLOAT z;
};

// Weapon flags
#define WEAPFLAG_SEMIAUTO 1
#define WEAPFLAG_ALTZOOM 0
#define DUAL_HAND_MASK 128
#define MAX_FIRE_SOUNDS 4

#define WEAPON_SOUND_DRYFIRE 1
#define WEAPON_SOUND_KEY 2
#define WEAPON_SOUND_FIRE 3

#define INVALID_ANI ((DDWORD) - 1)

// Structure to hold initial values for weapons
typedef struct WeaponData_t
{
    const char* m_szViewModelFilename;
    const char* m_szLeftViewModelFilename;
    const char* m_szViewModelSkin;
    const char* m_szHandModelFilename;
    const char* m_szHandModelSkin;
    DBYTE m_nType;               // Weapon type
    DBYTE m_nFireType;           // Weapon animation
    DBYTE m_nAmmoType;           // Type of ammo
    std::uint16_t m_nAmmoUse;    // Amount of ammo to use per shot
    std::uint16_t m_nAltAmmoUse; // Amount of ammo to use per Alt shot
    DFLOAT m_fMinDamage;         // minimum amount of damage per shot vector
    DFLOAT m_fMaxDamage;         // maximum amount of damage per shot vector
    DFLOAT m_fMinAltDamage;      // Alt minimum fire damage
    DFLOAT m_fMaxAltDamage;      // Alt maximum fire damage
    DFLOAT m_fReloadTime;        // How long to reload?
    DFLOAT m_fAltReloadTime;     // How long to reload for alt fire?
    Spread m_Spread;             // Spread;
    Spread m_AltSpread;
    DFLOAT m_fProjVelocity;
    DFLOAT m_fAltProjVelocity;
    DFLOAT m_fRange;
    DFLOAT m_fAltRange;
    DDWORD m_dwShotsPerFire;
    DDWORD m_dwAltShotsPerFire;
    DDWORD m_dwStrengthReq;
    DDWORD m_dwTwoHandStrengthReq;
    int m_nDamageRadius;
    int m_nAltDamageRadius;
    DBOOL m_bAltFireZoom;                // Alt fire for this weapon is a zoom function.
    DBOOL m_bSemiAuto;                   // Weapon has semi-auto firing
    const char* m_szFireSound;           // Sound made when weapon fires.
    const char* m_szAltFireSound;        // Sound made when weapon fires.
    const char* m_szEmptyWeaponSound;    // Sound made when weapon is empty
    const char* m_szAltEmptyWeaponSound; // Sound made when weapon is empty
    const char* m_szProjectileClass;
    const char* m_szAltProjectileClass;
    DFLOAT m_fFlashRadius;
    VectorInit m_vFlashColor;
    // Info for client
    const char* m_szWeaponName;
    int m_nWeaponNameID;
    const char* m_szFlashSprite;
    const char* m_szAltFlashSprite;
    DFLOAT m_fFlashDuration;
    DFLOAT m_fFlashScale;
    VectorInit m_vHandModelOffset; // Offset for the 3rd person view
    VectorInit m_vViewModelOffset; // Gun offset in relation to player
    VectorInit m_vMuzzleOffset;    // Muzzle Offset
    VectorInit m_vRecoil;          // Gun's recoil
    VectorInit m_vFlash;           // Flash position
    DFLOAT m_fEjectInterval;       // How often to eject shells
    DFLOAT m_fViewKick;            // Amount to adjust view when firing.
    DBOOL m_bCumulativeKick;       // Kick is Cumulative
    DBOOL m_bLoopAnim;             // Anim is looping
    DBOOL m_bAltLoopAnim;          // Alt Anim is looping
    const char* m_szStatusIcon;    // Icon displayed on the status bar
    const char* m_szStatusIconH;   // Highlighed icon displayed on the status bar
} WeaponData;

// Weapon states
enum WeaponState
{
    WS_REST,
    WS_IDLE,
    WS_DRAW,
    WS_HOLSTER,
    WS_START_FIRING,
    WS_FIRING,
    WS_STOP_FIRING,
    WS_START_ALT_FIRING,
    WS_ALT_FIRING,
    WS_STOP_ALT_FIRING,
    WS_HOLSTERED // holstered/inactive state (Greg 9/18)
};

const WeaponData* weapon_definition(DBYTE type);
