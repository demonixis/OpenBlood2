# Desktop launcher

The SDL3/SDL_GPU and Dear ImGui launcher implements L0 through L2 of the
[plan](../../Documentation/Roadmap/06_launcher.md): select external retail or
demo data, remember installations, check the game and REZ indexes, and start
OpenBlood2. Graphics editing, mods and demo download/installation remain later
steps. Nightmares is excluded. The launcher uses a plain title panel; optional
original artwork extraction is not implemented.

## Build and run

Run these commands from the Blood2 repository. Game and launcher use the same
pinned dependencies but separate build directories: native dialogs are enabled
only for the launcher. Existing game graphics options and shaders are unchanged.

```sh
cmake --preset release
cmake --build --preset release --target blood2_app --parallel
cmake --preset launcher-release
cmake --build --preset launcher-release --parallel
./build/launcher-release/blood2-launcher --game "$PWD/build/release/blood2"
```

On Windows use the corresponding `.exe` paths from an x64 developer shell, as
described in [setup](getting_started.md#windows-build). Native Windows/macOS
launcher qualification is pending; Linux/NVIDIA results are recorded in the
[validation report](../../Documentation/Implementation/launcher.md).

The default executable location is beside the launcher, for future packaged
distributions. Separate development builds need `--game` or the executable
chooser under Game executable / diagnostics. Use this build's OpenBlood2 game;
the commercial Windows launcher does not implement the capability protocol.

Choose the directory containing `BLOOD2.REZ` or `B2Demo.rez`, select Blood II or
Demo explicitly, then Check installation and Play. Stored selections are checked
when the launcher opens. Remember installation saves the current record; Play
also saves it. Spaces and UTF-8 paths are retained. Original data stays in place.
Manual path entry remains available if a native folder dialog fails.

Play first rechecks the archives and writes a separate bootstrap INI, then opens
the game's normal main menu. It does not select a level or enter diagnostic
play. The launcher hides while the game runs, then returns after exit. Closing
the launcher leaves the game alive. A nonzero game exit is reported with its log
folder; use Open log folder under diagnostics.

## Stored state and validation

`launcher.cfg` lives under `SDL_GetPrefPath("OpenBlood2", "Launcher")`. It stores
the executable, selected installation and up to 32 installation records. Game
audio/mouse/graphics preferences are neither edited nor duplicated. Malformed
or future-version launcher state is reported and retained, not silently reset.
See the [schema and ownership contracts](contracts/launcher_contracts.md).

Each run gets a newly reserved `runs/<id>/bootstrap.ini` and game log directory
under launcher storage. The explicit INI and absolute CLI paths isolate startup
from an unrelated working-directory INI or `BLOOD2_DATA_DIR`. The game's normal
resource decoding and service checks remain authoritative: an indexed archive
does not certify every asset, world or campaign.

Standalone `blood2 --launcher-info` reports the bounded versioned capability
record without SDL initialization, logging or data access. The launcher refuses
unknown/malformed protocols, query failures/timeouts and executables changed
after validation. It never pipes a playable game's output into an unread buffer.

## Development checks

`launcher-debug` and `launcher-sanitize` provide Debug and Clang ASan/UBSan/LSan
builds. The sanitizer preset requires Clang. Register GPU tests explicitly with
`-DBLOOD2_GPU_TESTS=ON`; private checks also need
`BLOOD2_TEST_DATA_DIR`, `BLOOD2_DEMO_DATA_DIR` and
`BLOOD2_LAUNCHER_TEST_GAME` pointing to the separately built current game.
All these paths are local CMake values, not production defaults.

```sh
ctest --preset launcher-debug -L headless
ctest --preset launcher-release -L headless
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset launcher-sanitize -L headless
```

GPU tests exercise scaling, resize and actual Play-widget launch/hide/return with
a synthetic subprocess. Existing private game menu tests verify real retail/demo
rendering separately. No commercial fixtures or captured game artwork are bundled.
The documented full Debug `gpu_registry_tests` link failure and M2-GPU-001 remain
open and separate from the required launcher/game target checks.

Diagnostic CLI options include `--state-dir ABS_PATH`, `--game ABS_PATH`,
`--data-dir ABS_PATH`, `--demo`, `--frames N`, `--capture ABS_PNG` and
`--ui-scale 0.75..3`. The isolated `--smoke` requires a state directory and at
least 12 frames. `--launch-smoke` additionally drives the actual Play widget and
asserts a child exit/return; use it with the test fixture, not an unattended game.
