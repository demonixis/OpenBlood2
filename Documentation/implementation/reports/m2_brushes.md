# M2 — Rotating and destructible brush continuation

> Extended by [m2_pushables.md](m2_pushables.md): selected Pushable/Gravity brushes and
> brush damage impulses now work with bounded box physics. Omissions and counts
> below describe the earlier stage; polygon dynamics and full M2 remain open.

> Extended by [m2_explosions.md](m2_explosions.md): original CDebris requests and explosive
> brush area damage are now connected. Earlier omissions/counts below are historical;
> WorldModelDebris, pushable brushes, client FX/audio and the full M2 gate remain open.

**Status: implemented for the bounded inspection session; M2 remains incomplete.**
This extends [original USE / rotating doors](m2_interaction.md).

The increment connects original RotatingBrush, its Rotating aggregate and selected
CDestructableBrush lifecycle/damage rules. Continuous rotation, per-axis spin-up
and spin-down, ON/OFF/TOGGLE, hidden/inactive state, atomic collision and rider
transport are inspectable. Shared CDestructable now also delivers original named
damage/death triggers and KILL, with deferred brush removal and owned diagnostics.
Read the [contracts](../../contracts/brush_contracts.md) and
[source ledger](../sources/m2_brush_sources.json) for the exact selection.

The source's zero-based first rotation, elapsed-time restart and asymmetric
ON/OFF state guards are preserved. The existing replacement rotation solver now
accepts rotation about the object's own position without Door's GOTHRUWORLD flag.
It retains explicit work limits and atomic blocking. This is not recovered retail
physics. Source code was compared; executable behavior remains unverified.

## Installed-content checks

The read-only catalog checks isolated creation, four updates and teardown in
57 readable worlds. The known `worlds/16_bridge.dat` parser failure remains.

| Original class | Authored | Created | Rejected | Filtered | Recorded rotation |
| --- | ---: | ---: | ---: | ---: | ---: |
| CDestructableBrush | 2,455 | 2,115 | 340 | 0 | 0 |
| RotatingBrush | 136 | 98 | 38 | 0 | 98 |

RotatingBrush rejections are 38 older multiplayer TypeAction schemas. Static
brush rejections are:

| First rejecting condition | Count |
| --- | ---: |
| Pushable/gravity gameplay | 138 |
| Exploding gameplay | 48 |
| SpawnObject gameplay | 38 |
| Older TypeAction schema | 51 |
| Missing model | 30 |
| Ambiguous model name | 30 |
| Invalid closed shell | 5 |

Conditions can coexist; the table groups the first rejection. The raw catalog records each input;
these are not campaign-playability results or full-world physics certification.

SubwayA now has **141 created source entries + 7 owned SelfTriggers**, **372
editor-only / 67 unported / 0 failed**, with **148 runtime objects before player
entry and 151 afterward**. This adds 103 static destructible brushes to the
preceding increment. `--start-game` still exits 6 with the remaining M2 blockers.
Rejected precreate entries elsewhere are failed, not difficulty-filtered.

## Validation

GCC Debug with Dear ImGui and GCC Release without the inspector each passed
**139/139 tests**: 112 synthetic headless, 16 private headless and 11 serial GPU
cases. Debug took 61.55 seconds and Release 41.84 seconds on this Linux machine.
The Brushes screenshot was visually checked for posed geometry, state/units,
buttons, rider counters and clearly labelled omissions.

The final Clang ASan/UBSan/leak suite passed **128/128 headless tests** in
**124.69 seconds**, with no sanitizer diagnostic.

The first sanitizer run passed 127 cases and timed out `weapons.private` at its
old 30-second limit. An isolated ASan/UBSan/leak run completed all three reloads
and 4,140 simulation frames in **45.07 seconds**, without diagnostics. Its timeout
is now 90 seconds, matching the private catalog budget; assertions and simulated
frames were retained. Both the initial failure and the successful isolated run
are preserved in the evidence. This is sanitizer timing, not a rendering or
retail-performance measurement.

Blood2 commit: `aed07ee16b37edc75cdabd029ae23a6cdea3326d`.
All **1103 local Markdown targets** resolve. Validation results and file hashes are recorded in
[evidence/m2-brushes.json](evidence/m2-brushes.json). The new synthetic cases cover
command/timer rules, per-axis ramps, authored poses/rays, invalid and over-budget
requests, rider/follow-flag boundaries, atomic blocking/retry, ordinary and
trigger-only damage, named triggers, reentrancy and deferred removal.

The GPU smoke loads the external SubwayA resources and creates an explicitly
synthetic rotating platform/rider above the level. It checks ON/OFF ramps,
rotation/transport while the inspector is hidden, the Brushes tab, resize,
unload/reload, stale selection and restored object/string counts. It does not
claim a rendered retail world or copy commercial geometry into the repository.

## Reproduce

Run inside `Blood2/`; local presets point to the external installation.

```sh
cmake --preset local-debug
cmake --build --preset debug -j4
ctest --preset debug --output-on-failure -j4
cmake --preset local-release
cmake --build --preset release -j4
ctest --preset release --output-on-failure -j4
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DBLOOD2_INSPECTOR=OFF -DBLOOD2_SANITIZERS=ON -DBLOOD2_GPU_TESTS=OFF \
  -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build build/sanitize -j3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure -j2
```

SDL/ImGui/spdlog pins are unchanged. The build metadata and commands are in the
[evidence ledger](evidence/m2-brushes.json), alongside raw logs and a reverse-checked
adaptation patch. Source integrity verification found **10,923 unchanged reference
files and six clean reference repositories**. No commercial files were added.

## Remaining gate

Pushable/gravity brushes, explosive brushes and SpawnObject reject creation.
Debris properties and destruction requests are retained, but CDebris rendering,
WorldModelDebris, material fallback/decals and audio are pending. Other movers,
models/sprites, complete player/AI/combat, general pickups, world transitions and
save/restore still block M2. Other platforms and XR remain unverified.

The next bounded task should extend required world classes and services from the
original sources, using the reduced unsupported-entry inventory; keep the
[unchanged M2 exit gate](../../history/Roadmap/03_milestones.md#m2--connect-world-session-and-gameplay-services).
