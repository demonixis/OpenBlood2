# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Rebindable first/third-person camera toggle (C), with obstacle clearance and the crosshair retained; disabled in VR.
- Error dialog naming the cause and the log folder when the game has to stop, instead of closing silently.

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
