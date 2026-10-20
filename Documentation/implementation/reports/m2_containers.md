# M2 continuation — Containers, liquids and ladders

> Historical increment: [m2_specialized_volumes.md](m2_specialized_volumes.md) now
> adds Conveyor, Damage, FreeFall and initial player damage protection. Its new
> validation and source-entry counts supersede the corresponding limits below.

The clean `Blood2/` project now connects selected original **VolumeBrush, Water,
Blood, Acid and Ladder** code to the shared original player and MoveMgr. This
continues [m2_locomotion.md](m2_locomotion.md). M2 remains in progress: complete
original gameplay and its exit gate are still incomplete.

## Implemented

- Original container properties and defaults, named DAT world-model binding,
  ON/OFF/TOGGLE, current, viscosity, liquid gravity and periodic volume damage.
- Separate body/head membership, four tracked body volumes, hidden-volume
  behavior, original swimming/surface-exit rules, pitch-relative motion and
  ladder ascent/descent. Existing collision, crouch and animation paths remain.
- Original air depletion/recovery and suffocation, plus selected ordinary and
  instant-death damage branches. Local messages and callback lifetime stay typed
  and owned; player physics is integrated once.
- Closed convex/non-convex volume geometry with bounded validation and exact
  triangle boundary queries. New source metadata preserves the DAT base ring
  separately from extra indices. The unavailable original BSP traversal is not
  simulated with bounding boxes.
- A Dear ImGui Containers tab for volume geometry/properties, active and hidden
  state, body/head contents, current/gravity, movement mode and air. Queued trigger
  and checked position edits support inspection. Simulation remains independent
  of ImGui and works in the inspector-free build.

[container_contracts.md](../../contracts/container_contracts.md) details source
rules, numeric limits, ordering/timing adaptations and missing services.
[m2_container_sources.json](../sources/m2_container_sources.json) records
source selections and hashes. Existing donors/reference repositories remain
unchanged. The runtime keeps the `OpenBlood2` namespace and project formatting.

## Private-data findings

The configured external installation is `/home/yann/.steam/steam/steamapps/common/Blood II/`.
No commercial files were copied into the project. Tests checked **245 available
volume shells**, including **seven non-convex shells**, across readable base and
multiplayer DAT files. All passed bounded closure validation. Two source Acid
entries in `worlds/multi/ctf_jam.dat` have no matching model. The existing reader
still rejects `worlds/16_bridge.dat` at a typed property; the scan reports and
skips that file rather than claiming complete archive coverage.

`02_CityHub` creates **five original volumes**: Canal1, Pool, Ladder1 and the two
escape ladders. Its canal passes actual player swimming/head/air checks; its
Ladder1 passes collision-clear placement and a **29.17-unit ascent** in 20 frames.
Water metadata in this level uses DamageType=8 and Damage=0, despite a different
source registration default. The runtime retains the file values.

Retail polygon records often contain a repeated ring with extra edge splits.
Using all indices as one closed boundary caused false topology failures. The
volume builder now uses the explicit base ring, handles T junctions and welds
near-identical vertices within 0.01 original units for closure checking. It
retains real vertex/plane positions for queries. These are new engine services,
not a recovered retail BSP implementation or an executable parity comparison.

## Validation

Validation results and file hashes are recorded in
[evidence/m2-containers.json](evidence/m2-containers.json). The current validation
matrix includes 77 synthetic headless cases, nine private headless cases and
six GPU cases on Linux. The final results are:

| Configuration | Passed | Graphics scope |
| --- | ---: | --- |
| GCC Debug with Dear ImGui | 92/92 | Six SDL_GPU/Vulkan window scenarios |
| GCC Release without Dear ImGui | 92/92 | Same six lifecycles, no inspection UI |
| Clang ASan + UBSan + leak detection | 86/86 | Headless only |

Changed C++ files pass clang-format 23.1.3. The six reference repositories remain
clean and all 10,923 inventoried reference-file hashes match. The final
[inspector capture](evidence/m2-containers-inspector.png) was visually checked at
1000 × 680; the volume geometry and main state/property controls are visible.

The nine synthetic container cases cover closed/open/rotated/non-convex geometry, base rings, body/head queries,
hidden triggers, overlap limits, stale handles, static-volume mutation rejection,
generic versus player physics, source viscosity, swimming, surface exit, ladders,
air/armor and volume damage. Earlier object, reader, player, weapon, movement and
locomotion regressions remain part of the suite.

```sh
cd Blood2
cmake --preset local-debug
cmake --build --preset debug -j4
ctest --preset debug --output-on-failure
cmake --preset local-release
cmake --build --preset release -j4
ctest --preset release --output-on-failure

cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DBLOOD2_INSPECTOR=OFF -DBLOOD2_SANITIZERS=ON -DBLOOD2_GPU_TESTS=OFF \
  -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build build/sanitize -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure

# Real window, SDL_GPU and volume inspection, using the configured data path:
./build/debug/blood2 --world 02_CityHub --containers-smoke --capture build/containers.png
```

## Remaining M2 work

Liquid PolyGrid/textured surfaces, underwater fog/color/audio, key handshakes,
AI container reactions, save/restore, specialized Damage/FreeFall registration,
conveyors, crane controls, minefields and moving world models remain unported.
The unchanged wider gate also needs pickups, missing runtime classes and full
combat/death behavior. `01_SubwayA` has no volumes of these five classes; its
previous 18 supported / 372 editor-only / 190 missing runtime source-entry
accounting is unchanged. `--start-game` still rejects the incomplete session.
Other platforms, XR and comparison against the original executable were not
tested. DirLight remains a spotlight; implicit main-light investigation is still
pending as documented in the audit.

## Commits

Implementation and project documentation: `80ffd8a` in the independent
`Blood2/` repository. Workspace documentation, source-integrity records, test
logs and the inspection capture are committed separately in the root repository.
