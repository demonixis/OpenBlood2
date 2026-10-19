#include "WeaponDefs.h"
namespace
{
    constexpr WeaponData definitions[] = {
        // Bug Spray Canister
        {
            "Models\\Weapons\\BugSpray_pv.abc",              // Model
            "Models\\Weapons\\BugSpray_pv.abc",              // Model
            "Skins\\Weapons\\C_BugSpray_pv_t.dtx",           // Skin
            "Models\\Powerups\\BugSpray_pu.abc",             // 3rd person model
            "Skins\\Powerups\\BugSpray_pu.dtx",              // 3rd person skin
            WEAP_BUGSPRAY,                                   // Type
            TYPE_AUTORIFLE,                                  // Fire type
            AMMO_DIEBUGDIE,                                  // Ammo type
            1,                                               // Ammo Use
            1,                                               // Alt Ammo Use
            40.0f,                                           // Min Damage
            60.0f,                                           // Max Damage
            60.0f,                                           // Min Alt damage
            80.0f,                                           // Max Alt damage
            0.25f,                                           // Reload time
            1.0f,                                            // Alt reload time
            {0.0f, 0.0f},                                    // Spread
            {0.0f, 0.0f},                                    // Alt Spread
            1500.0f,                                         // Projectile Velocity
            1500.0f,                                         // Alt Projectile Velocity
            500.0f,                                          // Range
            500.0f,                                          // Alt Range
            1,                                               // Shots per fire
            1,                                               // Alt Shots per fire
            2,                                               // Strength or Magic required
            0,                                               // Two-handed Strength or Magic required
            0,                                               // Damage Radius (projectiles)
            0,                                               // Alt Damage Radius (projectiles)
            DFALSE,                                          // Alt fire zoom
            DFALSE,                                          // Semi-auto
            "Sounds\\Weapons\\bugbuster\\fire.wav",          // Fire sound
            "Sounds\\Weapons\\bugbuster\\alt.wav",           // Alt Fire sound
            "",                                              // Empty weapon sound
            "",                                              // Alt Empty weapon sound
            "CBugSprayProjectile",                           // Projectile class
            "CBugSprayAltProjectile",                        // Alt Projectile class
            0,                                               // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                              // Muzzle Flash color
            "Bug Spray Canister",                            // Name
            IDS_WEAPON_BUGSPRAY,                             // Name ID (resource)
            "",                                              // flash sprite name
            "",                                              // alt flash Sprite name
            0.25f,                                           // Flash duration
            0.10f,                                           // Flash scale
            {0.0f, 0.0f, 0.0f},                              // 3rd person gun offset
            {1.5f, -2.1f, 4.0f},                             // Gun Offset
            {0.0f, 0.275f, 1.0f},                            // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                           // Recoil
            {4.5f, -3.5f, 20.0f},                            // Flash position
            0.625f,                                          // Eject interval
            0.0f,                                            // View kick
            DFALSE,                                          // View kick is Cumulative
            DFALSE,                                          // Animation looping
            DTRUE,                                           // Alt anim looping
            "interface\\statusbar\\weapons\\bugspray.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\bugspray_h.pcx", // Highlighted icon for status bar
        },

        // Voodoo doll
        {
            "Models\\Weapons\\Voodoo_pv.abc",              // Model
            "Models\\Weapons\\Voodoo_pv.abc",              // Model
            "Skins\\Weapons\\C_Voodoo_pv_t.dtx",           // Skin
            "Models\\Powerups\\Voodoo_pu.abc",             // 3rd person model
            "Skins\\Powerups\\Voodoo_pu.dtx",              // 3rd person skin
            WEAP_VOODOO,                                   // Type
            TYPE_MELEE,                                    // Fire type
            AMMO_FOCUS,                                    // Ammo type
            5,                                             // Ammo Use
            100,                                           // Alt Ammo Use
            10.0f,                                         // Min Damage
            20.0f,                                         // Max Damage
            40.0f,                                         // Min Alt damage
            60.0f,                                         // Max Alt damage
            0.33f,                                         // Reload time
            1.0f,                                          // Alt reload time
            {0.0f, 0.0f},                                  // Spread
            {0.0f, 0.0f},                                  // Alt Spread
            0.0f,                                          // Projectile Velocity
            0.0f,                                          // Alt Projectile Velocity
            3500.0f,                                       // Range
            3500.0f,                                       // Alt Range
            1,                                             // Shots per fire
            1,                                             // Alt Shots per fire
            1,                                             // Strength or Magic required
            0,                                             // Two-handed Strength or Magic required
            0,                                             // Damage Radius (projectiles)
            0,                                             // Alt Damage Radius (projectiles)
            DFALSE,                                        // Alt fire zoom
            DFALSE,                                        // Semi-auto
            "Sounds\\Weapons\\voodoo\\fire.wav",           // Fire sound
            "Sounds\\Weapons\\voodoo\\alt.wav",            // Alt Fire sound
            "",                                            // Empty weapon sound
            "",                                            // Alt Empty weapon sound
            NULL,                                          // Projectile class
            NULL,                                          // Alt Projectile class
            0,                                             // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                            // Muzzle Flash color
            "Voodoo Doll",                                 // Name
            IDS_WEAPON_VOODOO,                             // Name ID (resource)
            "",                                            // flash sprite name
            "",                                            // alt flash Sprite name
            0.0f,                                          // Flash duration
            0.0f,                                          // Flash scale
            {0.0f, 0.0f, 0.0f},                            // 3rd person gun offset
            {-0.2f, -0.2f, 1.8f},                          // Gun Offset
            {0.0f, 0.275f, 1.0f},                          // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                         // Recoil
            {0.0f, 0.0f, 0.1f},                            // Flash position
            0.625f,                                        // Eject interval
            0.0f,                                          // View kick
            DFALSE,                                        // View kick is Cumulative
            DFALSE,                                        // Animation looping
            DFALSE,                                        // Alt anim looping
            "interface\\statusbar\\weapons\\voodoo.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\voodoo_h.pcx", // Highlighted icon for status bar
        },

        // RemoteBomb
        {
            "Models\\Weapons\\C4remote_pv.abc",    // Model
            "Models\\Weapons\\C4remote_pv.abc",    // Model
            "Skins\\Weapons\\C_C4remote_pv_t.dtx", // Skin
            "Models\\Powerups\\remotes_pu.abc",    // 3rd person model
            "Skins\\Powerups\\remotes_pu.dtx",     // 3rd person skin
            WEAP_REMOTEBOMB,                       // Type
            TYPE_MELEE,                            // Fire type
            AMMO_REMOTEBOMB,                       // Ammo type
            1,                                     // Ammo Use
            1,                                     // Alt Ammo Use
            80.0f,                                 // Min Damage
            120.0f,                                // Max Damage
            80.0f,                                 // Min Alt damage
            120.0f,                                // Max Alt damage
            1.0f,                                  // Reload time
            1.0f,                                  // Alt reload time
            {0.0f, 0.0f},                          // Spread
            {0.0f, 0.0f},                          // Alt Spread
            1000.0f,                               // Projectile Velocity
            0.0f,                                  // Alt Projectile Velocity
            2000.0f,                               // Range
            0.0f,                                  // Alt Range
            1,                                     // Shots per fire
            1,                                     // Alt Shots per fire
            1,                                     // Strength or Magic required
            0,                                     // Two-handed Strength or Magic required
            250,                                   // Damage Radius (projectiles)
            250,                                   // Alt Damage Radius (projectiles)
            DFALSE,                                // Alt fire zoom
            DFALSE,                                // Semi-auto
            "Sounds\\Weapons\\remotes\\fire.wav",  // Fire sound
            "Sounds\\Weapons\\remotes\\alt.wav",   // Alt Fire sound
            "",                                    // Empty weapon sound
            "",                                    // Alt Empty weapon sound
            "CRemoteBomb",                         // Projectile class
            "CRemoteBomb",                         // Alt Projectile class
            0,                                     // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                    // Muzzle Flash color
            "Remote Bomb",                         // Name
            369,                                   // Muzzle Flash radius
            "",                                    // flash sprite name
            "",                                    // alt flash Sprite name
            0.25f,                                 // Flash duration
            0.0f,                                  // Flash scale
            {0.0f, 0.0f, 0.0f},                    // 3rd person gun offset
            {-0.7f, 0.0f, 2.4f},                   // Gun Offset
            {0.0f, 0.375f, 12.0f},                 // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                 // Recoil
            {3.0f, -7.5f, 5.0f},                   // Flash position
            0.0f,                                  // Eject interval
            0.0f,                                  // View kick
            DFALSE,                                // View kick is Cumulative
            DTRUE,                                 // Animation looping
            DFALSE,                                // Alt anim looping
            nullptr,                               // Icon for status bar
            nullptr,                               // Highlighted icon for status bar
        },

        // ProximityBomb
        {
            "Models\\Weapons\\C4proximity_pv.abc",    // Model
            "Models\\Weapons\\C4proximity_pv.abc",    // Model
            "Skins\\Weapons\\C_C4prox_pv_t.dtx",      // Skin
            "Models\\Powerups\\proximities_pu.abc",   // 3rd person model
            "Skins\\Powerups\\proximities_pu.dtx",    // 3rd person skin
            WEAP_PROXIMITYBOMB,                       // Type
            TYPE_MELEE,                               // Fire type
            AMMO_PROXIMITYBOMB,                       // Ammo type
            1,                                        // Ammo Use
            1,                                        // Alt Ammo Use
            80.0f,                                    // Min Damage
            120.0f,                                   // Max Damage
            80.0f,                                    // Min Alt damage
            120.0f,                                   // Max Alt damage
            1.0f,                                     // Reload time
            1.0f,                                     // Alt reload time
            {0.0f, 0.0f},                             // Spread
            {0.0f, 0.0f},                             // Alt Spread
            1000.0f,                                  // Projectile Velocity
            0.0f,                                     // Alt Projectile Velocity
            2000.0f,                                  // Range
            0.0f,                                     // Alt Range
            1,                                        // Shots per fire
            1,                                        // Alt Shots per fire
            1,                                        // Strength or Magic required
            0,                                        // Two-handed Strength or Magic required
            250,                                      // Damage Radius (projectiles)
            250,                                      // Alt Damage Radius (projectiles)
            DFALSE,                                   // Alt fire zoom
            DFALSE,                                   // Semi-auto
            "Sounds\\Weapons\\proximities\\fire.wav", // Fire sound
            "Sounds\\Weapons\\proximities\\alt.wav",  // Alt Fire sound
            "",                                       // Empty weapon sound
            "",                                       // Alt Empty weapon sound
            "CProximityBomb",                         // Projectile class
            "CProximityBomb",                         // Alt Projectile class
            368,                                      // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                       // Muzzle Flash color
            "Proximity Bomb",                         // Name
            0,                                        // Name ID (resource)
            "",                                       // flash sprite name
            "",                                       // alt flash Sprite name
            0.25f,                                    // Flash duration
            0.0f,                                     // Flash scale
            {0.0f, 0.0f, 0.0f},                       // 3rd person gun offset
            {-0.7f, 0.0f, 2.4f},                      // Gun Offset
            {0.0f, 0.375f, 12.0f},                    // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                    // Recoil
            {3.0f, -7.5f, 5.0f},                      // Flash position
            0.0f,                                     // Eject interval
            0.0f,                                     // View kick
            DFALSE,                                   // View kick is Cumulative
            DTRUE,                                    // Animation looping
            DFALSE,                                   // Alt anim looping
            nullptr,                                  // Icon for status bar
            nullptr,                                  // Highlighted icon for status bar
        },

        // M16 Assault Rifle
        {
            "Models\\Weapons\\M16_pv.abc",              // Model
            "Models\\Weapons\\lh_M16_pv.abc",           // Model
            "Skins\\Weapons\\C_M16_pv_t.dtx",           // Skin
            "Models\\Powerups\\M16_pu.abc",             // 3rd person model
            "Skins\\Powerups\\M16_pu.dtx",              // 3rd person skin
            WEAP_ASSAULTRIFLE,                          // Type
            TYPE_AUTORIFLE,                             // Fire type
            AMMO_BULLET,                                // Ammo type
            1,                                          // Ammo Use
            5,                                          // Alt Ammo Use
            10.0f,                                      // Min Damage
            16.0f,                                      // Max Damage
            100.0f,                                     // Min Alt damage
            140.0f,                                     // Max Alt damage
            0.1f,                                       // Reload time
            0.0f,                                       // Alt reload time
            {75.0f, 25.0f},                             // Spread
            {50.0f, 50.0f},                             // Alt Spread
            0.0f,                                       // Projectile Velocity
            1500.0f,                                    // Alt Projectile Velocity
            3500.0f,                                    // Range
            400.0f,                                     // Alt Range
            1,                                          // Shots per fire
            1,                                          // Alt Shots per fire
            3,                                          // Strength or Magic required
            0,                                          // Two-handed Strength or Magic required
            0,                                          // Damage Radius (projectiles)
            200,                                        // Alt Damage Radius (projectiles)
            DFALSE,                                     // Alt fire zoom
            DFALSE,                                     // Semi-auto
            "Sounds\\Weapons\\assault\\fire.wav",       // Fire sound
            "Sounds\\Weapons\\assault\\alt.wav",        // Alt Fire sound
            "",                                         // Empty weapon sound
            "",                                         // Alt Empty weapon sound
            NULL,                                       // Projectile class
            "CGrenade",                                 // Alt Projectile class
            100,                                        // Muzzle Flash radius
            {1.0f, 0.68f, 0.35f},                       // Muzzle Flash color
            "M16 Assault Rifle",                        // Name
            IDS_WEAPON_M16,                             // Name ID (resource)
            "Sprites\\m16flare.spr",                    // flash sprite name
            "Sprites\\m16altflare.spr",                 // alt flash Sprite name
            0.1f,                                       // Flash duration
            0.125f,                                     // Flash scale
            {0.0f, 0.0f, 0.0f},                         // 3rd person gun offset
            {0.7f, -0.9f, 1.7f},                        // Gun Offset
            {0.0f, 0.275f, 3.0f},                       // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                      // Recoil
            {5.25f, -5.8f, 30.0f},                      // Flash position
            0.625f,                                     // Eject interval
            0.5f,                                       // View kick
            DFALSE,                                     // View kick is Cumulative
            DTRUE,                                      // Animation looping
            DFALSE,                                     // Alt anim looping
            "interface\\statusbar\\weapons\\m16.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\m16_h.pcx", // Highlighted icon for status bar
        },

        // Original Flare Gun (complete definition).
        {
            "Models\\Weapons\\Flarepistol_pv.abc",           // Model
            "Models\\Weapons\\lh_Flarepistol_pv.abc",        // Model
            "Skins\\Weapons\\C_Flare_pv_t.dtx",              // Skin
            "Models\\Powerups\\Flarepistol_pu.abc",          // 3rd person model
            "Skins\\Powerups\\Flarepistol_pu.dtx",           // 3rd person skin
            WEAP_FLAREGUN,                                   // Type
            TYPE_PISTOL,                                     // Fire type
            AMMO_FLARE,                                      // Ammo type
            1,                                               // Ammo Use
            8,                                               // Alt Ammo Use
            10.0f,                                           // Min Damage
            15.0f,                                           // Max Damage
            20.0f,                                           // Min Alt damage
            30.0f,                                           // Max Alt damage
            0.75f,                                           // Reload time
            1.5f,                                            // Alt reload time
            {0.0f, 0.0f},                                    // Spread
            {0.0f, 0.0f},                                    // Alt Spread
            2000.0f,                                         // Projectile Velocity
            2000.0f,                                         // Alt Projectile Velocity
            5000.0f,                                         // Range
            5000.0f,                                         // Alt Range
            1,                                               // Shots per fire
            1,                                               // Alt Shots per fire
            2,                                               // Strength or Magic required
            3,                                               // Two-handed Strength or Magic required
            100,                                             // Damage Radius (projectiles)
            100,                                             // Alt Damage Radius (projectiles)
            DFALSE,                                          // Alt fire zoom
            DFALSE,                                          // Semi-auto
            "Sounds\\Weapons\\flare\\fire.wav",              // Fire sound
            "Sounds\\Weapons\\flare\\alt.wav",               // Alt Fire sound
            "",                                              // Empty weapon sound
            "",                                              // Alt Empty weapon sound
            "CFlareProjectile",                              // Projectile class
            "CFlareAltProjectile",                           // Alt Projectile class
            150,                                             // Muzzle Flash radius
            {1.0f, 0.68f, 0.35f},                            // Muzzle Flash color
            "Flare Gun",                                     // Name
            IDS_WEAPON_FLAREGUN,                             // Name ID (resource)
            "Sprites\\FlaregunFlare.spr",                    // flash sprite name
            "Sprites\\FlaregunFlare.spr",                    // alt flash Sprite name
            0.05f,                                           // Flash duration
            0.175f,                                          // Flash scale
            {0.0f, 0.0f, 0.0f},                              // 3rd person gun offset
            {0.8f, -0.7f, 1.6f},                             // Gun Offset
            {0.0f, 0.275f, 1.0f},                            // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                           // Recoil
            {6.5f, -4.5f, 28.0f},                            // Flash position
            0.625f,                                          // Eject interval
            0.0f,                                            // View kick
            DFALSE,                                          // View kick is Cumulative
            DFALSE,                                          // Animation looping
            DFALSE,                                          // Alt anim looping
            "interface\\statusbar\\weapons\\flaregun.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\flaregun_h.pcx", // Highlighted icon for status bar
        },
        // Shotgun
        {
            "Models\\Weapons\\Shotgun_pv.abc",              // Model
            "Models\\Weapons\\Shotgun_pv.abc",              // Model
            "Skins\\Weapons\\C_Shot_pv_t.dtx",              // Skin
            "Models\\Powerups\\Shotgun_pu.abc",             // 3rd person model
            "Skins\\Powerups\\Shotgun_pu.dtx",              // 3rd person skin
            WEAP_SHOTGUN,                                   // Type
            TYPE_PISTOL,                                    // Fire type
            AMMO_SHELL,                                     // Ammo type
            1,                                              // Ammo Use
            2,                                              // Alt Ammo Use
            4.0f,                                           // Min Damage
            6.0f,                                           // Max Damage
            8.0f,                                           // Min Alt damage
            12.0f,                                          // Max Alt damage
            0.5f,                                           // Reload time
            1.0f,                                           // Alt reload time
            {200.0f, 100.0f},                               // Spread
            {200.0f, 100.0f},                               // Alt Spread
            0.0f,                                           // Projectile Velocity
            0.0f,                                           // Alt Projectile Velocity
            3500.0f,                                        // Range
            3500.0f,                                        // Alt Range
            16,                                             // Shots per fire
            16,                                             // Alt Shots per fire
            3,                                              // Strength or Magic required
            4,                                              // Two-handed Strength or Magic required
            0,                                              // Damage Radius (projectiles)
            0,                                              // Alt Damage Radius (projectiles)
            DFALSE,                                         // Alt fire zoom
            DFALSE,                                         // Semi-auto
            "Sounds\\Weapons\\shotgun\\fire.wav",           // Fire sound
            "Sounds\\Weapons\\shotgun\\alt.wav",            // Alt Fire sound
            "",                                             // Empty weapon sound
            "",                                             // Alt Empty weapon sound
            NULL,                                           // Projectile class
            NULL,                                           // Alt Projectile class
            125,                                            // Muzzle Flash radius
            {1.0f, 0.68f, 0.35f},                           // Muzzle Flash color
            "Sawed-off Shotgun",                            // Name
            IDS_WEAPON_SHOTGUN,                             // Name ID (resource)
            "Sprites\\shotgun1.spr",                        // flash sprite name
            "Sprites\\shotgun2.spr",                        // alt flash Sprite name
            0.05f,                                          // Flash duration
            0.19f,                                          // Flash scale
            {0.0f, 0.0f, 0.0f},                             // 3rd person gun offset
            {0.5f, -0.5f, 0.9f},                            // Gun Offset
            {0.08f, 0.5f, 2.0f},                            // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                          // Recoil
            {5.0f, -5.0f, 30.0f},                           // Flash position
            0.625f,                                         // Eject interval
            2.5f,                                           // View kick
            DFALSE,                                         // View kick is Cumulative
            DFALSE,                                         // Animation looping
            DFALSE,                                         // Alt anim looping
            "interface\\statusbar\\weapons\\shotgun.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\shotgun_h.pcx", // Highlighted icon for status bar
        },
        // Singularity
        {
            "Models\\Weapons\\Singularity_pv.abc",              // Model
            "Models\\Weapons\\Singularity_pv.abc",              // Model
            "Skins\\Weapons\\C_Single_pv_t.dtx",                // Skin
            "Models\\Powerups\\Singularity_pu.abc",             // 3rd person model
            "Skins\\Powerups\\Singularity_pu.dtx",              // 3rd person skin
            WEAP_SINGULARITY,                                   // Type
            TYPE_RIFLE,                                         // Fire type
            AMMO_BATTERY,                                       // Ammo type
            50,                                                 // Ammo Use
            50,                                                 // Alt Ammo Use
            5.0f,                                               // Min Damage
            10.0f,                                              // Max Damage
            5.0f,                                               // Min Alt damage
            10.0f,                                              // Max Alt damage
            5.0f,                                               // Reload time
            5.0f,                                               // Alt reload time
            {0.0f, 0.0f},                                       // Spread
            {0.0f, 0.0f},                                       // Alt Spread
            0.0f,                                               // Projectile Velocity
            0.0f,                                               // Alt Projectile Velocity
            3500.0f,                                            // Range
            3500.0f,                                            // Alt Range
            1,                                                  // Shots per fire
            1,                                                  // Alt Shots per fire
            5,                                                  // Strength or Magic required
            0,                                                  // Two-handed Strength or Magic required
            500,                                                // Damage Radius (projectiles)
            500,                                                // Alt Damage Radius (projectiles)
            DFALSE,                                             // Alt fire zoom
            DFALSE,                                             // Semi-auto
            "Sounds\\Weapons\\singularity\\fire.wav",           // Fire sound
            "Sounds\\Weapons\\singularity\\fire.wav",           // Alt Fire sound
            "",                                                 // Empty weapon sound
            "",                                                 // Alt Empty weapon sound
            "CSingularityProjectile",                           // Projectile class
            "CSingularityProjectile",                           // Alt Projectile class
            0,                                                  // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                                 // Muzzle Flash color
            "Singularity Cannon",                               // Name
            IDS_WEAPON_SINGULARITY,                             // Name ID (resource)
            "",                                                 // flash sprite name
            "",                                                 // alt flash Sprite name
            0.10f,                                              // Flash duration
            0.13f,                                              // Flash scale
            {0.0f, 0.0f, 0.0f},                                 // 3rd person gun offset
            {1.4f, -1.9f, 3.4f},                                // Gun Offset
            {0.0f, 0.0f, 0.0f},                                 // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                              // Recoil
            {4.25f, -4.25f, 25.0f},                             // Flash position
            0.625f,                                             // Eject interval
            0.2f,                                               // View kick
            DFALSE,                                             // View kick is Cumulative
            DFALSE,                                             // Animation looping
            DFALSE,                                             // Alt anim looping
            "interface\\statusbar\\weapons\\singularity.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\singularity_h.pcx", // Highlighted icon for status bar
        },
        // Tesla Cannon
        {
            "Models\\Weapons\\TeslaRod_pv.abc",          // Model
            "Models\\Weapons\\lh_TeslaRod_pv.abc",       // Model
            "Skins\\Weapons\\C_Tesla_pv_t.dtx",          // Skin
            "Models\\Powerups\\TeslaCannon_pu.abc",      // 3rd person model
            "Skins\\Powerups\\TeslaCannon_pu.dtx",       // 3rd person skin
            WEAP_TESLACANNON,                              // Type
            TYPE_AUTORIFLE,                                // Fire type
            AMMO_BATTERY,                                  // Ammo type
            2,                                             // Ammo Use
            25,                                            // Alt Ammo Use
            30.0f,                                         // Min Damage
            50.0f,                                         // Max Damage
            100.0f,                                        // Min Alt damage
            150.0f,                                        // Max Alt damage
            0.175f,                                        // Reload time
            1.0f,                                          // Alt reload time
            {20.0f, 20.0f},                                // Spread
            {0.0f, 0.0f},                                  // Alt Spread
            1000.0f,                                       // Projectile Velocity
            1000.0f,                                       // Alt Projectile Velocity
            3500.0f,                                       // Range
            3500.0f,                                       // Alt Range
            1,                                             // Shots per fire
            1,                                             // Alt Shots per fire
            4,                                             // Strength or Magic required
            0,                                             // Two-handed Strength or Magic required
            100,                                           // Damage Radius (projectiles)
            150,                                           // Alt Damage Radius (projectiles)
            DFALSE,                                        // Alt fire zoom
            DFALSE,                                        // Semi-auto
            "Sounds\\Weapons\\Tesla\\fire.wav",          // Fire sound
            "Sounds\\Weapons\\Tesla\\alt.wav",           // Alt Fire sound
            "",                                            // Empty weapon sound
            "",                                            // Alt Empty weapon sound
            "CTeslaProjectile",                            // Projectile class
            "CTeslaBallProjectile",                        // Alt Projectile class (CTeslaCannon::Fire overrides)
            150,                                           // Muzzle Flash radius
            {0.2f, 0.2f, 1.0f},                            // Muzzle Flash color
            "Tesla Cannon",                                // Name
            IDS_WEAPON_TESLA,                              // Name ID (resource)
            "Sprites\\teslamuzzle.spr",                    // flash sprite name
            "Sprites\\teslaflash.spr",                     // alt flash Sprite name
            0.25f,                                         // Flash duration
            0.1f,                                          // Flash scale
            {0.0f, 0.0f, 0.0f},                            // 3rd person gun offset
            {0.5f, -0.9f, 1.4f},                           // Gun Offset
            {0.15f, 0.3f, 5.0f},                           // Muzzle Offset
            {0.03f, 0.03f, 0.15f},                         // Recoil
            {5.0f, -5.0f, 30.0f},                          // Flash position
            0.625f,                                        // Eject interval
            0.0f,                                          // View kick
            DFALSE,                                        // View kick is Cumulative
            DTRUE,                                         // Animation looping
            DFALSE,                                        // Alt anim looping
            "interface\\statusbar\\weapons\\tesla.pcx",   // Icon for status bar
            "interface\\statusbar\\weapons\\tesla_h.pcx", // Highlighted icon for status bar
        },
        // Shikari Hand Claw
        {
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Skins\\Weapons\\knifec_pv_t.dtx", // Skin
            DNULL,        // Model
            DNULL,        // Skin
            WEAP_SHIKARI_CLAW,    // Type
            TYPE_MELEE,    // Fire type
            AMMO_NONE,       // Ammo type
            1,      // Ammo Use
            1,      // Alt Ammo Use
            30.0f,     // Min Damage
            60.0f,     // Max Damage
            30.0f,     // Min Alt damage
            60.0f,     // Max Alt damage
            1.00f,     // Reload time
            0.0f,     // Alt reload time
            {0.0f, 0.0f},   // Spread
            {0.0f, 0.0f},   // Alt Spread
            0.0f,     // Projectile Velocity
            0.0f,     // Alt Projectile Velocity
            100.0f,        // Range
            100.0f,        // Alt Range
            1,      // Shots per fire
            1,      // Alt Shots per fire
            1,      // Strength or Magic required
            0,      // Two-handed Strength or Magic required
            0,      // Damage Radius (projectiles)
            0,      // Alt Damage Radius (projectiles)
            DFALSE,     // Alt fire zoom
            DFALSE,     // Semi-auto
            "Sounds\\Weapons\\Knife\\fire.wav", // Fire sound
            "Sounds\\Weapons\\Knife\\alt.wav", // Alt Fire sound
            "", // Empty weapon sound
            "", // Alt Empty weapon sound
            NULL,     // Projectile class
            NULL,     // Alt Projectile class
            0,      // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},  // Muzzle Flash color
            "Shikari_Claw",   // Name
            0,      // Name ID (resource)
            "",      // flash sprite name
            "",      // alt flash Sprite name
            0.25f,     // Flash duration
            0.10f,     // Flash scale
            {0.0f, 0.0f, 0.0f},  // 3rd person gun offset
            {0.0f, -1.4f, 0.3f}, // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},  // Muzzle Offset
            {0.03f, 0.03f, 0.15f}, // Recoil
            {0.0f, 0.0f, 0.1f},  // Flash position
            0.625f,     // Eject interval
            0.0f,     // View kick
            DFALSE,     // View kick is Cumulative
            DFALSE,     // Animation looping
            DFALSE,     // Alt anim looping
            DNULL,     // Icon for status bar
            DNULL,     // Highlighted icon for status bar
        },
        // Shikari Spit
        {
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Skins\\Weapons\\knifec_pv_t.dtx", // Skin
            DNULL,        // Model
            DNULL,        // Skin
            WEAP_SHIKARI_SPIT,   // Type
            TYPE_RIFLE,    // Fire type
            AMMO_NONE,       // Ammo type
            1,      // Ammo Use
            1,      // Alt Ammo Use
            30.0f,     // Min Damage
            50.0f,     // Max Damage
            30.0f,     // Min Alt damage
            50.0f,     // Max Alt damage
            1.00f,     // Reload time
            0.0f,     // Alt reload time
            {0.0f, 0.0f},   // Spread
            {0.0f, 0.0f},   // Alt Spread
            1000.0f,     // Projectile Velocity
            1000.0f,     // Alt Projectile Velocity
            75.0f,        // Range
            75.0f,        // Alt Range
            1,      // Shots per fire
            1,      // Alt Shots per fire
            1,      // Strength or Magic required
            0,      // Two-handed Strength or Magic required
            0,      // Damage Radius (projectiles)
            0,      // Alt Damage Radius (projectiles)
            DFALSE,     // Alt fire zoom
            DFALSE,     // Semi-auto
            "Sounds\\Enemies\\Shikari\\sh_spit_3.wav", // Fire sound
            "Sounds\\Enemies\\Shikari\\sh_spit_3.wav", // Alt Fire sound
            "Sounds\\Enemies\\Shikari\\sh_spit_1.wav", // Empty Fire sound
            "Sounds\\Enemies\\Shikari\\sh_spit_1.wav", // Alt EmptyFire sound
            "CShikariLoogieProjectile", // Projectile class
            "CShikariLoogieProjectile", // Alt Projectile class
            0,      // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},  // Muzzle Flash color
            "Shikari_Spit",   // Name
            0,      // Name ID (resource)
            "",      // flash sprite name
            "",      // alt flash Sprite name
            0.25f,     // Flash duration
            0.10f,     // Flash scale
            {0.0f, 0.0f, 0.0f},  // 3rd person gun offset
            {0.0f, -1.4f, 0.3f}, // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},  // Muzzle Offset
            {0.03f, 0.03f, 0.15f}, // Recoil
            {0.0f, 0.0f, 0.1f},  // Flash position
            0.625f,     // Eject interval
            0.0f,     // View kick
            DFALSE,     // View kick is Cumulative
            DFALSE,     // Animation looping
            DFALSE,     // Alt anim looping
            DNULL,     // Icon for status bar
            DNULL,     // Highlighted icon for status bar
        },
        // SoulDrudge Crowbar
        {
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Skins\\Weapons\\knifec_pv_t.dtx", // Skin
            "Models\\Powerups\\crowbar_pu.abc", // Model
            "Skins\\Powerups\\crowbar_pu.dtx", // Skin
            WEAP_SOUL_CROWBAR,  // Type
            TYPE_MELEE,    // Fire type
            AMMO_NONE,       // Ammo type
            1,      // Ammo Use
            1,      // Alt Ammo Use
            10.0f,     // Min Damage
            20.0f,     // Max Damage
            10.0f,     // Min Alt damage
            20.0f,     // Max Alt damage
            1.00f,     // Reload time
            0.0f,     // Alt reload time
            {0.0f, 0.0f},   // Spread
            {0.0f, 0.0f},   // Alt Spread
            0.0f,     // Projectile Velocity
            0.0f,     // Alt Projectile Velocity
            75.0f,        // Range
            75.0f,        // Alt Range
            1,      // Shots per fire
            1,      // Alt Shots per fire
            1,      // Strength or Magic required
            0,      // Two-handed Strength or Magic required
            0,      // Damage Radius (projectiles)
            0,      // Alt Damage Radius (projectiles)
            DFALSE,     // Alt fire zoom
            DFALSE,     // Semi-auto
            "Sounds\\Weapons\\Knife\\fire.wav", // Fire sound
            "Sounds\\Weapons\\Knife\\alt.wav", // Alt Fire sound
            "", // Empty weapon sound
            "", // Alt Empty weapon sound
            NULL,     // Projectile class
            NULL,     // Alt Projectile class
            0,      // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},  // Muzzle Flash color
            "Soul_Crowbar",   // Name
            0,      // Name ID (resource)
            "",      // flash sprite name
            "",      // alt flash Sprite name
            0.25f,     // Flash duration
            0.10f,     // Flash scale
            {0.0f, 0.0f, 0.0f},  // 3rd person gun offset
            {0.0f, -1.4f, 0.3f}, // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},  // Muzzle Offset
            {0.03f, 0.03f, 0.15f}, // Recoil
            {0.0f, 0.0f, 0.1f},  // Flash position
            0.625f,     // Eject interval
            0.0f,     // View kick
            DFALSE,     // View kick is Cumulative
            DFALSE,     // Animation looping
            DFALSE,     // Alt anim looping
            DNULL,     // Icon for status bar
            DNULL,     // Highlighted icon for status bar
        },
        // SoulDrudge Axe
        {
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Skins\\Weapons\\knifec_pv_t.dtx", // Skin
            "Models\\Powerups\\fireaxe_pu.abc", // Model
            "Skins\\Powerups\\fireaxe_pu.dtx", // Skin
            WEAP_SOUL_AXE,  // Type
            TYPE_MELEE,    // Fire type
            AMMO_NONE,       // Ammo type
            1,      // Ammo Use
            1,      // Alt Ammo Use
            20.0f,     // Min Damage
            30.0f,     // Max Damage
            20.0f,     // Min Alt damage
            30.0f,     // Max Alt damage
            1.00f,     // Reload time
            0.0f,     // Alt reload time
            {0.0f, 0.0f},   // Spread
            {0.0f, 0.0f},   // Alt Spread
            0.0f,     // Projectile Velocity
            0.0f,     // Alt Projectile Velocity
            75.0f,        // Range
            75.0f,        // Alt Range
            1,      // Shots per fire
            1,      // Alt Shots per fire
            1,      // Strength or Magic required
            0,      // Two-handed Strength or Magic required
            0,      // Damage Radius (projectiles)
            0,      // Alt Damage Radius (projectiles)
            DFALSE,     // Alt fire zoom
            DFALSE,     // Semi-auto
            "Sounds\\Weapons\\Knife\\fire.wav", // Fire sound
            "Sounds\\Weapons\\Knife\\alt.wav", // Alt Fire sound
            "", // Empty weapon sound
            "", // Alt Empty weapon sound
            NULL,     // Projectile class
            NULL,     // Alt Projectile class
            0,      // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},  // Muzzle Flash color
            "Soul_Axe",   // Name
            0,      // Name ID (resource)
            "",      // flash sprite name
            "",      // alt flash Sprite name
            0.25f,     // Flash duration
            0.10f,     // Flash scale
            {0.0f, 0.0f, 0.0f},  // 3rd person gun offset
            {0.0f, -1.4f, 0.3f}, // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},  // Muzzle Offset
            {0.03f, 0.03f, 0.15f}, // Recoil
            {0.0f, 0.0f, 0.1f},  // Flash position
            0.625f,     // Eject interval
            0.0f,     // View kick
            DFALSE,     // View kick is Cumulative
            DFALSE,     // Animation looping
            DFALSE,     // Alt anim looping
            DNULL,     // Icon for status bar
            DNULL,     // Highlighted icon for status bar
        },
        // SoulDrudge Pipe
        {
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Models\\Weapons\\knifec_pv.abc", // Model
            "Skins\\Weapons\\knifec_pv_t.dtx", // Skin
            "Models\\Powerups\\pipe_pu.abc", // Model
            "Skins\\Powerups\\pipe_pu.dtx", // Skin
            WEAP_SOUL_PIPE,  // Type
            TYPE_MELEE,    // Fire type
            AMMO_NONE,       // Ammo type
            1,      // Ammo Use
            1,      // Alt Ammo Use
            10.0f,     // Min Damage
            20.0f,     // Max Damage
            10.0f,     // Min Alt damage
            20.0f,     // Max Alt damage
            1.00f,     // Reload time
            0.0f,     // Alt reload time
            {0.0f, 0.0f},   // Spread
            {0.0f, 0.0f},   // Alt Spread
            0.0f,     // Projectile Velocity
            0.0f,     // Alt Projectile Velocity
            75.0f,        // Range
            75.0f,        // Alt Range
            1,      // Shots per fire
            1,      // Alt Shots per fire
            1,      // Strength or Magic required
            0,      // Two-handed Strength or Magic required
            0,      // Damage Radius (projectiles)
            0,      // Alt Damage Radius (projectiles)
            DFALSE,     // Alt fire zoom
            DFALSE,     // Semi-auto
            "Sounds\\Weapons\\Knife\\fire.wav", // Fire sound
            "Sounds\\Weapons\\Knife\\alt.wav", // Alt Fire sound
            "", // Empty weapon sound
            "", // Alt Empty weapon sound
            NULL,     // Projectile class
            NULL,     // Alt Projectile class
            0,      // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},  // Muzzle Flash color
            "Soul_Pipe",   // Name
            0,      // Name ID (resource)
            "",      // flash sprite name
            "",      // alt flash Sprite name
            0.25f,     // Flash duration
            0.10f,     // Flash scale
            {0.0f, 0.0f, 0.0f},  // 3rd person gun offset
            {0.0f, -1.4f, 0.3f}, // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},  // Muzzle Offset
            {0.03f, 0.03f, 0.15f}, // Recoil
            {0.0f, 0.0f, 0.1f},  // Flash position
            0.625f,     // Eject interval
            0.0f,     // View kick
            DFALSE,     // View kick is Cumulative
            DFALSE,     // Animation looping
            DFALSE,     // Alt anim looping
            DNULL,     // Icon for status bar
            DNULL,     // Highlighted icon for status bar
        },
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

        // DrudgeLord Hook
        {
            "Models\\Weapons\\knifec_pv.abc",   // Model
            "Models\\Weapons\\knifec_pv.abc",   // Model
            "Skins\\Weapons\\knifec_pv_t.dtx",  // Skin
            nullptr,                            // Model
            nullptr,                            // Skin
            WEAP_SOUL_HOOK,                     // Type
            TYPE_MELEE,                         // Fire type
            AMMO_NONE,                          // Ammo type
            1,                                  // Ammo Use
            1,                                  // Alt Ammo Use
            50.0f,                              // Min Damage
            80.0f,                              // Max Damage
            50.0f,                              // Min Alt damage
            80.0f,                              // Max Alt damage
            1.00f,                              // Reload time
            0.0f,                               // Alt reload time
            {0.0f, 0.0f},                       // Spread
            {0.0f, 0.0f},                       // Alt Spread
            0.0f,                               // Projectile Velocity
            0.0f,                               // Alt Projectile Velocity
            100.0f,                             // Range
            100.0f,                             // Alt Range
            1,                                  // Shots per fire
            1,                                  // Alt Shots per fire
            1,                                  // Strength or Magic required
            0,                                  // Two-handed Strength or Magic required
            0,                                  // Damage Radius (projectiles)
            0,                                  // Alt Damage Radius (projectiles)
            DFALSE,                             // Alt fire zoom
            DFALSE,                             // Semi-auto
            "Sounds\\Weapons\\Knife\\fire.wav", // Fire sound
            "Sounds\\Weapons\\Knife\\alt.wav",  // Alt Fire sound
            "",                                 // Empty weapon sound
            "",                                 // Alt Empty weapon sound
            nullptr,                            // Projectile class
            nullptr,                            // Alt Projectile class
            0,                                  // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                 // Muzzle Flash color
            "Soul_Hook",                        // Name
            0,                                  // Name ID (resource)
            "",                                 // flash sprite name
            "",                                 // alt flash Sprite name
            0.25f,                              // Flash duration
            0.10f,                              // Flash scale
            {0.0f, 0.0f, 0.0f},                 // 3rd person gun offset
            {0.0f, -1.4f, 0.3f},                // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},                 // Muzzle Offset
            {0.03f, 0.03f, 0.15f},              // Recoil
            {0.0f, 0.0f, 0.1f},                 // Flash position
            0.625f,                             // Eject interval
            0.0f,                               // View kick
            DFALSE,                             // View kick is Cumulative
            DFALSE,                             // Animation looping
            DFALSE,                             // Alt anim looping
            nullptr,                            // Icon for status bar
            nullptr,                            // Highlighted icon for status bar
        },
        // Drudge Fireball
        {
            "Models\\Weapons\\knifec_pv.abc",    // Model
            "Models\\Weapons\\knifec_pv.abc",    // Model
            "Skins\\Weapons\\knifec_pv_t.dtx",   // Skin
            nullptr,                             // Model
            nullptr,                             // Skin
            WEAP_DRUDGE_FIREBALL,                // Type
            TYPE_RIFLE,                          // Fire type
            AMMO_NONE,                           // Ammo type
            1,                                   // Ammo Use
            1,                                   // Alt Ammo Use
            40.0f,                               // Min Damage
            60.0f,                               // Max Damage
            40.0f,                               // Min Alt damage
            60.0f,                               // Max Alt damage
            0.50f,                               // Reload time
            0.50f,                               // Alt reload time
            {0.0f, 0.0f},                        // Spread
            {0.0f, 0.0f},                        // Alt Spread
            750.0f,                              // Projectile Velocity
            750.0f,                              // Alt Projectile Velocity
            1000.0f,                             // Range
            1000.0f,                             // Alt Range
            1,                                   // Shots per fire
            1,                                   // Alt Shots per fire
            1,                                   // Strength or Magic required
            0,                                   // Two-handed Strength or Magic required
            300,                                 // Damage Radius (projectiles)
            300,                                 // Alt Damage Radius (projectiles)
            DFALSE,                              // Alt fire zoom
            DFALSE,                              // Semi-auto
            "Sounds\\Weapons\\Napalm\\fire.wav", // Fire sound
            "Sounds\\Weapons\\Napalm\\fire.wav", // Alt Fire sound
            "",                                  // Empty weapon sound
            "",                                  // Alt Empty weapon sound
            "CNapalmProjectile",                 // Projectile class
            "CNapalmProjectile",                 // Alt Projectile class
            20,                                  // Muzzle Flash radius
            {0.8f, 0.3f, 0.0f},                  // Muzzle Flash color
            "Fireball",                          // Name
            0,                                   // Name ID (resource)
            "",                                  // flash sprite name
            "",                                  // alt flash Sprite name
            0.25f,                               // Flash duration
            0.40f,                               // Flash scale
            {0.0f, 0.0f, 0.0f},                  // 3rd person gun offset
            {0.0f, -1.4f, 0.3f},                 // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},                  // Muzzle Offset
            {0.03f, 0.03f, 0.15f},               // Recoil
            {0.0f, 0.0f, 0.1f},                  // Flash position
            0.625f,                              // Eject interval
            0.0f,                                // View kick
            DFALSE,                              // View kick is Cumulative
            DFALSE,                              // Animation looping
            DFALSE,                              // Alt anim looping
            nullptr,                             // Icon for status bar
            nullptr,                             // Highlighted icon for status bar
        },
        // Bone Leech Suck
        {
            "Models\\Weapons\\knifec_pv.abc",   // Model
            "Models\\Weapons\\knifec_pv.abc",   // Model
            "Skins\\Weapons\\knifec_pv_t.dtx",  // Skin
            nullptr,                            // Model
            nullptr,                            // Skin
            WEAP_BONELEECH_SUCK,                // Type
            TYPE_MELEE,                         // Fire type
            AMMO_NONE,                          // Ammo type
            0,                                  // Ammo Use
            0,                                  // Alt Ammo Use
            4.0f,                               // Min Damage
            8.0f,                               // Max Damage
            4.0f,                               // Min Alt damage
            8.0f,                               // Max Alt damage
            1.00f,                              // Reload time
            0.0f,                               // Alt reload time
            {0.0f, 0.0f},                       // Spread
            {0.0f, 0.0f},                       // Alt Spread
            0.0f,                               // Projectile Velocity
            0.0f,                               // Alt Projectile Velocity
            100.0f,                             // Range
            100.0f,                             // Alt Range
            1,                                  // Shots per fire
            1,                                  // Alt Shots per fire
            1,                                  // Strength or Magic required
            0,                                  // Two-handed Strength or Magic required
            0,                                  // Damage Radius (projectiles)
            0,                                  // Alt Damage Radius (projectiles)
            DFALSE,                             // Alt fire zoom
            DFALSE,                             // Semi-auto
            "Sounds\\Weapons\\Knife\\fire.wav", // Fire sound
            "Sounds\\Weapons\\Knife\\alt.wav",  // Alt Fire sound
            "",                                 // Empty weapon sound
            "",                                 // Alt Empty weapon sound
            nullptr,                            // Projectile class
            nullptr,                            // Alt Projectile class
            0,                                  // Muzzle Flash radius
            {0.0f, 0.0f, 0.0f},                 // Muzzle Flash color
            "Boneleech_Suck",                   // Name
            0,                                  // Name ID (resource)
            "",                                 // flash sprite name
            "",                                 // alt flash Sprite name
            0.25f,                              // Flash duration
            0.10f,                              // Flash scale
            {0.0f, 0.0f, 0.0f},                 // 3rd person gun offset
            {0.0f, -1.4f, 0.3f},                // 1st person gun Offset
            {0.0f, 0.0f, 0.0f},                 // Muzzle Offset
            {0.03f, 0.03f, 0.15f},              // Recoil
            {0.0f, 0.0f, 0.1f},                 // Flash position
            0.625f,                             // Eject interval
            0.0f,                               // View kick
            DFALSE,                             // View kick is Cumulative
            DFALSE,                             // Animation looping
            DFALSE,                             // Alt anim looping
            nullptr,                            // Icon for status bar
            nullptr,                            // Highlighted icon for status bar
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
