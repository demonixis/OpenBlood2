# Blood II Reconstruction aka OpenBlood2

A new, independent C++20 implementation. **M0/M1 are complete for their documented Linux scope; M2 is in progress.** The app now mounts REZ archives, reads DAT v56 geometry and typed properties, and runs a supported subset of original objects in an in-game inspector. ABC skeletal/vertex animation, model callbacks and primary-world collision are implemented. Selected original shell/CPlayerObj entry, inventory, Beretta/knife animation and vector-fire/damage branches and original MoveMgr dry-land movement, stairs, crouch and body locomotion animations, containers, liquids, ladders, conveyors and damage/freefall volumes, original sliding doors, translating world-model collision, Trigger/SelfTrigger, key inventory, KeyPickup, original player USE, RotatingDoor, RotatingBrush, selected CDestructableBrush damage/removal and original CDebris explosive area damage and BoxPhysics pushable/gravity brushes with original damage impulses are connected. Original ammo/health/armor pickups and destruction spawns extend this subset; complete player gameplay remains missing. This is not a playable game.

The three runtime targets remain small: `engine` (resources, world/object runtime, configuration, SDL_GPU), `blood2_game` (original code and typed services) and `blood2_app` (`blood2`, optional Dear ImGui). No previous prototype is a build dependency, and no EnTT/ECS was introduced.

## Build and Run

Requirements: CMake 3.28+, Ninja, a C++20 compiler and SDL's platform development dependencies. Only Linux/Wayland/Vulkan has been tested. Windows/D3D12 and macOS/Metal remain target configurations, not verified platforms.

```sh
cmake --preset debug
cmake --build --preset debug --parallel
./build/debug/blood2 --data-dir "/path/to/Blood II"
```

Dependency archives and SHA-256 pins are in [Dependencies.cmake](cmake/Dependencies.cmake). SDL uses the selected `main` commit with upstream OpenXR support; the app does not start an XR session. Downloads/build products are ignored under `.cache/` and `build/`.

This workspace's ignored `blood2.local.ini` already configures `/home/yann/.steam/steam/steamapps/common/Blood II/`:

```sh
./build/debug/blood2
./build/debug/blood2 --inspect-world
./build/debug/blood2 --inspect-player
./build/debug/blood2 --world 02_CityHub
./build/debug/blood2 --world 01_SubwayA --frames 3 --capture build/world.png
```

The default world is `01_SubwayA`. Its patched DAT supplies 580 source objects and 116 BSP models. **144 source instances** run (`GameStartPoint`, `Key`, `PathPoint`, `WorldProperties`, `Conveyor`, `Damage`, `Door`, `Trigger`, `CDestructableBrush`, `ObjectivesTrigger`, `CameraObj`), plus **7 door-owned SelfTriggers** (151 runtime instances before player entry); 372 entries are original preprocessor-only objects and **64 required runtime objects remain unported**. Every source entry remains inspectable.

The World tab shows source properties, status, model selection and a diagnostic wireframe. Objects shows actual original instance state and spawn/remove controls. Resources shows archive precedence, indexed paths and missing world references. Diagnostics shows reached services. Reload/unload operations release original objects/strings and invalidate stale selection. Press **F1** to toggle inspection; Escape quits when UI does not capture input. Models previews the decoded Caleb skeleton, coat deformation, 86 animations and markers; it is not a CPlayerObj instance. Services shows collision index, gravity and original console variables. Session enters and inspects the actual CPlayerObj subset, client association, attributes, health/ammo, spawn and exit. Weapons exposes owned slots/hands, ammo, primary/alternate firing, actual weapon animation, hit/damage traces and sound resource requests. Movement shows the real player body/ABC pose, requested and resolved clips, dimensions, crouch clearance, stair attempts, ground contacts and a local collision map. Enable keyboard movement with **F2**: physical WASD/ZQSD, Shift to run, Space to jump, Ctrl to crouch, arrows to look, **E to USE** (one command per press). Text editing/focus loss suppress input; hiding with F1 keeps keyboard movement running. Containers lists original volumes, geometry, hidden state, body/head contents, air and water/ladder motion; it offers queued triggers and checked position edits. Space/Ctrl move up/down in free movement. Doors inspects the original state machine, transformed geometry, pusher/rider results and queued TRIGGER/TRIGGERCLOSE. Triggers inspects active/inactive sensors, target matches, delays, filters, locks and player keys; Doors links to its SelfTrigger. World key pickups retain resource diagnostics. Session shows the original 82-unit USE ray and target; Doors also shows angular motion, the hinge and posed collision geometry. Brushes shows rotating/destructible geometry, angular ramps, blocking/rider counts, health gates, queued ON/OFF/TOGGLE/KILL and retained destruction requests. Inspector commands test the direct message path. Audio and portal visibility remain pending. Liquid surfaces/fog/audio remain pending effect requests. Textures, sound playback and a complete game session remain unimplemented.

## Configuration and Modes

Copy [config/blood2.example.ini](config/blood2.example.ini) to ignored `blood2.local.ini`, or use `--config FILE`. Relative data paths are resolved against the config file's directory. Precedence: **`--data-dir` → `BLOOD2_DATA_DIR` → config**. Paths use UTF-8; explicitly missing/invalid configs fail even with another data-root source.

| Option | Behavior |
| --- | --- |
| `--check-data` | File metadata/readability only; no parser or GPU |
| `--inspect-world` | Headless REZ/DAT parsing and original subset creation/teardown |
| `--world NAME` | Resource name, default `01_SubwayA`; exact normalized lookup, no traversal/basename fallback |
| `--start-game` | Checks session startup and currently **fails with code 6** on missing original runtime services |
| `--no-inspector` | Hide development UI; simulation remains independent |
| `--frames N` / `--capture FILE.png` | Bounded graphical run / third-frame SDL_GPU readback |
| `--gpu-smoke` | M1 synthetic object/GPU lifecycle checks |
| `--world-smoke` | Real-world GPU, UI toggle, resize, unload/reload and teardown checks |
| `--inspect-player` | Headless original shell/player entry inspection, with explicit gameplay blockers |
| `--player-smoke` | GPU Session panel, hidden player updates, unload/re-entry checks |
| `--weapons-smoke` | GPU Beretta/knife firing while hidden, unload/re-entry; capture at frame 181 |
| `--cameras-smoke` | GPU original camera tracking, client view/player gates while hidden, inspection and reload |
| `--objectives-smoke` | GPU original objective delivery and localized client text while hidden, inspection and reload |
| `--pickups-smoke` | GPU original ammo collection while hidden, Ward inspection and reload |
| `--brushes-smoke` | GPU original brush ON/OFF, hidden rider transport, explosive chain and dynamic box gravity/impulse, inspection and reload |
| `--interaction-smoke` | GPU SDL E press-edge USE, missing-key denial/unlock, synthetic rotating hinge, hidden motion and reload |
| `--triggers-smoke` | GPU missing-key denial, real key pickup, hidden unlock/door movement, resize and reload |
| `--doors-smoke` | GPU original Door commands, hidden translation, resize, unload/reload; capture at frame 15 |
| `--movement-smoke` | GPU SDL walk/run/jump/crouch, live body pose and hidden stance changes, unload/re-entry; capture at frame 181 |
| `--containers-smoke` | GPU volume simulation while hidden and unload/re-entry: `--world 02_CityHub` (canal/air) or `--world 01_SubwayA` (conveyor), capture at frame 15 |
| `--help` | All options |

Mount order is base, sound, patch P, patch P2; later exact paths win. Nightmare Levels is excluded. Content stays external and is never modified/extracted into the source project. Exit codes: 0 success; 2 config/content metadata; 3 GPU/platform/capture; 4 object/smoke; 5 world/resource decode; 6 incomplete game-session startup.

## Tests

```sh
ctest --preset debug
ctest --preset debug -L world
```

There are **151 portable synthetic headless tests**: 11 original object contracts, 11 config/CLI checks, 10 reader/world/session/helper cases, 7 model, animation, collision and original-service cases, 5 player/client lifetime cases, 8 inventory/weapon/damage cases, 9 movement/input/contact/timing cases, 7 locomotion/clearance/stair/lifetime cases, 14 container/geometry/liquid/ladder/conveyor/damage/air cases, 6 door/mover/collision/lifetime cases, 8 trigger/key/ownership/contact cases, 9 USE/rotation/input/transport cases, 7 brush/ramp/damage/lifetime cases and 8 debris/explosion/radius/chain/lifetime cases and 10 pushable/physics/priority/rider cases, 9 pickup/spawn cases and 6 objective/resource-string cases plus 6 camera/tracking/player-gate cases. The old metadata text fixture is not used as an archive acceptance fixture; new tests generate bounded synthetic REZ/DAT data.

Enable installation-specific tests and real GPU windows explicitly:

```sh
cmake --preset debug -DBLOOD2_GPU_TESTS=ON -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build --preset debug --parallel
ctest --preset debug
```

The configured installation adds nineteen private headless tests and fourteen GPU tests, for **184 cases**. GPU captures are `build/debug/tests/gpu.png`, `world.png`, `player.png`, `weapons.png`, `movement.png`, `containers.png`, `conveyor.png`, `doors.png`, `triggers.png`, `interaction.png`, `brushes.png`, `pickups.png`, `objectives.png` and `cameras.png`. The ignored local presets enable these options:

```sh
cmake --preset local-debug
cmake --build --preset debug -j8
ctest --preset debug
cmake --preset local-release
cmake --build --preset release -j8
ctest --preset release
```

Public `release` disables Dear ImGui. It still loads the world and executes original object callbacks but presents a clear background until game rendering is implemented. `BLOOD2_SANITIZERS=ON` instruments project code with ASan/UBSan; [M2 evidence](../Documentation/Implementation/M2.md) gives Clang commands and actual results. Sanitizer checks do not cover a GPU session or its driver.

On Windows use an MSVC Developer PowerShell and the same CMake/Ninja commands, then `build/debug/blood2.exe`; on macOS use Apple Clang. These procedures, mobile and headset builds have not been executed.

## Contracts and Remaining Work

Read [world/session contracts](docs/WORLD_CONTRACTS.md), [object contracts](docs/OBJECT_CONTRACTS.md), [provenance](docs/PROVENANCE.md) and [M2 results](../Documentation/Implementation/M2.md). The [roadmap's M2 gate](../Documentation/Roadmap/03_MILESTONES.md#m2--connect-world-session-and-gameplay-services) remains open: complete player/gameplay callbacks, required world classes, broader mover/physics behavior and remaining original services must still be connected. See the [service continuation](../Documentation/Implementation/M2_SERVICES.md) for the new tested subset. M3 has not started.

The [pickup continuation](../Documentation/Implementation/M2_PICKUPS.md) records the preceding pickup stage; read [PICKUP_CONTRACTS.md](docs/PICKUP_CONTRACTS.md) before extending collection or destruction spawns. The [pushable continuation](../Documentation/Implementation/M2_PUSHABLES.md) records the preceding stage; read [PUSHABLE_CONTRACTS.md](docs/PUSHABLE_CONTRACTS.md) before extending body physics, pushing or impulses. The [explosion continuation](../Documentation/Implementation/M2_EXPLOSIONS.md) records the preceding stage; read [EXPLOSION_CONTRACTS.md](docs/EXPLOSION_CONTRACTS.md) before extending debris, area damage or chain reactions. Its effects are inspection requests; client FX/audio remain pending. The [brush continuation](../Documentation/Implementation/M2_BRUSHES.md) records the preceding stage; read [BRUSH_CONTRACTS.md](docs/BRUSH_CONTRACTS.md) before extending rotators, brush damage or debris. The [USE/rotation continuation](../Documentation/Implementation/M2_INTERACTION.md) records the preceding stage; read [INTERACTION_CONTRACTS.md](docs/INTERACTION_CONTRACTS.md) before extending USE, rotating collision or player door pushes. The [trigger/key continuation](../Documentation/Implementation/M2_TRIGGERS.md) records the preceding stage; read [TRIGGER_CONTRACTS.md](docs/TRIGGER_CONTRACTS.md) before extending activation, keys or SelfTrigger ownership. The [door continuation](../Documentation/Implementation/M2_DOORS.md) records the previous stage; read [DOOR_CONTRACTS.md](docs/DOOR_CONTRACTS.md) before extending world-model motion or door commands. The [specialized volume continuation](../Documentation/Implementation/M2_SPECIALIZED_VOLUMES.md) records the preceding stage (Conveyor, Damage, FreeFall and level-entry damage protection). Read [SPECIALIZED_VOLUME_CONTRACTS.md](docs/SPECIALIZED_VOLUME_CONTRACTS.md) before extending these branches. The [container continuation](../Documentation/Implementation/M2_CONTAINERS.md) records the preceding stage. Read [CONTAINER_CONTRACTS.md](docs/CONTAINER_CONTRACTS.md) for original VolumeBrush/MoveMgr/air rules, static shell queries and pending effects. The [locomotion continuation](../Documentation/Implementation/M2_LOCOMOTION.md) records the prior stage. Read [LOCOMOTION_CONTRACTS.md](docs/LOCOMOTION_CONTRACTS.md) for source clip fallbacks, atomic resize and the replacement stair solver. Read [MOVEMENT_CONTRACTS.md](docs/MOVEMENT_CONTRACTS.md) for the original MoveMgr subset, shared body, timing and grounding adaptations, and the subsequent translating platform extension. The [weapon continuation](../Documentation/Implementation/M2_WEAPONS.md) retains the earlier weapon evidence. Read [WEAPON_CONTRACTS.md](docs/WEAPON_CONTRACTS.md) for ownership, original behavior and deliberate limits. Read [PLAYER_CONTRACTS.md](docs/PLAYER_CONTRACTS.md) before extending the source subsets, typed client messages or player lifetime. Entry inspection succeeds; `--start-game` still fails with missing gameplay services.

## Source conventions

Runtime C++ namespaces use `OpenBlood2`; original global game/SDK class
names are preserved. Follow [.clang-format](.clang-format) and
[.editorconfig](.editorconfig). The current formatting was applied and checked
with clang-format 23.1.3 (a development tool, not a runtime dependency). Commit
each completed implementation run with its tests and English documentation.

The Pickups tab inspects all supported ammo/health/armor objects, including hidden/inactive or missing-resource entries, player pools/caps and the latest 32 pickup/spawn reports. Audio and pickup FX remain requests/metadata, not playback.

## Mission objectives continuation

[Latest evidence](../Documentation/Implementation/M2_OBJECTIVES.md) and
[contracts](docs/OBJECTIVES_CONTRACTS.md) cover original ObjectivesTrigger,
typed client broadcasts and bounded PE resource-string lookup from external
cres.dll. The Objectives tab shows source objects, delivery/resolution state and
actual localized text. Use `./build/debug/blood2 --objectives-smoke` for the hidden
UI/reload check. Original HUD and audio are pending; M2 remains incomplete.

## Scripted camera continuation

[Latest evidence](../Documentation/Implementation/M2_CAMERAS.md) and
[contracts](docs/CAMERA_CONTRACTS.md) cover original CameraObj activation/tracking,
typed client camera selection and player movement/USE/fire restrictions. Cameras
shows both source and published listener flags, target/pose, timers and player
restrictions; diagnostic ON/OFF/LINK controls use original messages. Run
`./build/debug/blood2 --cameras-smoke` for hidden-UI and reload checks. The selected
view, visibility and audio listener are inspection data/requests; cinematic world
rendering, KeyFramer/CinematicTrigger and audio remain pending. Latest SubwayA
accounting is 144 source instances + 7 SelfTriggers, 154 with player/owned models,
372 editor-only entries, 64 unported and zero failed. M2 is still open.
