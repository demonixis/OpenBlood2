# Blood II Reconstruction aka OpenBlood2

A new, independent C++20 implementation. **M0/M1 are complete for their documented Linux scope; M2 is in progress.** The app now mounts REZ archives, reads DAT v56 geometry and typed properties, and runs a supported subset of original objects in an in-game inspector. ABC skeletal/vertex animation, model callbacks and primary-world collision are implemented. Selected original shell/CPlayerObj entry, inventory, Beretta/knife animation and vector-fire/damage branches and original MoveMgr standing movement are connected; complete player gameplay and moving world-model collision remain missing. This is not a playable game.

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
./build/debug/blood2 --world 01_SubwayA --frames 3 --capture build/world.png
```

The default world is `01_SubwayA`. Its patched DAT supplies 580 source objects and 116 BSP models. **18 original instances** run (`GameStartPoint`, `Key`, `PathPoint`, `WorldProperties`); 372 entries are original preprocessor-only objects and **190 required runtime objects remain unported**. Every source entry remains inspectable.

The World tab shows source properties, status, model selection and a diagnostic wireframe. Objects shows actual original instance state and spawn/remove controls. Resources shows archive precedence, indexed paths and missing world references. Diagnostics shows reached services. Reload/unload operations release original objects/strings and invalidate stale selection. Press **F1** to toggle inspection; Escape quits when UI does not capture input. Models previews the decoded Caleb skeleton, coat deformation, 86 animations and markers; it is not a CPlayerObj instance. Services shows collision index, gravity and original console variables. Session enters and inspects the actual CPlayerObj subset, client association, attributes, health/ammo, spawn and exit. Weapons exposes owned slots/hands, ammo, primary/alternate firing, actual weapon animation, hit/damage traces and sound resource requests. Movement shows the real player body, ground contacts, speeds, jump/landing counts and a local collision map. Enable keyboard movement with **F2**: physical WASD/ZQSD, Shift to run, Space to jump, arrows to look. Text editing/focus loss suppress input; hiding with F1 keeps keyboard movement running. Textures, sound playback and a complete game session remain unimplemented.

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
| `--movement-smoke` | GPU SDL walking/jump/landing while hidden, unload/re-entry; capture at frame 181 |
| `--help` | All options |

Mount order is base, sound, patch P, patch P2; later exact paths win. Nightmare Levels is excluded. Content stays external and is never modified/extracted into the source project. Exit codes: 0 success; 2 config/content metadata; 3 GPU/platform/capture; 4 object/smoke; 5 world/resource decode; 6 incomplete game-session startup.

## Tests

```sh
ctest --preset debug
ctest --preset debug -L world
```

There are **61 portable synthetic headless tests**: 11 original object contracts, 11 config/CLI checks, 10 reader/world/session/helper cases, 7 model, animation, collision and original-service cases, 5 player/client lifetime cases, 8 inventory/weapon/damage cases, and 9 movement/input/contact/timing cases. The old metadata text fixture is not used as an archive acceptance fixture; new tests generate bounded synthetic REZ/DAT data.

Enable installation-specific tests and real GPU windows explicitly:

```sh
cmake --preset debug -DBLOOD2_GPU_TESTS=ON -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build --preset debug --parallel
ctest --preset debug
```

The configured installation adds seven private headless tests and five GPU tests, for **73 cases**. GPU captures are `build/debug/tests/gpu.png` , `world.png`, `player.png`, `weapons.png` and `movement.png`. The ignored local presets enable these options:

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

Read [world/session contracts](docs/WORLD_CONTRACTS.md), [object contracts](docs/OBJECT_CONTRACTS.md), [provenance](docs/PROVENANCE.md) and [M2 results](../Documentation/Implementation/M2.md). The [roadmap's M2 gate](../Documentation/Roadmap/03_MILESTONES.md#m2--connect-world-session-and-gameplay-services) remains open: complete player/gameplay callbacks, required world classes, moving world-model collision and remaining original services must still be connected. See the [service continuation](../Documentation/Implementation/M2_SERVICES.md) for the new tested subset. M3 has not started.

The [movement continuation](../Documentation/Implementation/M2_MOVEMENT.md) is the latest evidence. Read [MOVEMENT_CONTRACTS.md](docs/MOVEMENT_CONTRACTS.md) for the original MoveMgr subset, shared body, timing and grounding adaptations, and outstanding stairs/containers/platforms. The [weapon continuation](../Documentation/Implementation/M2_WEAPONS.md) retains the earlier weapon evidence. Read [WEAPON_CONTRACTS.md](docs/WEAPON_CONTRACTS.md) for ownership, original behavior and deliberate limits. Read [PLAYER_CONTRACTS.md](docs/PLAYER_CONTRACTS.md) before extending the source subsets, typed client messages or player lifetime. Entry inspection succeeds; `--start-game` still fails with missing gameplay services.
