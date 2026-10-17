#include "WeaponDefs.h"
namespace
{
    constexpr WeaponData definitions[] = {
        // Ingram Mac-10 Sub-machine Gun
        {
            "Models\\Weapons\\Mac10_pv.abc",              // Model
            "Models\\Weapons\\lh_Mac10_pv.abc",           // Model
            "Skins\\Weapons\\C_Mac10_pv_t.dtx",           // Skin
            "Models\\Powerups\\Mac10_pu.abc",             // 3rd person model
            "Skins\\Powerups\\Mac10_pu.dtx",              // 3rd person skin
            WEAP_SUBMACHINEGUN,                           // Type
            TYPE_PISTOL,                                  // Fire type
            AMMO_BULLET,                                  // Ammo type
            1,                                            // Ammo Use
            1,                                            // Alt Ammo Use
            8.0f,                                         // Min Damage
            12.0f,                                        // Max Damage
            8.0f,                                         // Min Alt damage
            12.0f,                                        // Max Alt damage
            0.1f,                                         // Reload time
            0.5f,                                         // Alt reload time
            {200.0f, 100.0f},                             // Spread
            {75.0f, 25.0f},                               // Alt Spread
            0.0f,                                         // Projectile Velocity
            0.0f,                                         // Alt Projectile Velocity
            3500.0f,                                      // Range
            3500.0f,                                      // Alt Range
            1,                                            // Shots per fire
            1,                                            // Alt Shots per fire
            2,                                            // Strength or Magic required
            3,                                            // Two-handed Strength or Magic required
            0,                                            // Damage Radius (projectiles)
            0,                                            // Alt Damage Radius (projectiles)
            DFALSE,                                       // Alt fire zoom
            DFALSE,                                       // Semi-auto
            "Sounds\\weapons\\mac10\\fire.wav",           // Fire sound
            "Sounds\\weapons\\mac10\\alt.wav",            // Alt Fire sound
            "",                                           // Empty weapon sound
            "",                                           // Alt Empty weapon sound
            NULL,                                         // Projectile class
            NULL,                                         // Alt Projectile class
            100,                                          // Muzzle Flash radius
            {1.0f, 0.68f, 0.35f},                         // Muzzle Flash color
            "Ingram Mac-10",                              // Name
            IDS_WEAPON_MAC10,                             // Name ID (resource)
            "Sprites\\mac10flare.spr",                    // flash sprite name
            "Sprites\\mac10flare.spr",                    // alt flash Sprite name
            0.01f,                                        // Flash duration
            0.11f,                                        // Flash scale
            {0.0f, 0.0f, 0.0f},                           // 3rd person gun offset
            {0.8f, -0.7f, 2.0f},                          // Gun Offset
            {1.0f, 0.0f, 1.0f},                           // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                        // Recoil
            {8.0f, -5.75f, 28.0f},                        // Flash position
            0.625f,                                       // Eject interval
            0.0f,                                         // View kick
            DFALSE,                                       // View kick is Cumulative
            DTRUE,                                        // Animation looping
            DTRUE,                                        // Alt anim looping
            "interface\\statusbar\\weapons\\mac10.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\mac10_h.pcx", // Highlighted icon for status bar
        },

        // Beretta (Pistol)
        {
            "Models\\Weapons\\Beretta_pv.abc",    // View Model
            "Models\\Weapons\\lh_Beretta_pv.abc", // View Model
            "Skins\\Weapons\\C_Beretta_pv_t.dtx", // View Skin
            "Models\\Powerups\\beretta_pu.abc",   // 3rd person model
            "Skins\\Powerups\\beretta_pu.dtx",    // 3rd person skin
            WEAP_BERETTA,                         // Type
            TYPE_PISTOL,                          // Fire type
            AMMO_BULLET,                          // Ammo type
            1,                                    // Ammo Use
            1,                                    // Alt Ammo Use
            8.0f,                                 // Min Damage
            12.0f,                                // Max Damage
            10.0f,                                // Min Alt damage
            20.0f,                                // Max Alt damage
            0.33f,                                // Reload time
            0.5f,                                 // Alt reload time
            {75.0f, 25.0f},                       // Spread
            {250.0f, 150.0f},                     // Alt Spread
            0.0f,                                 // Projectile Velocity
            0.0f,                                 // Alt Projectile Velocity
            3500.0f,                              // Range
            3500.0f,                              // Alt Range
            1,                                    // Shots per fire
            1,                                    // Alt Shots per fire
            1,                                    // Strength or Magic required
            2,                                    // Two-handed Strength or Magic required
            0,                                    // Damage Radius (projectiles)
            0,                                    // Alt Damage Radius (projectiles)
            DFALSE,                               // Alt fire zoom
            DTRUE,                                // Semi-auto
            "Sounds\\weapons\\beretta\\fire.wav", // Fire sound
            "Sounds\\weapons\\beretta\\fire.wav", // Fire sound for alt fire
            "",                                   // Empty weapon sound
            "",                                   // Alt Empty weapon sound
            NULL,                                 // Projectile class
            NULL,                                 // Alt Projectile class
            100,                                  // Muzzle Flash radius
            {1.0f, 0.68f, 0.35f},                 // Muzzle Flash color
            // Client info
            "Beretta 92F",                                  // Name
            IDS_WEAPON_BERETTA,                             // Name ID (resource)
            "Sprites\\pistolflare.spr",                     // flash sprite name
            "Sprites\\pistolflare.spr",                     // alt flash sprite name
            0.01f,                                          // Flash duration
            0.12f,                                          // Flash scale
            {0.0f, 0.0f, 0.0f},                             // 3rd person gun offset
            {0.6f, -0.6f, 1.9f},                            // Gun Offset
            {0.15f, 0.1f, 2.0f},                            // Muzzle Offset
            {0.03f, 0.03f, 2.55f},                          // Recoil
            {7.5f, -5.6f, 28.0f},                           // Flash Offset
            0.625f,                                         // Eject interval
            0.0f,                                           // View kick
            DFALSE,                                         // View kick is Cumulative
            DFALSE,                                         // Animation looping
            DTRUE,                                          // Alt anim looping
            "interface\\statusbar\\weapons\\beretta.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\beretta_h.pcx", // Highlighted icon for status bar
        },
        // Melee Hand Weapon
        {
            "Models\\Weapons\\c_knife_pv.abc",              // Model
            "Models\\Weapons\\c_knife_pv.abc",              // Model
            "Skins\\Weapons\\C_knife_pv_t.dtx",             // Skin
            "Models\\Powerups\\knifec_pu.abc",              // Model
            "Skins\\Powerups\\knifec_pu.dtx",               // Skin
            WEAP_MELEE,                                     // Type
            TYPE_MELEE,                                     // Fire type
            AMMO_NONE,                                      // Ammo type
            1,                                              // Ammo Use
            1,                                              // Alt Ammo Use
            15.0f,                                          // Min Damage
            25.0f,                                          // Max Damage
            35.0f,                                          // Min Alt damage
            60.0f,                                          // Max Alt damage
            0.0f,                                           // Reload time
            0.0f,                                           // Alt reload time
            {0.0f, 0.0f},                                   // Spread
            {0.0f, 0.0f},                                   // Alt Spread
            0.0f,                                           // Projectile Velocity
            0.0f,                                           // Alt Projectile Velocity
            75.0f,                                          // Range
            75.0f,                                          // Alt Range
            1,                                              // Shots per fire
            1,                                              // Alt Shots per fire
            1,                                              // Strength or Magic required
            0,                                              // Two-handed Strength or Magic required
            0,                                              // Damage Radius (projectiles)
            0,                                              // Alt Damage Radius (projectiles)
            DFALSE,                                         // Alt fire zoom
            DFALSE,                                         // Semi-auto
            "Sounds\\Weapons\\Knife\\fire.wav",             // Fire sound
            "Sounds\\Weapons\\Knife\\alt.wav",              // Alt Fire sound
            "",                                             // Empty weapon sound
            "",                                             // Alt Empty weapon sound
            NULL,                                           // Projectile class
            NULL,                                           // Alt Projectile class
            0,                                              // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                             // Muzzle Flash color
            "Chosen Knife",                                 // Name
            IDS_WEAPON_KNIFE,                               // Name ID (resource)
            "",                                             // flash sprite name
            "",                                             // alt flash Sprite name
            0.0f,                                           // Flash duration
            0.0f,                                           // Flash scale
            {0.0f, 0.0f, 0.0f},                             // 3rd person gun offset
            {-0.4f, -1.2f, 0.1f},                           // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},                             // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                          // Recoil
            {0.0f, 0.0f, 0.1f},                             // Flash position
            0.625f,                                         // Eject interval
            0.0f,                                           // View kick
            DFALSE,                                         // View kick is Cumulative
            DFALSE,                                         // Animation looping
            DFALSE,                                         // Alt anim looping
            "interface\\statusbar\\weapons\\c_knife.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\c_knife_h.pcx", // Highlighted icon for status bar
        },
        // Zealot energy blast
        {
            "Models\\Weapons\\knifec_pv.abc",        // Model
            "Models\\Weapons\\knifec_pv.abc",        // Model
            "Skins\\Weapons\\knifec_pv_t.dtx",       // Skin
            nullptr,                                 // Model
            nullptr,                                 // Skin
            WEAP_ZEALOT_ENERGYBLAST,                 // Type
            TYPE_MELEE,                              // Fire type
            AMMO_NONE,                               // Ammo type
            1,                                       // Ammo Use
            1,                                       // Alt Ammo Use
            25.0f,                                   // Min Damage
            50.0f,                                   // Max Damage
            25.0f,                                   // Min Alt damage
            50.0f,                                   // Max Alt damage
            0.50f,                                   // Reload time
            0.50f,                                   // Alt reload time
            {0.0f, 0.0f},                            // Spread
            {0.0f, 0.0f},                            // Alt Spread
            1000.0f,                                 // Projectile Velocity
            1000.0f,                                 // Alt Projectile Velocity
            1000.0f,                                 // Range
            1000.0f,                                 // Alt Range
            1,                                       // Shots per fire
            1,                                       // Alt Shots per fire
            1,                                       // Strength or Magic required
            0,                                       // Two-handed Strength or Magic required
            175,                                     // Damage Radius (projectiles)
            175,                                     // Alt Damage Radius (projectiles)
            DFALSE,                                  // Alt fire zoom
            DFALSE,                                  // Semi-auto
            "Sounds\\Enemies\\Zealot\\distfire.wav", // Fire sound
            "Sounds\\Enemies\\Zealot\\distfire.wav", // Alt Fire sound
            "Sounds\\Enemies\\Zealot\\distfire.wav", // Empty weapon sound
            "Sounds\\Enemies\\Zealot\\distfire.wav", // Alt Empty weapon sound
            "CEnergyBlastProjectile",                // Projectile class
            "CEnergyBlastProjectile",                // Alt Projectile class
            20,                                      // Muzzle Flash radius
            {0.0f, 0.0f, 1.0f},                      // Muzzle Flash color
            "Energy_Blast",                          // Name
            0,                                       // Name ID (resource)
            "",                                      // flash sprite name
            "",                                      // alt flash Sprite name
            0.25f,                                   // Flash duration
            0.40f,                                   // Flash scale
            {0.0f, 0.0f, 0.0f},                      // 3rd person gun offset
            {0.0f, -1.4f, 0.3f},                     // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},                      // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                   // Recoil
            {0.0f, 0.0f, 0.1f},                      // Flash position
            0.625f,                                  // Eject interval
            0.0f,                                    // View kick
            DFALSE,                                  // View kick is Cumulative
            DFALSE,                                  // Animation looping
            DFALSE,                                  // Alt anim looping
            nullptr,                                 // Icon for status bar
            nullptr,                                 // Highlighted icon for status bar
        },

        // Naga Eye Beam
        {
            "Models\\Weapons\\Laserrifle_pv.abc",  // Model
            "Models\\Weapons\\Laserrifle_pv.abc",  // Model
            "Skins\\Weapons\\C_DeathRay_pv_t.dtx", // Skin
            nullptr,                               // Model
            nullptr,                               // Skin
            WEAP_NAGA_EYEBEAM,                     // Type
            TYPE_MELEE,                            // Fire type
            AMMO_NONE,                             // Ammo type
            1,                                     // Ammo Use
            1,                                     // Alt Ammo Use
            30.0f,                                 // Min Damage
            45.0f,                                 // Max Damage
            30.0f,                                 // Min Alt damage
            45.0f,                                 // Max Alt damage
            0.2f,                                  // Reload time
            0.05f,                                 // Alt reload time
            {0.0f, 0.0f},                          // Spread
            {0.0f, 0.0f},                          // Alt Spread
            0.0f,                                  // Projectile Velocity
            0.0f,                                  // Alt Projectile Velocity
            2000.0f,                               // Range
            2000.0f,                               // Alt Range
            1,                                     // Shots per fire
            1,                                     // Alt Shots per fire
            1,                                     // Strength or Magic required
            0,                                     // Two-handed Strength or Magic required
            0,                                     // Damage Radius (projectiles)
            0,                                     // Alt Damage Radius (projectiles)
            DFALSE,                                // Alt fire zoom
            DFALSE,                                // Semi-auto
            "Sounds\\Weapons\\laser\\fire.wav",    // Fire sound
            "",                                    // Alt Fire sound
                                                   //		"Sounds\\Weapons\\laser\\fire.wav",	// Alt Fire sound
            "",                                    // Empty weapon sound
            "",                                    // Alt Empty weapon sound
            NULL,                                  // Projectile class
            NULL,                                  // Alt Projectile class
            0,                                     // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                    // Muzzle Flash color
            "Naga_Beam",                           // Name
            0,                                     // Name ID (resource)
            "",                                    // flash sprite name
            "",                                    // alt flash Sprite name
            0.25f,                                 // Flash duration
            0.10f,                                 // Flash scale
            {0.0f, 0.0f, 0.0f},                    // 3rd person gun offset
            {0.7f, -0.9f, 2.1f},                   // Gun Offset
            {0.0f, 0.0f, 0.0f},                    // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                 // Recoil
            {4.25f, -4.25f, 25.0f},                // Flash position
            0.625f,                                // Eject interval
            0.0f,                                  // View kick
            DFALSE,                                // View kick is Cumulative
            DFALSE,                                // Animation looping
            DTRUE,                                 // Alt anim looping
            nullptr,                               // Icon for status bar
            nullptr,                               // Highlighted icon for status bar
        },

        {
            "Models\\Weapons\\knifec_pv.abc",      // Model
            "Models\\Weapons\\knifec_pv.abc",      // Model
            "Skins\\Weapons\\knifec_pv_t.dtx",     // Skin
            nullptr,                               // Model
            nullptr,                               // Skin
            WEAP_GIDEON_SHIELD,                    // Type
            TYPE_RIFLE,                            // Fire type
            AMMO_NONE,                             // Ammo type
            1,                                     // Ammo Use
            1,                                     // Alt Ammo Use
            10.0f,                                 // Min Damage
            20.0f,                                 // Max Damage
            10.0f,                                 // Min Alt damage
            20.0f,                                 // Max Alt damage
            1.00f,                                 // Reload time
            0.0f,                                  // Alt reload time
            {0.0f, 0.0f},                          // Spread
            {0.0f, 0.0f},                          // Alt Spread
            0.0f,                                  // Projectile Velocity
            0.0f,                                  // Alt Projectile Velocity
            75.0f,                                 // Range
            75.0f,                                 // Alt Range
            1,                                     // Shots per fire
            1,                                     // Alt Shots per fire
            1,                                     // Strength or Magic required
            0,                                     // Two-handed Strength or Magic required
            0,                                     // Damage Radius (projectiles)
            0,                                     // Alt Damage Radius (projectiles)
            DFALSE,                                // Alt fire zoom
            DFALSE,                                // Semi-auto
            "Sounds\\Enemies\\Gideon\\shield.wav", // Fire sound
            "Sounds\\Enemies\\Gideon\\shield.wav", // Alt Fire sound
            "Sounds\\Enemies\\Gideon\\shield.wav", // Empty weapon sound
            "Sounds\\Enemies\\Gideon\\shield.wav", // Alt Empty weapon sound
            nullptr,                               // Projectile class
            nullptr,                               // Alt Projectile class
            0,                                     // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                    // Muzzle Flash color
            "Gideon_Shield",                       // Name
            0,                                     // Name ID (resource)
            "",                                    // flash sprite name
            "",                                    // alt flash Sprite name
            0.25f,                                 // Flash duration
            0.10f,                                 // Flash scale
            {0.0f, 0.0f, 0.0f},                    // 3rd person gun offset
            {0.0f, -1.4f, 0.3f},                   // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},                    // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                 // Recoil
            {0.0f, 0.0f, 0.1f},                    // Flash position
            0.625f,                                // Eject interval
            0.0f,                                  // View kick
            DFALSE,                                // View kick is Cumulative
            DFALSE,                                // Animation looping
            DFALSE,                                // Alt anim looping
            nullptr,                               // Icon for status bar
            nullptr,                               // Highlighted icon for status bar
        },
        {
            "Models\\Weapons\\knifec_pv.abc",        // Model
            "Models\\Weapons\\knifec_pv.abc",        // Model
            "Skins\\Weapons\\knifec_pv_t.dtx",       // Skin
            nullptr,                                 // Model
            nullptr,                                 // Skin
            WEAP_GIDEON_WIND,                        // Type
            TYPE_RIFLE,                              // Fire type
            AMMO_NONE,                               // Ammo type
            1,                                       // Ammo Use
            1,                                       // Alt Ammo Use
            10.0f,                                   // Min Damage
            20.0f,                                   // Max Damage
            10.0f,                                   // Min Alt damage
            20.0f,                                   // Max Alt damage
            1.00f,                                   // Reload time
            0.0f,                                    // Alt reload time
            {0.0f, 0.0f},                            // Spread
            {0.0f, 0.0f},                            // Alt Spread
            0.0f,                                    // Projectile Velocity
            0.0f,                                    // Alt Projectile Velocity
            75.0f,                                   // Range
            75.0f,                                   // Alt Range
            1,                                       // Shots per fire
            1,                                       // Alt Shots per fire
            1,                                       // Strength or Magic required
            0,                                       // Two-handed Strength or Magic required
            0,                                       // Damage Radius (projectiles)
            0,                                       // Alt Damage Radius (projectiles)
            DFALSE,                                  // Alt fire zoom
            DFALSE,                                  // Semi-auto
            "Sounds\\Enemies\\Gideon\\windfire.wav", // Fire sound
            "Sounds\\Enemies\\Gideon\\windfire.wav", // Alt Fire sound
            "Sounds\\Enemies\\Gideon\\windfire.wav", // Empty weapon sound
            "Sounds\\Enemies\\Gideon\\windfire.wav", // Alt Empty weapon sound
            "CBehemothShockwaveProjectile",          // Projectile class
            "CBehemothShockwaveProjectile",          // Alt Projectile class
            0,                                       // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                      // Muzzle Flash color
            "Gideon_Shield",                         // Name
            0,                                       // Name ID (resource)
            "",                                      // flash sprite name
            "",                                      // alt flash Sprite name
            0.25f,                                   // Flash duration
            0.10f,                                   // Flash scale
            {0.0f, 0.0f, 0.0f},                      // 3rd person gun offset
            {0.0f, -1.4f, 0.3f},                     // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},                      // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                   // Recoil
            {0.0f, 0.0f, 0.1f},                      // Flash position
            0.625f,                                  // Eject interval
            0.0f,                                    // View kick
            DFALSE,                                  // View kick is Cumulative
            DFALSE,                                  // Animation looping
            DFALSE,                                  // Alt anim looping
            nullptr,                                 // Icon for status bar
            nullptr,                                 // Highlighted icon for status bar
        },
    };
}
const WeaponData* weapon_definition(DBYTE type)
{
    for (const auto& value : definitions)
        if (value.m_nType == type)
            return &value;
    return nullptr;
}
