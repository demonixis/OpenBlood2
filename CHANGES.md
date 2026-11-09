# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.6.0] - 2026-11-09

### Added

- Language selection in Setup and Options. Alongside English, the game offers French, German, Italian, Spanish, Brazilian Portuguese, Simplified Chinese and Korean. Translations cover menus, HUD and objectives, loading screens, credits, multiplayer notices, and original signs and billboards.
- The original CommLink portraits and text, plus localized subtitles for stock cutscenes and selected in-game dialogue.
- Playback of the original Monolith and GTI logos and The Chosen introduction from installed Smacker movies, with localized subtitles for the introduction. Nightmares plays the logos before its own title screen.
- The Nightmare boss and its homing fireballs, the Combat Shotgun and its gas grenades, and the Flayer's chains and portals in The Nightmare Levels. Save support for these new expansion objects is pending.
- Soccer ball and goal behavior for the two original expansion maps, including kicks, scoring and ball respawns.
- Support for 39 previously missing server classes, including cranes and wrecking balls, fire, rain, scripted scene objects, and additional projectiles and visual effects.

### Changed

- The Chosen's desktop Bloodbath hub now uses the original menu layout. Host, Join, Character Setup, LAN discovery, map rotation and the in-game Resume and Leave controls use the same interface for Bloodbath, Team and Capture the Flag.

### Fixed

- Authored enemy spawns and progression triggers in Cathedral, Ancient City and the Arena, including fights where the next enemy spawns before the previous body is removed.
- Music during Gideon's death sequence in Ancient City now handles overlapping instrument note ranges.
- Ophelia's Nightmares level now loads with untextured faces marked `Default` by the original editor.
- Older prototype PBR preferences load as Enhanced rendering with High lighting.

## [0.5.2] - 2026-11-05

### Added

- The original objectives page (Tab), air/throw/boss power bar and on-screen message lines, including pickup messages ([#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- Options > Display > FIELD OF VIEW, 60 to 120 degrees (default 90, the original); zoom and the binoculars keep their original magnification.

### Fixed

- The game no longer stops in the Center for Disease Management when a grenade fragment's smoke trail moves far in one frame ([#9](https://github.com/demonixis/OpenBlood2/issues/9)).
- Skies are drawn like the original (no inside-out cloud layers), with their sky fog and the world's far distance ([#11](https://github.com/demonixis/OpenBlood2/issues/11), [#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- The barred double doors of Cas Revenant open ([#11](https://github.com/demonixis/OpenBlood2/issues/11)).
- Exploding walls, speakers, fans, the floating ship and other exploding objects show their explosion and play its sound; wood crate fragments are visible ([#11](https://github.com/demonixis/OpenBlood2/issues/11), [#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- Museum skylights, display cases and other black-keyed glass are transparent ([#10](https://github.com/demonixis/OpenBlood2/issues/10), [#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- The view is tinted under water, blood and acid ([#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- Fog and the background follow the night goggles, Seeing Eye and blindness tints (a blinded player no longer sees the fog).
- Escalators and conveyors drag the player at the original speed, and the player can jump out of shallow water (Museum shotgun pool) ([#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- Museum trip lasers damage the player ([#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- The Mac-10 alternate fire and other weapons play their animation sounds ([#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- Gabriella appears in the Museum cut-scene ([#8](https://github.com/demonixis/OpenBlood2/issues/8)).
- The park seesaw plays its original double clank ([#8](https://github.com/demonixis/OpenBlood2/issues/8)).

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

[Unreleased]: https://github.com/demonixis/OpenBlood2/compare/v0.6.0...HEAD
[0.6.0]: https://github.com/demonixis/OpenBlood2/compare/v0.5.2...v0.6.0
[0.4.0]: https://github.com/demonixis/OpenBlood2/compare/v0.3.1...v0.4.0
