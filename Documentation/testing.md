# Tests

Run from `Blood2/` after [building](getting_started.md). Choose relevant checks;
report headless, private-data and GPU results separately. Documentation-only
changes need link/path checks, not a game rebuild.

## Headless

```sh
ctest --preset dev -L headless
```

## Private data and GPU

```sh
cmake --preset dev -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II" -DBLOOD2_GPU_TESTS=ON
cmake --build --preset dev --parallel
ctest --preset dev -L headless
ctest --preset dev -L gpu
```

CTest gives every test its own `BLOOD2_USER_DIR`, `build/dev/tests/user-dirs/<test>/`
(`tests/launcher/user-dirs/` for launcher tests), so no test reads or writes the real
preferences, `controls.cfg`, saves or logs in the user directory
([`blood2_isolate_test_user_directories`](../cmake/ProjectOptions.cmake), called by each test
directory). These directories persist between runs; delete them for a fresh profile. Running a
test executable or `blood2` directly uses the real directory unless you set the variable.

GPU checks require a real display; avoid concurrent interactive sessions and
verify the selected GPU ([Linux selection](getting_started.md#platforms)).
Captures go under `build/dev/tests/`. Demo checks use label `demo` and require
`-DBLOOD2_DEMO_DATA_DIR="/path/to/Blood2 Demo"`.
Application/UI lifetime changes also need an inspector-free `release` build and
relevant tests. Launcher checks are described in [launcher.md](launcher.md#checks).

## Sanitizers

```sh
cmake -S . -B build/sanitize -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DBLOOD2_SANITIZERS=ON -DBLOOD2_INSPECTOR=OFF \
  -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build build/sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure
```

[M2-GPU-001](implementation/reports/m2_acceptance.md#reservation-m2-gpu-001)
remains an unsuppressed NVIDIA/D-Bus GPU LeakSanitizer failure. Passing gameplay
assertions does not make a failing sanitizer process pass.

## Session startup

```sh
./build/dev/blood2 --data-dir "/path/to/Blood II" --start-game
```

This checks original shell/player entry, one update and cleanup. Missing required
classes/resources/services produce a nonzero exit. `--view-world` uses the same
entry validation. See [startup contracts](contracts/session_start_contracts.md).

## Performance

```sh
./build/dev/blood2 --data-dir "/path/to/Blood II" --world 02_CityHub --bench-size 3440x1440 --frames 600
./build/dev/blood2 --data-dir "/path/to/Blood II" --world 02_CityHub --bench-sweep
```

The benchmark uses the presentation path without a window or mouse capture.
Add `--capture FILE.png`, `--no-model-shadows` or `--pixel-lighting` as needed.
Compare simulation/presentation/GPU times and draw/pass/culling counters; see
[render performance](contracts/render_performance_contracts.md).
