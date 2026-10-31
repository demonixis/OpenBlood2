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
| [Initial network world](implementation/reports/m9_world_baseline.md) | Bounded ENet object baseline, independently resolved client assets and all-map verification. |
| [Network object membership](implementation/reports/m9_world_membership.md) | Reliable creation/removal, metadata updates, baseline ACK and retired-generation checks. |
| [Network poses and interpolation](implementation/reports/m9_world_state.md) | Absolute 20 Hz poses, bounded per-object history and reliable teleport revisions. |
| [Network collision/environment](implementation/reports/m9_collision_transport.md) | Client-owned DAT geometry, volume properties and atomic object/collision reception. |
| [Independent client movement](implementation/reports/m9_movement_services.md) | Server-independent CMoveMgr, replica prediction and original report cadence. |
| [Reusable network client](implementation/reports/m9_network_client.md) | Independent admission, world/movement/HUD/camera endpoint and monotonic presentation clock. |
| [Graphical network frontend](implementation/reports/m9_network_frontend.md) | Direct IPv4 connection, single-simulation host, independent rendered world/HUD and fixed-tick input. |
| [Independent weapon runtime](implementation/reports/m9_weapon_services.md) | CViewWeapon service boundary, unchanged synchronous solo bridge and independent retail animation tests. |
| [Private network weapon loadout](implementation/reports/m9_weapon_loadout.md) | Addressed two-hand state, mode-specific ammo and permissions, atomic resource loading and remote draw/rest views. |
| [Reliable network player commands](implementation/reports/m9_player_commands.md) | Original weapon/item/USE dispatch, bounded request/ACK ordering, input edges and remote zoom. |
| [Animation-validated network fire](implementation/reports/m9_network_fire.md) | Original animation timing on the server, independent prediction, checked actual fire and bounded input/ACK transport. |
| [Bloodbath death and respawn](implementation/reports/m9_network_death.md) | Original slow-death clocks, USE rescue/respawn, frag attribution and independent weapon cancellation. |
| [Remote damage and door impulses](implementation/reports/m9_network_impulses.md) | Reliable additive velocity, impulse-aware reports/ACKs and passive dead-player movement. |
| [Real-process Bloodbath combat](implementation/reports/m9_pvp.md) | Held fire in the graphical frontend; dedicated and host combat, private damage, frag and USE respawn. |
| [Remote logical audio](implementation/reports/m9_network_audio.md) | Reliable sound starts/stops, private delivery, late loops and independent client playback. |
| [Independent client speech](implementation/reports/m9_client_speech.md) | Original VoiceMgr/CommLink services, actual private playback and shared bounded audio. |
| [Validated weapon sounds](implementation/reports/m9_weapon_sounds.md) | Original animation one-shots/loops/stops, life checks and independent remote playback. |
| [Reliable player roster](implementation/reports/m9_roster.md) | Independent names/characters/frags, late arrivals, departure and reconnect identity. |
| [Client-routed CTF](implementation/reports/m9_ctf_capture.md) | Jam capture, late carried-flag admission, score-limit rotation and bounded weapon waiting under overload. |
| [Network CTF](implementation/reports/m9_ctf_network.md) | Dedicated/host admission, carried flags, absolute capture totals, independent notices/audio and retained-team rotation. |
| [Server CTF rules](implementation/reports/m9_ctf_rules.md) | Combined capture/match scores, OnlyFlagScores, membership multiplier, drops and bounded rotation; wire admission pending. |
| [Team network play](implementation/reports/m9_team_network.md) | Team admission, absolute totals, retained assignments, source team HUD/columns and dedicated/host verification. |
| [Server Team rules](implementation/reports/m9_team_rules.md) | Original membership, team skins/spawns, frag totals and friendly-fire/Voodoo rules; network Team admission follows. |
| [Bloodbath limits and map rotation](implementation/reports/m9_rotation.md) | Original frag/time/intermission rules, same-peer world replacement and dedicated/host process checks. |
| [Bloodbath scoreboard and player notices](implementation/reports/m9_scoreboard.md) | Original score ordering/art, Tab/gamepad toggle, death/revival visibility and localized arrival/departure text. |
| [Network audio cache](implementation/reports/m9_audio_cache.md) | Reclaim unused client clips within fixed budgets while keeping live voices and logical server audio intact. |
| [Combat announcements](implementation/reports/m9_combat_announcer.md) | Original frag/suicide/FinishHim tables, private voice playback and bounded observer notices. |
| [Connection clock](implementation/reports/m9_connection_clock.md) | Exclude synchronous content preparation from the strict connection deadline. |
| [Independent scene inputs](implementation/reports/m9_scene_inputs.md) | Server-free renderer inputs, original weapon visuals and world-owned GPU presentation. |
| [Network camera state](implementation/reports/m9_camera_transport.md) | Reliable selection, replica/predicted views and authoritative camera movement/look gates. |
| [Private player events](implementation/reports/m9_private_events.md) | Reliable pointer-free event delivery, independent HUD inbox and corrected broadcasts. |
| [Client movement correction](implementation/reports/m9_movement_client.md) | Independent setup/ACK receiver, bounded CMoveMgr replay and moving separate-process clients. |
| [Movement transport](implementation/reports/m9_movement_transport.md) | Private setup/report/ACK codecs, checked server movement and UDP tests; client correction remains pending. |
| [Network admission](implementation/reports/m9_admission.md) | Bounded ENet handshake, full-content gate and original player lifetime; gameplay replication pending. |
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
| [Dedicated server foundation](dedicated_server.md) | Headless build, Bloodbath/Team admission, gameplay and match rotation; qualification limits. |
| [Contributing](contributing.md) | Source reuse, conventions and verification. |
| [Server-class coverage](contracts/class_coverage.md) | Missing original registrations, replacements and Museum investigation. |
| [Remaining original VFX](implementation/reports/vfx_inventory.md) | Weapon/environment gaps, existing components and source evidence; delivery order in the roadmap. |
| [Contracts](contracts/readme.md) | Detailed subsystem rules and limitations. |
| [Implementation records](implementation/readme.md) | Provenance, level gaps, reports and test evidence. |
| [Historical studies](history/readme.md) | Earlier workspace inventory, audits and plans. |

[M2 is closed with reservation M2-GPU-001](implementation/reports/m2_acceptance.md).
M3 (in-game campaign acceptance) is current; it and later milestones remain unaccepted.
