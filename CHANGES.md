# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.5.1] - 2026-10-29

### Added

- The original weapon list on the right of the status bar, shown and hidden with F1 ([#5](https://github.com/demonixis/OpenBlood2/issues/5)).

### Fixed

- The game no longer stops about 100 seconds after entering Condemned Tenement (CityHubB) when its start crate, or a pickup or body in CityHubB/C, Abandoned Buildings or the Train Station, falls out of the level ([#6](https://github.com/demonixis/OpenBlood2/issues/6)).
- Enemies no longer freeze, become unkillable and block saving after being hurt by the Steam Tunnels valves, other damage volumes or their own explosions ([#7](https://github.com/demonixis/OpenBlood2/issues/7)).
- Enemies no longer freeze while planning a route past a moved door or platform, or from inside an obstacle (Steam Tunnels, Center for Disease Management) ([#7](https://github.com/demonixis/OpenBlood2/issues/7)).

## [0.5.0] - 2026-10-25

### Added

- Rebindable first/third-person camera toggle (C), with obstacle clearance and the crosshair retained; disabled in VR.
- Error dialog naming the cause and the log folder when the game has to stop, instead of closing silently.
- Blood II: The Nightmare Levels can be selected with the setup screen's "Launch Nightmares" box (Options > SETUP) or `--nightmares`, with its title, menu background, Single Player > NIGHTMARES row, level loading screens, credits, the robed cultists and the rock-throwing Gremlins; its levels are not fully playable yet.

### Changed

- Widescreen view keeps the 4:3 vertical field of view and widens the horizontal one, so the view weapon is no longer cut off.
- Smooth filtering for sprites and effects (muzzle flashes, blood pools).
- Data mods may replace `cres.dll` (read as resources, never run).
- CityHub escalators no longer switch the world fog off underfoot.

### Fixed

- Single Player > Chosen now plays without cut scenes, as in the original game.
- Cheats, weapon drops and player death in custom levels such as the Credits world.
- M16 grenade simulation failure when a living enemy overlaps it before its first bounce.
- Museum crash after Gideon's last hologram, when enemies spawn into the skylight glass.
- Fog-coloured instead of black letterbox bars during cutscenes.
- Hovering characters such as Gideon drifting up or down when their animation changes size.
- Seated characters floating above their seats in the Nightmares intro.
- Enemies behind walls now wake as in the original (level visibility data), so scripted guards react to triggers and Gideon idles in the CityHub intro.

## [0.4.0] - 2026-10-25

### Added

- Fullscreen modes, resolution, VSync and frame rate limits in Display options.
- Supported headset refresh rates and render scale in VR options.
- Nine assignable controls: strafe, auto-run, turn around, drop weapon, grab, local message, taunt, mouse aim and crosshair.
- Original destruction debris and additional ballistic, shotgun, flare, Tesla, explosive and energy weapon effects.
- Dynamic VFX lighting, selecting up to 12 lights per object on mobile and 32 on desktop, plus volume fog.

### Changed

- Sharper weapon and world textures with trilinear filtering and 8× anisotropy.
- Smaller Android/Quest APKs; CI and release downloads contain only archives and APKs.

### Fixed

- CityHub laundromat door rotation and adjacent sliding-door closure in the demo.
- Player look remaining active during cutscenes and death cameras.
- Transparent effect ordering and bullet marks extending beyond hit surfaces.
- Simulation failure logs reporting unrelated startup warnings.
- Xcode archives including dependencies and triggering nested builds.

[Unreleased]: https://github.com/demonixis/OpenBlood2/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/demonixis/OpenBlood2/compare/v0.3.1...v0.4.0
