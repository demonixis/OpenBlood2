# M2 — Original pushable brushes and bounded box physics

**Implemented for the bounded inspection session; M2 remains incomplete.** This
extends the [explosion increment](m2_explosions.md). CDestructableBrush now supports
selected Pushable/Gravity objects using original flags, mass/friction, damage
impulses and player blocking priorities. Engine movement uses a new bounded box
solver. Read the [contracts](../../contracts/pushable_contracts.md) and
[source ledger](../sources/m2_pushable_sources.json) for the distinction.

## Behavior

Pushable forces gravity, disables marks, adds MOVEABLE, uses priority 40 and
enables original damage impulses. Gravity-only brushes retain priority 100 and
no damage impulses. CPlayerObj priority 50 while grounded lets original MoveMgr
push a box; jumping changes it to 40. The player continues to integrate once.
Mass 750 deliberately produces friction 20 because the source formula uses its
unclamped member. Damage impulses preserve the 2500/mass factor, direction
threshold, resistance/armor order and nonpositive-mass fallback.

The engine sweeps actual geometry bounds with correct authored pivot offsets,
slides at walls, applies gravity/ground drag and connects existing container
physics/damage. It checks box/actor/rider paths before committing positions and
then dispatches deferred contacts. Failed moves, hidden/inactive objects, mass,
friction, priority, support, velocity and damage impulses are inspectable.

BoxPhysics is mandatory. No claim of retail physics fidelity is made. The solver
has four slide legs, at most 64 ordinary riders, conservative horizontal pushing,
no recursive chains/stacks, no crushing and no penetration recovery. Original
DONTFOLLOWSTANDING remains active. Polygon dynamics, rotating+gravity combinations
and SpawnObject reject rather than silently omitting their gameplay.

## Installed-content evidence

57 readable worlds are checked in isolation; `worlds/16_bridge.dat` remains the
known reader failure. External commercial resources are not copied.

| Class | Authored | Created | Rejected | Filtered | Explosive destructions |
| --- | ---: | ---: | ---: | ---: | ---: |
| CDestructableBrush | 2,455 | 2,237 | 218 | 0 | 50 |
| RotatingBrush | 136 | 98 | 38 | 0 | 0 |

There are **75 newly accepted dynamic brushes**. After four isolated 0.02-second
updates, 29 report movement and 2 report collision blocking. This is short,
isolated observation, not proof that all authored crates move correctly in a
complete level. All 98 rotating brushes still move. Three additional explosive
brushes bring isolated checked destructions to 50.

First rejection reasons are 28 unsupported polygon dynamics, 72 SpawnObject,
51 old TypeAction schema, 31 missing model, 31 ambiguous model and 5 invalid shell.
The prior 138 combined pushable/gravity failures split into 75 accepted, 28
polygon dynamics, 34 SpawnObject and one newly exposed ambiguous model. Counts
are first failures, not a claim that every rejected object has only one defect.
RotatingBrush still has 38 incompatible TypeAction entries.

SubwayA stays at **141 created source entries + 7 owned SelfTriggers, 372
editor-only, 67 unported and 0 failed**; 148 runtime objects before entry, 151
afterward. `--start-game` still exits 6. Full player/AI/gameplay is incomplete.

## Validation

GCC Debug with Dear ImGui passed **157/157** tests in **64.45 seconds**;
GCC Release without ImGui passed **157/157** in **42.32 seconds**. Each includes
130 synthetic headless, 16 private headless and 11 serial GPU cases. Clang
ASan/UBSan/leak checks passed **146/146 headless tests** in **130.80 seconds**,
without sanitizer diagnostics. No test timeout or dependency pin was changed.

Blood2 commit: `507a931b1dc1310e5b2c246a353a06627494f156`. All 22 changed C++ files pass clang-format
23.1.3, and diff whitespace checks pass. The screenshot was visually checked; all 1150 local Markdown targets resolve.
All 10,923 inventoried reference files are unchanged and all six reference
repositories are clean.

Ten new synthetic cases cover flags/services/rejections, off-center pivots,
gravity/support/inactivity, analytic drag, original impulse gates and zero mass,
priority pushing and blocked chains, real MoveMgr continuous pushing/jumping,
riders/detachment/64-rider budget, callback atomicity/removal/stale handles,
fast wall sliding/initial overlap, and liquid/environmental damage membership.

The expanded `--brushes-smoke` adds a synthetic dynamic brush above external
SubwayA alongside the existing rotating platform/rider and explosive pair. It
receives a real typed damage message, falls and moves while the inspector is
hidden. Existing explosion/rotation checks remain; capture shows dynamic body
inspection. Unload/reload restores object/string counts and clears stale IDs.
Release runs the same gameplay checks without Dear ImGui.

## Reproduce

Inside Blood2, run the local Debug/Release configure/build/CTest presets described
in its [README](../../../README.md). For focused verification:

```sh
ctest --test-dir build/debug -R 'pushables|brushes|explosions' --output-on-failure
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DBLOOD2_INSPECTOR=OFF -DBLOOD2_SANITIZERS=ON -DBLOOD2_GPU_TESTS=OFF \
  -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build build/sanitize -j3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure -j2
```

Raw logs, source integrity, screenshot, file hashes and the reverse-checked patch
are indexed by [evidence/m2-pushables.json](evidence/m2-pushables.json).

## Next boundary

Address polygon dynamics and SpawnObject/pickup ownership before claiming general
crate support. WorldModelDebris, player impulses, other movers, client FX/audio,
complete player/AI/combat, portals, save/restore and world transitions remain
pending. The [M2 exit gate](../../history/Roadmap/03_milestones.md#m2--connect-world-session-and-gameplay-services)
is unchanged. Retail executable behavior, other platforms and XR are unverified.
