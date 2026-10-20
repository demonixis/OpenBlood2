# Crash capture rebase resolution — 2026-10-18

Completed the interrupted Blood2 rebase onto `bec1146`. The conflict in
`src/app/main.cpp` now retains both crash-capture initialization and the demo flag
in startup logging. The crash-capture change is `62513e4`; the following CityHub
limitation documentation was replayed as `73ada5e`.

Validation exposed an existing missing `<algorithm>` include in
`src/app/scene_presenter.cpp`, required by initializer-list min/max and stable_sort.
A separate compilation fix adds that include without changing rendering behavior.

For both `debug` and `release`, ran:

```sh
cmake --build --preset PRESET --parallel 4 --target blood2_app config_tests logging_tests crash_capture_tests
ctest --preset PRESET -R '^(config|logging|crash|cli)\.' --output-on-failure --parallel 4
```

Both builds passed; each configuration passed all 37 selected tests. Release has
the inspector disabled. `git diff --check` passed. No interactive gameplay or GPU
validation was performed; M2-GPU-001 and milestone acceptance remain unchanged.
