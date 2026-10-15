# Blood II Reconstruction

A new C++20 project, independent of the earlier prototypes. **M0 and M1 are implemented:** SDL_GPU presentation, original `B2BaseClass`, `CameraSpot` and `Key` code, typed engine services, tested object lifetimes, external-content configuration and a live Dear ImGui inspector. No level or archive contents are loaded yet.

The runtime has three project targets: `engine` (object runtime, configuration and SDL/GPU lifetime), `blood2_game` (imported original code, registration and typed adapter), and `blood2_app` (executable named `blood2`, with optional inspection UI). There is no EnTT, ECS, ImGuizmo or inherited OpenGL renderer.

## Build and Run

Requirements: CMake 3.28+, Ninja, a C++20 compiler and SDL's platform development dependencies. A graphical run needs a working SDL_GPU backend: Vulkan on Linux, D3D12 on Windows or Metal on macOS. Only Linux/Wayland/Vulkan has been executed; the boundary also builds and passes headless tests with Clang.

From this directory:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
./build/debug/blood2 --data-dir "/path/to/Blood II"
```

The initial configure fetches exact dependency archives and verifies their SHA-256 hashes. Downloads are cached in `.cache/downloads/`; sources/build products stay under `build/`. No previous prototype is needed. SDL comes from the `main` commit recorded in [Dependencies.cmake](cmake/Dependencies.cmake), including upstream OpenXR support. The app does not create an XR session or require a headset.

On this workspace, `blood2.local.ini` already points to `/home/yann/.steam/steam/steamapps/common/Blood II/`, so simply run:

```sh
./build/debug/blood2
```

Press **F1** or the inspector's close button to hide/show the UI. Escape quits when the UI does not capture keyboard input. Objects shows two diagnostic objects executing original code. Select a row to inspect its actual state; use **Spawn CameraSpot**, **Spawn Key** and **Remove selected** to exercise lifetime. Commands apply at the next simulation boundary. Runtime shows backend/device/version information and archive file metadata. Resources remains empty until M2.

## Configuration

Copy [config/blood2.example.ini](config/blood2.example.ini) to `blood2.local.ini`, or supply `--config FILE`. The default configuration is read from the working directory. Relative paths inside a configuration file are resolved against that file's directory.

Content-root precedence is **`--data-dir` → `BLOOD2_DATA_DIR` environment variable → configuration file**. A supplied configuration must be valid; an explicitly missing file is an error even if another source provides a content root. Configuration supports one `data_dir` key, full-line `#` comments, spaces and optional surrounding double quotes. Paths and diagnostics use UTF-8. Local configuration and machine-specific CMake presets are ignored by Git.

```sh
./build/debug/blood2 --check-data
./build/debug/blood2 --frames 3 --capture build/inspector.png
./build/debug/blood2 --help
```

`--check-data` checks directory/file metadata and readability without initializing video or parsing archives. `BLOOD2.REZ` must be present and nonempty; the four other known archives are reported if present. Matching is case-insensitive and ambiguous duplicate filenames are rejected. File detection does not prove format compatibility. Original files are never modified or copied into the repository.

`--capture` saves the third presented frame as PNG using an explicit offscreen GPU target/readback. Normal frames render directly to the swapchain. Exit codes: `0` success, `2` configuration/content error, `3` platform/GPU/capture error, `4` game-boundary/smoke-test/capture completion failure.

## Tests

```sh
ctest --preset debug
```

The portable default suite has **22 headless tests**: 11 object contracts plus 11 checks for option parsing, configuration precedence/errors, UTF-8/relative paths, file metadata and CLI exit behavior. Run `ctest --preset debug -L objects` for the original-code/lifetime suite alone; it needs no display or retail data. Fixtures are synthetic text files, explicitly not valid REZ archives. They are never parser tests.

Opt into local installation and real-window tests:

```sh
cmake --preset debug -DBLOOD2_GPU_TESTS=ON -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build --preset debug --parallel
ctest --preset debug
```

This registers two additional tests: private content metadata and GPU lifecycle. The GPU test presents 24 frames, injects F1 hide/show events, resizes the window, saves a frame to `build/debug/tests/gpu.png`, removes the selected original `Key`, verifies its strings and selection are released, and exits through a queued quit event. It checks the resulting states and fails on timeout/errors. No physical input/headset is required, but a real display/backend is. These tests are opt-in rather than silently skipped or reported as passed on headless machines.

For this machine, the ignored `CMakeUserPresets.json` provides `local-debug` and `local-release` with these options and the supplied data path:

```sh
cmake --preset local-debug
cmake --build --preset debug --parallel
ctest --preset debug
```

Build the actual inspector-free variant separately:

```sh
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release
./build/release/blood2
```

The Release preset disables the inspector and does not fetch or link Dear ImGui. It still opens a GPU window and clears/presents frames; game rendering is future work. `BLOOD2_SANITIZERS=ON` enables ASan/UBSan on project targets for supported GCC/Clang toolchains. M1 passed all 22 headless cases with Clang ASan/UBSan and leak detection; see [reproduction commands](../Documentation/Implementation/M1.md#reproduce). A graphics sanitizer run has not been performed.

## Other Desktop Build Procedures

On Windows, use an MSVC Developer PowerShell with CMake/Ninja available, run the same configure/build commands, then `build/debug/blood2.exe --data-dir "C:/Games/Blood II"`. On macOS, use Apple Clang and the same presets, then `./build/debug/blood2 --data-dir "/path/to/Blood II"`. Start with headless CTest and enable the GPU test on an actual device. These procedures are documented but **not executed** in M0/M1; mobile and headset builds are later milestones.

## Next Work and Evidence

See [M1 results and limits](../Documentation/Implementation/M1.md), [object/service contracts](docs/OBJECT_CONTRACTS.md), the [roadmap](../Documentation/Roadmap/README.md), and [import/dependency provenance](docs/PROVENANCE.md). M2 adds bounded readers, world loading and the original local-session path. The M1 source interface is deliberately a subset; unsupported services cannot silently succeed.
