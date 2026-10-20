# Launcher

The default build places `blood2-launcher` beside `blood2`:

```sh
./build/dev/blood2-launcher
```

## Use

- **Game Settings:** select the folder containing `BLOOD2.REZ` or `B2Demo.rez`. Mixed folders have an edition selector. Advanced overrides the game executable.
- **Play:** validate content, open the normal game menu, hide the launcher and return after game exit.
- **Download Demo / Install from ZIP:** install the qualified Archive.org B2demo package into a new user directory. Size/hash checks, progress and cancellation are supported; the Windows installer is never run. ModDB's different ZIP is rejected.
- **Mods:** register local REZ archives and enable/disable overlays; later enabled entries override earlier exact paths. Removing an entry keeps its file. Code mods and Nightmares are unavailable.

Game options remain in the game. Mod readability does not certify gameplay or
campaign compatibility. Errors link to logs; invalid installs cannot be launched.

## Build and checks

Requirements and normal presets: [build guide](getting_started.md). OpenSSL
headers/libraries are required; pinned curl/miniz are fetched for the launcher.
Its child build isolates native-dialog SDL options from the game's SDL build.
`-DBLOOD2_BUILD_LAUNCHER=OFF` builds the game alone.

Standalone development:

```sh
cmake --preset launcher-release
cmake --build --preset launcher-release --parallel
./build/launcher-release/blood2-launcher --game "$PWD/build/dev/blood2"
```

## Checks

```sh
ctest --preset launcher-release -L headless
```

`launcher-debug` and `launcher-sanitize` presets are also available. GPU tests
require `BLOOD2_GPU_TESTS=ON`; private checks use `BLOOD2_TEST_DATA_DIR`,
`BLOOD2_DEMO_DATA_DIR` and `BLOOD2_LAUNCHER_TEST_GAME`. Headless checks do not download.
Linux/NVIDIA is tested; native Windows/macOS launcher qualification is pending.

## Storage and protocol

State uses `SDL_GetPrefPath("OpenBlood2", "Launcher")`: `launcher.cfg`, `mods.cfg`,
`content/` for demo installs and `runs/` for bootstrap configs/logs. Failed or
cancelled installs keep the selected installation; crashes may leave unreferenced
`.demo-install-*` staging directories. Game preferences are independent.

The sibling game path is the default; `--game ABS_PATH` overrides it. Startup uses
`--launcher-info` and absolute `--rez` arguments without a shell. See
[contracts](contracts/launcher_contracts.md) and [validation](implementation/reports/launcher_redesign.md).
