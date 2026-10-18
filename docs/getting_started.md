# Setup, controls and validation

The [README](../README.md) contains the basic build. Run the following commands
from the Blood2 repository. Dependency pins and the bounded local SDL correction
are in [Dependencies.cmake](../cmake/Dependencies.cmake). No prototype repository
is a build dependency. The first configure needs network access; build products
and downloaded dependencies are ignored under `build/` and `.cache/`.

## Content and launch

Provide a legally obtained Blood II installation externally. Pass `--data-dir`,
set `BLOOD2_DATA_DIR`, or copy [the example config](../config/blood2.example.ini)
to ignored `blood2.local.ini`. Precedence is command line, environment, then config;
relative paths in a config are resolved against that file's directory.

```sh
./build/debug/blood2 --data-dir "/path/to/Blood II" --view-world
./build/debug/blood2 --data-dir "/path/to/Blood II" --world 02_CityHub --view-world
ctest --preset debug -L headless
```

The default world is SubwayA. `--view-world` enters the original player and
requests mouse capture. The inspector can also **Enter player and play**.
The authored intro restricts movement/fire for roughly twelve simulation seconds.
`--inspect-world` and `--inspect-player` provide diagnostic CLI output;
`--start-game` still reports an incomplete M2 acceptance gate.

## Controls

| Action | Binding |
| --- | --- |
| Move | Physical WASD / ZQSD positions |
| Look | Captured mouse; arrow keys also work |
| Primary / secondary fire | Left / right mouse button; F / G |
| Run / jump / crouch | Shift / Space / Ctrl |
| Use | E |
| Owned weapon slots | 1..9 / 0, or keypad; initially 1 knife, 2 Beretta |
| Medkit / flashlight | H / L after collection; release Shift for inventory use |
| Proximity / remote bombs | P / R; B detonates owned remote charges |
| Inspector / play | F1, or Escape to release capture and inspect |
| Mouse capture / keyboard controls | F3 / F2 |

The first click that focuses/captures the window is not a shot. Text editing and
focus loss suppress game input. Inspection can be folded with its title arrow,
resized, hidden and reopened. The original PCX statistics HUD shows health, armor, the current weapon icon and
ammunition; it hides for external-camera cinematics. The original default crosshair
is centered in the view and also hides after death. Source camera/player gates
still control whether movement, inventory actions or fire are accepted.

## Platforms and GPU selection

Linux/NVIDIA/Vulkan is tested. On a hybrid Linux machine, select the installed
NVIDIA Vulkan ICD for local testing and check the logged GPU name, for example:

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ./build/debug/blood2 --data-dir "/path/to/Blood II" --view-world
```

The ICD path is machine-specific and is not a production requirement. On macOS,
use Apple Clang and the same CMake/Ninja presets. Bundled MSL shaders enable Metal;
ordinary builds do not require SDL_shadercross. Native execution is user-confirmed,
but automated Apple validation remains pending. See [Metal contracts](contracts/metal_shader_contracts.md)
for regeneration. Windows, mobile and VR are unverified; D3D12 shader artifacts
are not bundled. SDL's OpenXR code is pinned, but the app does not start an XR session.

## Tests

A normal full build includes the synthetic test executables. Enable private
installation tests and real GPU windows explicitly:

```sh
cmake --preset debug -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II" -DBLOOD2_GPU_TESTS=ON
cmake --build --preset debug --parallel
ctest --preset debug -L headless
ctest --preset debug -L gpu
```

GPU tests open windows and should run without concurrent interactive sessions.
Captures stay under `build/debug/tests/`. Use the local NVIDIA environment above
for GPU tests on this developer machine; dummy audio may be selected for unattended
checks. The `release` preset disables inspection but keeps the HUD and rendering.
Run relevant checks there when changing application or UI lifetime.

## Performance measurement

Use the `dev` preset (optimized, with inspector) when judging frame rate; Debug
builds are several times slower in large worlds. `--bench` measures the same
presentation path headlessly, without a window or mouse capture:

```sh
cmake --preset dev
cmake --build --preset dev --parallel
./build/dev/blood2 --data-dir "/path/to/Blood II" --world 02_CityHub --bench-size 3440x1440 --frames 600
./build/dev/blood2 --data-dir "/path/to/Blood II" --world 02_CityHub --bench-sweep
```

It reports simulation, presentation and GPU completion times per frame plus draw
call, pass and culling counters. The inspector's 3D view shows the same counters
for the last presented frame. See [presentation performance](contracts/render_performance_contracts.md).

For a separate Clang sanitizer build:

```sh
cmake -S . -B build/sanitize -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DBLOOD2_SANITIZERS=ON -DBLOOD2_INSPECTOR=OFF \
  -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build build/sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure
```

The [implementation reports](../../Documentation/Implementation/m2.md) distinguish
synthetic, private-data and GPU evidence, failed attempts and unverified paths.
Passing a selected regression set does not imply every project test or milestone
passes. The [message-lifetime report](../../Documentation/Implementation/m2_message_lifetime.md)
records an additional GPU LeakSanitizer failure separately from the passing
headless sanitizer checks. The [door-audio audit](../../Documentation/Implementation/m2_door_audio.md)
reproduces it without a world/player and attributes the reported allocation
sites to NVIDIA/DBus mappings. It remains an unsuppressed failed GPU leak check.

## Validate session startup

Run `./build/debug/blood2 --start-game` for a finite original-shell/player startup
check and one update, followed by cleanup. It returns a nonzero exit code for
missing classes/resources, unavailable services or invalid player entry.
`--view-world` applies the same check before graphical entry. See
[the startup contract](contracts/session_start_contracts.md) for diagnostic modes,
ownership and the distinction from full M2 acceptance.
