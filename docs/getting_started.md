> Current normal menu controls: launch without `--view-world` for the main menu;
> F10 toggles inspection and Esc pauses/resumes the game and its audio. The older
> F1/F2/F3 bindings below apply only to diagnostic smokes. Audio/Mouse preferences
> use SDL's OpenBlood2/Blood2 user directory. See [menu contracts](contracts/menu_contracts.md).

# Setup, controls and validation

The [README](../README.md) contains the basic build. Run the following commands
from the Blood2 repository. Dependency pins and the bounded local SDL correction
are in [Dependencies.cmake](../cmake/Dependencies.cmake). No prototype repository
is a build dependency. The first configure needs network access; build products
and downloaded dependencies are ignored under `build/` and `.cache/`.

The game build disables unused SDL subsystems: `SDL_RENDER`, `SDL_CAMERA`,
`SDL_DIALOG`, `SDL_TRAY`, `SDL_NOTIFICATION` and `SDL_DISKAUDIO`.
Both the game and ImGui render directly through SDL_GPU;
`SDL_RENDER` controls the separate SDL_Renderer API. GPU/OpenXR support,
platform audio/video backends, controller support and dummy test drivers retain
their existing configuration. OpenGL/OpenGL ES remain at their defaults because
SDL's KMSDRM video backend depends on EGL support enabled by those options.
Reconfigure existing build directories to apply these settings.
Dependency revisions are unchanged. See the
[validation record](../../Documentation/Implementation/sdl_build_options.md).

## Windows build

Install these tools, or check that they are already available:

- Visual Studio or Build Tools with **Desktop development with C++**, an MSVC
  C++20 toolset for x64, and a Windows 10/11 SDK.
- CMake 3.28 or newer, Ninja and Git, available on `PATH`.
- Network access for the initial dependency downloads. CMake fetches and builds
  pinned SDL3, spdlog and Dear ImGui; do not install separate copies.

Open the **x64 Native Tools Command Prompt** for Visual Studio, or initialize an
x64 Developer PowerShell. A normal terminal does not automatically load MSVC's
compiler, headers and libraries. For an existing PowerShell session, discover
the installed C++ tools and initialize them with:

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$vsPath\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
```

If no installation is returned, add the C++ workload through Visual Studio
Installer. From the repository directory in that developer shell, run:

```powershell
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev -L headless
.\build\dev\blood2.exe --data-dir "D:\Games\Blood II"
```

Use your own game installation path. Commercial data is required to play, but
not to compile or run synthetic tests. The `dev` preset enables optimization and
inspection; `release` disables inspection and `debug` disables optimization.
Keep GPU tests opt-in as described under [Tests](#tests).

If configuration previously reported `No CMAKE_C_COMPILER could be found`, enter
the developer shell and run `cmake --preset dev --fresh` before building. Use the
same shell for configuration and compilation. Do not pass `-A x64` to the Ninja
presets; the developer shell selects the compiler architecture.

The SDL property patch writes LF bytes explicitly on every platform and retains
its exact input/output SHA-256 checks. If an older Windows configure left
`SDL property fixes did not produce the expected source`, configure into a new
build directory so FetchContent extracts a clean copy:

```powershell
cmake --preset dev -B build/windows-clean
cmake --build build/windows-clean --parallel
ctest --test-dir build/windows-clean -L headless --output-on-failure
```

Here the executable is `build/windows-clean/blood2.exe`; the ordinary `dev` build
and test presets still refer to `build/dev`. `--fresh` clears the CMake cache but
does not remove an already modified dependency source tree. Do not disable hash
checks to recover a failed patch.

For graphical execution, install the GPU vendor's driver with Vulkan support.
The bundled game shaders support Vulkan on Windows; Direct3D 12 shader artifacts
are not supplied. The Vulkan SDK is optional for an ordinary build, and provides
validation layers and tools for GPU development. Shader regeneration uses the
separate [offline toolchain](contracts/metal_shader_contracts.md).

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

The 1998 two-level demo (`B2Demo.rez`) needs `--demo`; its first world is `Demo_01`.
Normal launch opens the [demo menu](contracts/demo_menu_contracts.md). Choose
Caleb and difficulty to start; the authored exit continues to Demo_02. EndGame
shows Info, then Esc/Enter/Space returns to Main. Quit shows Info before exiting.
`--view-world` retains menu-free direct entry and its end-image-to-exit behavior.
Esc pauses/resumes; Enter after death restarts the current level with initial
equipment. See [demo contracts](contracts/demo_contracts.md) for validation limits
and the future save-system integration point.

```sh
./build/debug/blood2 --demo --data-dir "/path/to/Blood2 Demo"
./build/debug/blood2 --demo --data-dir "/path/to/Blood2 Demo" --view-world --world Demo_02
```

The retail default world is SubwayA. `--view-world` enters the original player and
requests mouse capture. The inspector can also **Enter player and play**.
The authored intro restricts movement/fire for roughly twelve simulation seconds.
`--inspect-world` and `--inspect-player` provide diagnostic CLI output;
`--start-game` still reports an incomplete M2 acceptance gate.

## Controls

Every launch writes [diagnostic log files](logging.md) under SDL's user directory.
Use `--log-dir DIR --log-level debug` for a reproduction; retain the whole session
directory after a problem. File logging also works without the development inspector.
On Linux, install GDB on `PATH` to enable automatic [crash stacks and cores](crash_capture.md).
The startup log reports availability; `--no-crash-capture` leaves native handling alone.

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
for regeneration. Windows setup is described [above](#windows-build); graphical
Windows execution, mobile and VR remain unverified. D3D12 shader artifacts are not
bundled. SDL's OpenXR code is pinned, but the app does not start an XR session.

## Tests

A normal full build includes the synthetic test executables. Enable private
installation tests and real GPU windows explicitly:

```sh
cmake --preset debug -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II" -DBLOOD2_GPU_TESTS=ON
cmake --build --preset debug --parallel
ctest --preset debug -L headless
ctest --preset debug -L gpu
```

Demo-data checks (label `demo`) are enabled separately with
`-DBLOOD2_DEMO_DATA_DIR="/path/to/Blood2 Demo"` (the directory holding `B2Demo.rez`).

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
call, pass, culling, lighting and shadow counters. Add `--capture FILE.png` to save the last
measured frame. `--no-model-shadows` disables model shadows, as the original low and medium
detail settings do, for smaller GPUs or comparisons, and `--pixel-lighting` lights models
per pixel. In the game, Options > Display holds the original DETAIL LEVEL and the
per-vertex/per-pixel LIGHTING setting, saved in `preferences.cfg`. The inspector's 3D view shows the same counters
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
