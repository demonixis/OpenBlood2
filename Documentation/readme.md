# Documentation

| Document | Use |
| --- | --- |
| [Roadmap](roadmap.md) | Overall progress, M0–M9 completion and future work. |
| [Homebrew roadmap](roadmap_homebrew.md) | Exploratory console/handheld/headset port study (no milestone). |
| [Build and play](getting_started.md) | Requirements, commands, controls and platforms. |
| [CI builds](ci_builds.md) | Desktop archives, headless tests and dual-remote workflow. |
| [Packaging](packaging.md) | Version, icons, Xcode (macOS/iOS), Android and Quest presets. |
| [Tests](testing.md) | Headless, private-data, GPU and sanitizer commands. |
| [Setup screen](contracts/setup_screen_contracts.md) | Game data selection, demo installation and REZ mods. |
| [Logs](logging.md) / [crash capture](crash_capture.md) | Collect and inspect diagnostics. |
| [Architecture](architecture.md) | Targets, ownership and engine/game boundaries. |
| [Client cameras and HUD](implementation/reports/m9_client_view.md) | M9 independent camera selection/rays, client HUD ownership and local verification. |
| [Client audio](implementation/reports/m9_client_audio.md) | M9 logical server voices, independent client playback and local verification. |
| [Bloodbath player profiles](implementation/reports/m9_player_profiles.md) | Original skins, entry ammo and private startup announcement; fresh internal entry only. |
| [Server player connections](implementation/reports/m9_player_connections.md) | 16 checked connections, private ownership and original Bloodbath marker selection; wire admission pending. |
| [Client flashlight replica](implementation/reports/m9_flashlight.md) | Copied poses, generation-safe lifetime and independent client rays; unchanged battery rules. |
| [Moving client effect services](implementation/reports/m9_effect_services.md) | Independent moving FX, copied collision queries and typed child/audio outputs. |
| [Client cosmetic audio](implementation/reports/m9_effect_audio.md) | Impact playback, independent IDs, bounded client voices and dedicated isolation. |
| [Owner-bound client effects](implementation/reports/m9_effect_owners.md) | World-qualified particle/stream/PolyGrid/warp/light owners and independent replica updates. |
| [Client effect ownership and queries](implementation/reports/m9_effect_queries.md) | Independent cosmetic library, client-owned pools and fragment collision updates. |
| [M9 local client effects](implementation/reports/m9_client_effects.md) | Independent visual-effects frames, client scene consumers, lights/fog and local verification. |
| [Client environment replica](implementation/reports/m9_client_environment.md) | M9 local liquid/ladder/conveyor membership, independent geometry queries and solo verification. |
| [Client collision replica](implementation/reports/m9_client_collision.md) | M9 owned collision scene and independent prediction; solo validation and remaining services. |
| [Local client movement](implementation/reports/m9_local_movement.md) | M9 separate CMoveMgr body, speculative collision and immediate server correction; solo verification. |
| [Local client replica](implementation/reports/m9_client_world.md) | M9 object presentation through ClientWorld; remaining client/server dependencies and validation. |
| [Dedicated server foundation](dedicated_server.md) | Headless build and zero-client simulation; M9 player admission remains pending. |
| [Contributing](contributing.md) | Source reuse, conventions and verification. |
| [Server-class coverage](contracts/class_coverage.md) | Missing original registrations, replacements and Museum investigation. |
| [Remaining original VFX](implementation/reports/vfx_inventory.md) | Weapon/environment gaps, existing components and source evidence; delivery order in the roadmap. |
| [Contracts](contracts/readme.md) | Detailed subsystem rules and limitations. |
| [Implementation records](implementation/readme.md) | Provenance, level gaps, reports and test evidence. |
| [Historical studies](history/readme.md) | Earlier workspace inventory, audits and plans. |

[M2 is closed with reservation M2-GPU-001](implementation/reports/m2_acceptance.md).
M3 (in-game campaign acceptance) is current; it and later milestones remain unaccepted.
