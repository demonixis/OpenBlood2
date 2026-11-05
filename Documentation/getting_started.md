# Build and play

Run commands from `Blood2/`. Requirements: CMake 3.28+, Ninja, Git, a C++20
compiler and SDL platform development dependencies; Linux also needs OpenSSL
development files (the setup screen's demo download). The first configure downloads
[pinned dependencies](../cmake/Dependencies.cmake) and [content dependencies](../cmake/ContentDependencies.cmake).
Commercial game data stays outside the repository.

## Build

```sh
cmake --preset dev
cmake --build --preset dev --parallel
./build/dev/blood2
```

| Preset | Use |
| --- | --- |
| `dev` | Optimized build with inspector; use for playing and performance measurements. |
| `debug` | Unoptimized build with inspector. |
| `release` | Optimized build without inspector. |

See [tests](testing.md).

## Windows build

Install Visual Studio/Build Tools with **Desktop development with C++** and the
Windows SDK. Run the presets in an **x64 Native Tools Command Prompt** or x64
Developer PowerShell; Ninja gets its architecture from that shell, without `-A`.
Use `.exe` executable paths. HTTPS uses Windows' own TLS (Schannel); no OpenSSL is needed.

If CMake cached a missing compiler, enter the developer shell and reconfigure
with `cmake --preset dev --fresh`. If an old SDL extraction fails patch hashing,
use a clean directory; `--fresh` does not replace downloaded sources:

```powershell
cmake --preset dev -B build/windows-clean
cmake --build build/windows-clean --parallel
ctest --test-dir build/windows-clean -L headless --output-on-failure
```

## Content and launch

Without a configured data folder the game opens its [setup screen](contracts/setup_screen_contracts.md):
pick a detected or added Blood II folder (retail or demo), download the free demo,
enable data mods, then Play. The choice is remembered in `setup.cfg`; Options > SETUP
changes it. A data folder given on the command line skips the screen:

```sh
./build/dev/blood2 --data-dir "/path/to/Blood II"
./build/dev/blood2 --data-dir "/path/to/Blood II" --view-world --world 02_CityHub
./build/dev/blood2 --demo --data-dir "/path/to/Blood2 Demo"
```

Content path priority: `--data-dir`, `BLOOD2_DATA_DIR`, then ignored
`blood2.local.ini` ([example](../config/blood2.example.ini)). Config-relative paths
use that config's directory.

Normal launch shows the original title screen, then the menu; every load shows
the original [loading screen](contracts/loading_screen_contracts.md). `--view-world` enters directly: SubwayA
for retail, Demo_01 for the demo. The retail intro locks movement/fire for about
12 simulation seconds. Demo_01 continues to Demo_02; Enter after death restarts
the current demo level with initial equipment. Demo EndGame returns through Info
to Main; Quit shows Info before exit. Direct demo entry exits after the end image.
Retail Single Player has the original Load and Save menus: eight slots, Quickload and Restart (the
start of the current level). After death, Use opens the Load menu. The demo has no saves; see the
[save contract](contracts/save_load_contracts.md) and the [demo retry hook](contracts/demo_contracts.md).

## Bloodbath hosting and direct connection

The current [M9 implementation](implementation/reports/m9_rotation.md) supports
retail Bloodbath through the command line; Host/Join/LAN screens remain pending.
Use the same mounted content and protocol on all machines. Default port: UDP 27888.

```sh
./build/dev/blood2 --data-dir "/path/to/Blood II" --host multi/bb_crypt --name Caleb --end either --frag-limit 25 --time-limit 10 --map multi/bb_fields
./build/dev/blood2 --data-dir "/path/to/Blood II" --connect 192.168.1.10:27888 --name Ophelia
```

The dedicated target/preset is described in the [server foundation](implementation/reports/m9_multiplayer.md):

```sh
./build/server/blood2-server --data-dir "/path/to/Blood II" --world multi/bb_crypt --mode bloodbath --end either --frag-limit 25 --time-limit 10 --map multi/bb_fields
```

`--end` accepts `frags`, `time`, `either` or `never`. Defaults are 25 frags with
`frags` selected, plus 15 minutes for a host or 10 for the dedicated when a timed
rule is selected. Zero is an immediate threshold; use `never` to disable limits.
Repeat `--map` for a cyclic rotation of at most 50 maps including the initial
map; one map reloads itself. An end notice/bell and scoreboard precede the change
by more than 7.5 simulation seconds. Input is neutral during that interval.
Connections/profiles persist, but scores and inventory restart on the next map.
Esc/focus loss does not pause a hosted server; quitting its host ends the game.
Use E to respawn after death. Internet direct connection requires a reachable UDP
port; there is no relay or NAT traversal. Team/CTF admission remains pending.

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
| Bloodbath scores (network) | Tab / gamepad Back; toggle without pausing |

Inventory actions require collection; release Shift before using them. Focus
capture does not fire a shot. Text editing/focus loss suppress game input.
F1/F2/F3 apply only to diagnostic smokes; see [input contracts](contracts/player_control_contracts.md).
These are the defaults. Options > Controls rebinds them (Enter, then the new key, button or
gamepad control; Esc cancels; Delete or Backspace unassigns); see the
[input action contracts](contracts/input_action_contracts.md). Gamepads use the console layout,
and touch screens show on-screen controls with a floating stick on the left.
Audio/Mouse/Display preferences, `controls.cfg`, saved games (`saves/retail/`) and default
[logs](logging.md) (`logs/`) use SDL's OpenBlood2/Blood2 user directory
(`~/Library/Application Support/OpenBlood2/Blood2` on macOS, `%APPDATA%\OpenBlood2\Blood2` on
Windows, `~/.local/share/OpenBlood2/Blood2` on Linux). A nonempty `BLOOD2_USER_DIR` environment
variable replaces that directory, for example for a portable or throwaway profile.

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
