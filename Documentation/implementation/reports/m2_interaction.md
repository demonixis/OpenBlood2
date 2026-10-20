# M2 — Original USE and RotatingDoor continuation

> Continued by [m2_brushes.md](m2_brushes.md). Counts and test totals below
> describe the USE/rotating-door increment.


**Status: implemented and tested for the bounded Linux inspection session;
M2 remains incomplete.** This extends the [trigger/key increment](m2_triggers.md)
and [sliding-door increment](m2_doors.md).

## Delivered

- Original player 82-unit USE ray, current eye/aim, nearest-object occlusion,
  direct Door/RotatingDoor exclusion and original MID_TRIGGER delivery.
- Checked typed local shell dispatch and E press-edge input, gated by F2,
  focus and text editing; simulation continues with F1 hiding the inspector.
- Original RotatingDoor properties, angular rules, pivot calculation, shared
  Door state machine, stationary owned SelfTrigger and consuming key relay.
- Posed world-model geometry/BVH and bounded atomic rotation/body transport,
  block/retry, current ray/support/clearance queries and deferred sensor contacts.
- Original opening-door player push selection, consumed once by the local
  movement path. Session and Doors expose interaction and rotation diagnostics.

Read [interaction_contracts.md](../../contracts/interaction_contracts.md) for
precise semantics and bounds, and [source selections](../sources/m2_interaction_sources.json)
for provenance. Original trees and commercial content remain external/unchanged.

## Preservation findings

Original `RotatingDoor::CalcAngle` advances twice per update, can overshoot for
one update and has an asymmetric one-degree direction comparison. These were
retained. Zero-span division is guarded. Original StartOpen does not reverse the
angular endpoints or initially rotate the door; the parent's positional branch
is retained. Speed and closing speed are radians/s after original degree conversion.

USE does not directly open a door. It must hit its SelfTrigger or another
eligible object, and an intervening target blocks the ray. Original trigger/key
rules remain in force. The later LithTech reference informs non-solid/interior
ray filtering, but retail executable ray parity has not been demonstrated.

The collision solver is a replacement, with conservative envelopes, bounded
AABB separation and explicit work limits. It commits geometry and transported
bodies together, then dispatches callbacks. It replaces the original engine's
force/crush/rollback protocol; no retail physics equivalence is claimed.

## Installed-content evidence

The read-only catalog examines RotatingDoor entries across **57 readable worlds**;
`worlds/16_bridge.dat` remains the previously known unreadable file.

| Measure | Count |
| --- | ---: |
| Authored RotatingDoor entries | 235 |
| Created | 223 |
| Owned SelfTriggers | 155 |
| Original game-mode filtering | 0 |
| Rejected with diagnostics | 12 |
| Instances recording motion in four isolated updates | 215 |

The catalog instantiates each door in isolation, triggers it and checks cleanup;
it does not establish campaign connectivity or full-world collision compatibility.
The 12 rejections comprise **7 missing models, 1 ambiguous model name and
4 unsupported older multiplayer TypeAction properties**. See the raw catalog
log for each entry.

SubwayA counts are unchanged: **38 created source entries + 7 owned children**,
**372 editor-only / 170 unported / 0 failed**; 45 runtime objects before entry and
48 after player/muzzle-light/scent creation. It contains no newly ported
RotatingDoor source entries. The M2 startup gate still rejects complete gameplay.

## Validation

| Configuration | Passed |
| --- | ---: |
| Linux GCC Debug / Dear ImGui | 130/130 |
| Linux GCC Release / inspector OFF | 130/130 |
| Clang ASan + UBSan + leak detection / headless | 120/120 |

The full suite contains 105 synthetic headless, 15 private headless and 10 GPU
cases. GPU tests ran serially, with no overlap between build directories.
The final sanitizer suite ran at `-j2` after builds, with no sanitizer diagnostic.
All 32 changed C++ files pass clang-format; code/prose whitespace checks pass.
All 10,923 inventoried reference files match their hashes, and all six reference
repositories remain clean. Dependency pins are unchanged.

- [Debug log](evidence/m2-interaction-debug-tests.log)
- [Release log](evidence/m2-interaction-release-tests.log)
- [Sanitizer log](evidence/m2-interaction-sanitize-tests.log)
- [Installed-door catalog](evidence/m2-interaction-catalog.log)
- [GPU scenario log](evidence/m2-interaction-gpu.log)
- [Inspector capture](evidence/m2-interaction-inspector.png)
- [Reference integrity](evidence/m2-interaction-source-integrity.json)

Implementation commit: `5e4b866` in the independent Blood2 repository.

The new synthetic cases cover USE distance/aim/eye level/nearest occlusion,
interior sensor filtering, stale association, input edges/focus/text gating,
key denial/consumption through SelfTrigger, direct-door refusal, source angular
arithmetic, StartOpen, multi-axis and authored transforms, bounds/rays, work
limits, riders/DONTFOLLOWSTANDING, ordinary pushes, wall-blocked atomic rollback,
removal during transported contact, and the once-consumed original player push.

The GPU scenario combines real Caleb/weapon resources with an explicitly
synthetic hinge above SubwayA. It first checks missing-key denial, grants a
fixture key, then injects SDL E down/up, verifies exactly one new command while
held, observes unlock and rotation with hidden UI, captures the Doors view,
unloads/re-enters and checks object/string counts. It is separate from the
retail door catalog. The inspector-free Release runs the same simulation checks.

Final commands, results and hashes are recorded in the
[evidence ledger](evidence/m2-interaction.json). Sanitizers cover headless project
code, not SDL_GPU or its driver. Linux is the only platform exercised; runtime
rendering remains diagnostic geometry, not the finished game's renderer.

```sh
cd Blood2
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
./build/debug/blood2 --interaction-smoke --capture build/interaction.png
```

## Remaining gate

Complete session/player/AI behavior, remaining mover/world classes, general
pickups, FX/audio, visibility portals, persistence and world transitions are
still missing. Broader collision fidelity and performance need retail comparison
and profiling. M3 and the playable slice have not started.
