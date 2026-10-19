# OpenBlood 2 Launcher

A compact SDL3/SDL_GPU + Dear ImGui desktop launcher. The main window has Play,
Game Settings, Mods and Quit. Play is unavailable until a configured game and
its data have passed validation. Original artwork is read, without executing
code, from the user's `BLOOD2.EXE` or `B2Demo.exe`; otherwise a plain title is
shown. No commercial artwork is included in the build.

## Using it

In **Game Settings**, browse to the folder containing `BLOOD2.REZ` or
`B2Demo.rez`. Demo is selected automatically when only its base archive is
present; an explicit edition selector handles mixed folders. OK remembers the
path and returns to the main window. Clear the path and press OK to unconfigure
it. Manual entry remains available. Advanced contains the OpenBlood 2 executable
path for unusual development layouts.

**Download Demo** retrieves the qualified 36 MB [Archive.org package](https://archive.org/details/B2demo),
checks its exact size and SHA-256, extracts its `Game/` contents into a new
user-owned directory and selects that path automatically. Music, Readme and
original files are retained. The old Windows installer is never run. **Install
from ZIP** accepts that same package from disk. Downloading/installing shows
progress and supports cancellation; a failed operation leaves the current
installation selected. ModDB's different `b2demoa.zip` is not this qualified
package and is explicitly rejected, rather than installed as the tested v1.0.

**Mods** lists local REZ archives associated with the current folder and edition.
Add REZ registers an initially disabled archive; check/uncheck it to enable or
disable it. Later enabled entries override earlier exact resource paths. Remove
unregisters an entry without deleting its file. The game is revalidated after
changes and before Play. This supports data overlays using the existing game
code; code mods and Nightmares are unavailable. A readable REZ does not certify
a particular mod's levels, gameplay or campaign compatibility. No graphical
settings panel is provided; the game's own options remain authoritative.

Play opens the game's normal main menu. The launcher hides while the game runs,
then returns after exit. Errors include a link to the game logs. The graphical
stack, shader artifacts and original gameplay rules are unchanged by the launcher.

## Build and run

Install OpenSSL development headers/libraries in addition to the game's build
requirements. curl and miniz are fetched at pinned revisions, only in launcher
builds; Droid Sans comes from the already pinned ImGui archive. OpenSSL is
provided by the platform/development installation and retains its own notices.

```sh
cmake --preset release
cmake --build --preset release --parallel
./build/release/blood2-launcher
```

The `debug`, `dev` and `release` presets build both executables by default:
`blood2-launcher` beside `blood2`, with dependency notices in `licenses/`.
Explicitly building `--target blood2_app` also builds the launcher. Native dialogs
and HTTP/ZIP dependencies remain isolated in the `build/<preset>/launcher/`
child CMake tree; the game's SDL options and graphics stack are unchanged.
The child uses the parent compiler, build configuration and sanitizer setting;
its incremental build uses at most two jobs alongside the parent build.
Reconfigure an existing preset to enable this default. To build only the game,
configure with `-DBLOOD2_BUILD_LAUNCHER=OFF`.

By default the game executable is `blood2` (`blood2.exe` on Windows) in the
launcher's own directory, independent of the current working directory. A saved
Advanced path or `--game ABS_PATH` overrides that default. There is no search in
neighboring build/preset folders. Window titles are **OpenBlood 2 Launcher** and
**OpenBlood 2**; internal protocol and preference-directory identifiers remain
compatible with existing state.

The `launcher-debug`, `launcher-release` and `launcher-sanitize` presets remain
available for independent launcher development and tests. With those layouts,
pass `--game /absolute/path/to/blood2` or configure the Advanced path. For example:

```sh
cmake --preset launcher-release
cmake --build --preset launcher-release --parallel
./build/launcher-release/blood2-launcher --game "$PWD/build/release/blood2"
```

Windows uses `.exe` paths and an x64 developer shell. Windows/macOS launcher
execution and native chooser qualification remain pending. The tested platform
is Linux/NVIDIA; see the [default build validation](../../Documentation/Implementation/launcher_default_build.md)
and [feature validation](../../Documentation/Implementation/launcher_redesign.md).

## Storage and startup

Launcher state lives under `SDL_GetPrefPath("OpenBlood2", "Launcher")`:

- `launcher.cfg`: executable and selected installation; existing version-1 files remain readable.
- `mods.cfg`: bounded version-1 list, edition/root association and enable flags.
- `content/Blood2Demo-1.0-<id>/Game/`: published demo data; the original ZIP is retained alongside `Game/`.
- `runs/<id>/`: explicit bootstrap INI and game logs.

New installation directories are published only after complete extraction and
validation. They never replace an existing installation. A cancelled/failed
operation cleans its own staging directory; an OS/process crash can leave an
unreferenced `.demo-install-*` directory. It is never selected or launched.
Storage failures do not change persisted selection. Malformed/future state is
reported and retained. Game audio/mouse/graphics preferences are not edited.

`blood2 --launcher-info` advertises `profiles=rez1`; the launcher also accepts
older `profiles=0` games when no mods are enabled. Each enabled mod becomes an
absolute `--rez` argument, without a shell. Startup revalidates and mounts those
archives after stock data; existing menu/session resource ownership carries the
same index through world changes. See the [contracts](contracts/launcher_contracts.md).

## Checks

```sh
ctest --preset launcher-debug -L headless
ctest --preset launcher-release -L headless
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset launcher-sanitize -L headless
```

Enable GPU tests explicitly with `-DBLOOD2_GPU_TESTS=ON`. Private checks use
`BLOOD2_TEST_DATA_DIR`, `BLOOD2_DEMO_DATA_DIR` and `BLOOD2_LAUNCHER_TEST_GAME`.
Headless tests do not download anything. For an explicit, isolated delivery test:

```sh
./build/launcher-release/tests/launcher/launcher_delivery_tests install /absolute/test/storage /absolute/B2Demo.zip
./build/launcher-release/tests/launcher/launcher_delivery_tests download /absolute/test/storage
```

UI diagnostics: `--state-dir`, `--game`, `--data-dir`, `--demo`, `--frames`,
`--capture`, `--ui-scale`, `--panel main|settings|mods`. `--smoke` requires isolated
state and at least 12 frames. `--launch-smoke` drives Play with a test executable (explicit or beside the launcher);
`--navigation-smoke` drives the menu and both panels (use at least 24 frames).
`--install-smoke ABS_ZIP` drives Install from ZIP with an injected native-chooser
reply (use a test executable and at least 160 frames). These checks use the real
widgets/controller/installer; native chooser interaction still needs manual
platform qualification. No commercial fixtures or captures are distributed.
