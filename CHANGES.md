# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

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
