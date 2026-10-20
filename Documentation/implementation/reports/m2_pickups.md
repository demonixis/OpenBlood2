# M2 — Original pickups and destruction spawns

**Implemented for the bounded inspection session; M2 remains incomplete.**
This extends the [pushable increment](m2_pushables.md) with twelve original
pickup classes and CDestructable's SpawnObject path. Read the
[contracts](../../contracts/pickup_contracts.md) and
[source ledger](../sources/m2_pickup_sources.json).

## Behavior

Eight AmmoPickup classes now feed the original CInventoryMgr ammo pools.
HealthPU, MegaHealthPU, WardPU and NecroWardPU use the original normal/bonus
health and armor caps. Full reserves leave the item available, partial additions
clip to the cap, and dead players cannot collect. Successful typed replies hide
the pickup, send its authored trigger and defer removal. Model dimensions,
world ground placement, source amounts and sound resource names are preserved.
Sound requests and resource IDs are inspectable; audio/localized HUD output is
still pending.

Original CDestructable sends the death trigger before SpawnObject. The new object
inherits the current transform unless its string properties override it, then
receives SpawnObjectVel after initialization. It survives parent removal.
Unsupported spawn classes reject the brush; removal during the death callback
cancels the spawn safely. Missing pickup models stay visible as diagnostics with
contact/gravity disabled. Typed messages, bounded reports, static list cleanup
and generation-checked handles preserve lifetime across reloads.

The Pickups tab exposes objects, hidden/inactive state, resources, original
values, player pools and recent consumption/spawn results. The added smoke test
collects ammo with inspection hidden, keeps a Ward available for inspection,
and verifies unload/reload and stale selection. The same simulation runs in
Release without ImGui. No dependency revision or reference source changed.

## Installed-content evidence

The catalog checks selected classes in isolation across **57 readable worlds**;
`worlds/16_bridge.dat` remains the known reader failure. This is not campaign or
full-world gameplay coverage.

| Class | Authored entries |
| --- | ---: |
| BulletAmmoPU | 243 |
| ShellAmmoPU | 186 |
| BMGAmmoPU | 76 |
| FlareAmmoPU | 117 |
| DieBugDieAmmoPU | 105 |
| HowitzerAmmoPU | 100 |
| FuelAmmoPU | 108 |
| BatteryAmmoPU | 150 |
| HealthPU | 209 |
| MegaHealthPU | 49 |
| WardPU | 58 |
| NecroWardPU | 33 |

Of **1,434 authored pickups**, **1,258 create with a decoded model**, and **176
reject the older TypeAction schema**. The twelve selected classes cover **60 of
72 brush SpawnObject requests**. The remaining twelve request MedKitPU (6),
BinocularsPU (1), FlashLightPU/FlashlightPU (2), InvulnerabilityPU (1), TheAngerPU
(1) and RocketAmmoPU (1); these remain unsupported.

CDestructableBrush creation rises from 2,237 to **2,295 of 2,455**. All **58 newly
accepted brushes** spawn their expected modeled pickup with its authored velocity
when killed in isolation. Two other supported requests expose pre-existing
missing/ambiguous geometry. First rejection reasons are now: 12 unsupported
spawns, 28 polygon dynamics, 51 TypeAction, 32 missing models, 32 ambiguous models
and 5 invalid shells. There are 103 accepted dynamic brushes, 37 reporting motion
after four isolated updates and 2 reporting blocking. All 50 explosive brushes
still detonate; all 98 accepted RotatingBrush entries still move (38 TypeAction
rejections). Counts describe first failures, not all defects of an entry.

SubwayA is unchanged: **141 created source entries + 7 owned SelfTriggers**, 372
editor-only, 67 unported and 0 failed; 148 runtime objects before player entry and
151 afterward. `--start-game` still exits 6. Other movers, full player/AI/combat,
polygon physics/stacks, remaining pickups, FX/audio, save/restore and transitions
remain outside the completed subset.

## Validation

GCC Debug with Dear ImGui passed **168/168** tests in **63.70 seconds**;
GCC Release without ImGui passed **168/168** in **11.66 seconds**.
Each suite includes 139 synthetic headless, 17 private headless and 12 serial GPU
cases. Clang ASan/UBSan/leak checks passed **156/156 headless tests** in
**144.60 seconds**, without sanitizer findings.

After the final UI fixture/rotation-metadata adjustments, the complete pickup
subset passed again: **11/11 Debug, 11/11 Release and 10/10 sanitized**. The
updated brush catalog assertion also passed. No timeout or dependency pin changed.
The final screenshot was visually inspected; 34 changed C++ files pass the formatter.
All 1,178 local Markdown targets resolve.
All 10,923 inventoried reference files are unchanged and all six reference
repositories are clean. Blood2 commit: `761cfeb004ea01d2bdd1985c886e79f08b923d6e`.

Nine new synthetic cases cover original defaults/ancestry/list cleanup; all eight
ammo capacities; normal/bonus health/armor and dead-player guards; malformed and
unsolicited typed replies; actual MoveMgr contact; destruction order, transform,
velocity and child lifetime; unsupported requests and rollback; callback removal
and reentrancy; missing resources and bounded histories. A private pickup catalog
and the expanded brush catalog check installed resources and actual death spawns.
The new serial GPU case covers the inspector and hidden collection.

Inside Blood2, reproduce with its local Debug/Release presets and:

```sh
ctest --test-dir build/debug -R 'pickups|brushes.catalog' --output-on-failure
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DBLOOD2_INSPECTOR=OFF -DBLOOD2_SANITIZERS=ON -DBLOOD2_GPU_TESTS=OFF \
  -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build build/sanitize -j3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure -j2
```

[Machine-readable evidence](evidence/m2-pickups.json) records revisions, tests,
artifact hashes and source integrity. Source/data inspection was performed on
Linux; there was no retail executable comparison, additional-platform build or
XR session. Original commercial assets remain outside the repositories.
