# Build and play

Run commands from `Blood2/`. Requirements: CMake 3.28+, Ninja, Git, a C++20
compiler, SDL platform development dependencies and OpenSSL development files
for the launcher. The first configure downloads [pinned dependencies](../cmake/Dependencies.cmake).
Commercial game data stays outside the repository.

## Build

```sh
cmake --preset dev
cmake --build --preset dev --parallel
./build/dev/blood2-launcher
```

| Preset | Use |
| --- | --- |
| `dev` | Optimized build with inspector; use for playing and performance measurements. |
| `debug` | Unoptimized build with inspector. |
| `release` | Optimized build without inspector. |

All three build the game and launcher. Configure with `-DBLOOD2_BUILD_LAUNCHER=OFF`
for the game alone. See [launcher](launcher.md) and [tests](testing.md).

## Windows build

Install Visual Studio/Build Tools with **Desktop development with C++** and the
Windows SDK. Run the presets in an **x64 Native Tools Command Prompt** or x64
Developer PowerShell; Ninja gets its architecture from that shell, without `-A`.
Use `.exe` executable paths. Supply `-DOPENSSL_ROOT_DIR=...` if needed.

If CMake cached a missing compiler, enter the developer shell and reconfigure
with `cmake --preset dev --fresh`. If an old SDL extraction fails patch hashing,
use a clean directory; `--fresh` does not replace downloaded sources:

```powershell
cmake --preset dev -B build/windows-clean
cmake --build build/windows-clean --parallel
ctest --test-dir build/windows-clean -L headless --output-on-failure
```

## Content and launch

Select an installation in the launcher, or launch directly:

```sh
./build/dev/blood2 --data-dir "/path/to/Blood II"
./build/dev/blood2 --data-dir "/path/to/Blood II" --view-world --world 02_CityHub
./build/dev/blood2 --demo --data-dir "/path/to/Blood2 Demo"
```

Content path priority: `--data-dir`, `BLOOD2_DATA_DIR`, then ignored
`blood2.local.ini` ([example](../config/blood2.example.ini)). Config-relative paths
use that config's directory.

Normal launch opens the original menu. `--view-world` enters directly: SubwayA
for retail, Demo_01 for the demo. The retail intro locks movement/fire for about
12 simulation seconds. Demo_01 continues to Demo_02; Enter after death restarts
the current demo level with initial equipment. Demo EndGame returns through Info
to Main; Quit shows Info before exit. Direct demo entry exits after the end image.
Retail Single Player has the original Load and Save menus: eight slots, Quickload and Restart (the
start of the current level). After death, Use opens the Load menu. The demo has no saves; see the
[save contract](contracts/save_load_contracts.md) and the [demo retry hook](contracts/demo_contracts.md).

## Controls

| Action | Binding |
| --- | --- |
| Move / look | Physical WASD/ZQSD positions / mouse or arrows |
| Fire primary / secondary | Left / right mouse; F / G |
| Run / jump / crouch | Shift / Space / Ctrl |
| Use | E |
| Weapon slots | 1–9 / 0 or keypad |
| Medkit / flashlight | H / L |
| Binoculars / night goggles | Z / N |
| TheEye | I: throw, then toggle camera; touch the landed eye to retrieve |
| Attached BoneLeech | Tap E rapidly, or 25 presses at any pace |
| Proximity / remote / time bombs | P / R / T; B detonates remote charges |
| Quicksave / quickload | F5 / F6 (retail) |
| Pause / inspector | Esc / F10 |

Inventory actions require collection; release Shift before using them. Focus
capture does not fire a shot. Text editing/focus loss suppress game input.
F1/F2/F3 apply only to diagnostic smokes; see [input contracts](contracts/player_control_contracts.md).
These are the defaults. Options > Controls rebinds them (Enter, then the new key, button or
gamepad control; Esc cancels; Delete or Backspace unassigns); see the
[input action contracts](contracts/input_action_contracts.md). Gamepads use the console layout,
and touch screens show on-screen controls with a floating stick on the left.
Audio/Mouse/Display preferences, `controls.cfg` and saved games (`saves/retail/`) use SDL's
OpenBlood2/Blood2 user directory.

## Platforms

Linux/NVIDIA/Vulkan is tested; macOS/Metal execution is user-confirmed, with native
automated validation pending. Windows graphical qualification, mobile and VR
remain pending. Vulkan/SPIR-V and Metal/MSL shaders are bundled; D3D12 artifacts
are not. Ordinary builds do not need the Vulkan SDK or SDL_shadercross.

On a hybrid Linux machine, select the installed NVIDIA ICD and check the GPU in
the log. This path is machine-specific:

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ./build/dev/blood2 --data-dir "/path/to/Blood II"
```

For performance, use `dev` and the [benchmark commands](testing.md#performance).
For problems, collect [logs](logging.md) and [crash artifacts](crash_capture.md).
