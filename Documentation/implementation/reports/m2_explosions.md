# M2 — Explosive brushes and original debris requests

> Extended by [m2_pushables.md](m2_pushables.md): selected Pushable/Gravity brushes and
> brush damage impulses now work with bounded box physics. Omissions and counts
> below describe the earlier stage; polygon dynamics and full M2 remain open.

**Status: implemented for the bounded inspection session; M2 remains incomplete.**
This extends [rotating/destructible brushes](m2_brushes.md).

CDestructableBrush now owns the original CDebris aggregate. Explosive destruction
executes original area damage and can schedule later brush explosions. Original
material selection, surface fallback, custom resource requests and launch-vector
rules are inspectable. Debris, explosion visuals and sound remain **requests**;
no fragment renderer or audio playback is claimed.

Read the [contracts](../../contracts/explosion_contracts.md) and
[source ledger](../sources/m2_explosion_sources.json). The source's squared
falloff uses average object dimensions and no visibility test. Its unread authored
ExplodeDamage/DamageRadius properties leave effective values at 100/200. Repeated
SetupClientGibFX calls alternate the velocity sign across materials. These rules
are preserved and tested, not silently corrected.

## Installed-content checks

The isolated catalog still covers 57 readable worlds; `worlds/16_bridge.dat`
remains the known parser failure. No commercial resources are copied.

| Original class | Authored | Created | Rejected | Filtered | Isolated explosive destructions |
| --- | ---: | ---: | ---: | ---: | ---: |
| CDestructableBrush | 2,455 | 2,162 | 293 | 0 | 47 |
| RotatingBrush | 136 | 98 | 38 | 0 | 0 |

Compared with the preceding increment, 47 explosive brushes now create and
execute destruction/damage-request cleanup. The remaining formerly explosive
rejection exposes a missing named model. The 98 rotating brushes still move in
four isolated updates. First rejection conditions for CDestructableBrush are
138 pushable/gravity, 38 SpawnObject, 51 older TypeAction schema, 31 missing model,
30 ambiguous model and 5 invalid shells. RotatingBrush has 38 TypeAction failures.

SubwayA counts are unchanged: **141 created source entries + 7 owned SelfTriggers,
372 editor-only, 67 unported, 0 failed**; 148 runtime objects before player entry,
151 afterward. These are isolated catalog/inspection results, not campaign
playability or full-world collision certification. `--start-game` still exits 6.

## Validation

GCC Debug with Dear ImGui passed **147/147** tests in **61.62 seconds**;
GCC Release without ImGui passed **147/147** in **41.94 seconds**. Each includes
120 synthetic headless, 16 private headless and 11 serial GPU cases. Clang
ASan/UBSan/leak checks passed **136/136 headless tests** in **125.17 seconds**,
without sanitizer diagnostics. No test timeout or dependency pin was changed.

Blood2 commit: `31948ae32f9c6098ff43c7f73b614be9555cfb9e`. The 18 changed C++ files pass
clang-format 23.1.3; code diff whitespace checks pass. The screenshot was visually
checked for the two explosion/four debris records, effective values, pending FX
labels and preserved rotating geometry/state. All 1125 local Markdown targets resolve. All 10,923 inventoried reference
files are unchanged and the six reference repositories are clean.

The eight new synthetic cases cover world/string properties and byte conversion,
surface precedence, custom resources, material velocity alternation and zero
normalization, squared-distance/dimension falloff, zero-damage boundary delivery,
wall-independent/inactive targets, chains, damage gates/armor, callback removal,
new-target exclusion, buffer limits, player/client synchronization and cleanup.
The existing catalog now destroys all 47 accepted explosive entries in isolation.

The expanded `--brushes-smoke` adds two explicitly synthetic explosive brushes
above the external SubwayA level, away from the rotating platform/rider. KILL
runs while the inspector is hidden. The test checks two explosions and four
material requests, preserves rotation/rider checks, captures the Brushes panel,
and verifies unload/reload plus restored object/string counts. The Release smoke
runs the same rules without Dear ImGui. GPU verification is not a retail-world
renderer or original-executable comparison.

## Reproduce

Run inside `Blood2/`, using ignored local presets for this machine's external
content path. Elsewhere configure BLOOD2_TEST_DATA_DIR explicitly.

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

Use `ctest --test-dir build/debug -R 'explosions|brushes' --output-on-failure`
for focused checks. Raw logs, source integrity, screenshot, tracked-file hashes
and a reverse-checked adaptation patch are linked by
[evidence/m2-explosions.json](evidence/m2-explosions.json).

## Remaining gate

WorldModelDebris fragments, pushable/gravity brushes, impulses, SpawnObject,
other explosion classes, client FX/rendering/audio, other movers and general
pickups remain pending. So do complete player/AI/combat, world transitions,
portals and save/restore. The [M2 exit gate](../../history/Roadmap/03_milestones.md#m2--connect-world-session-and-gameplay-services)
is unchanged; other platforms, XR and retail behavior remain unverified.
