# M2: Independently closed compound DAT geometry

Project commit: `4b8444ac9557de0fb3c882403cb3029206429d48`.

The replacement engine now accepts closed components that share an edge without
assuming that any balanced set of faces is valid. This restores CityHub's original
`CDestructableBrush startfences` and `Door Buildings` through their normal creation
callbacks. Original classes, property rules, dependency pins, reference sources and
commercial assets are unchanged.

See the [geometry contract](../../contracts/compound_shell_contracts.md) and
[retained evidence](evidence/m2-compound-shells.json). This is a correction to new
engine validation, not a recovered retail physics algorithm.

The old validator reported four incident faces with balanced directions at two
edges in each model. Connecting faces through ordinary two-face edges reveals
three independently closed components for `startfences`, two for `Buildings`.
The validator now verifies opposite paired edges inside each component, preserving
base-ring selection, split-vertex welding, plane checks and all existing limits.
Open, duplicated, misoriented and unresolved non-manifold components still fail.
No missing face, named model or collision volume is fabricated.

The fence retains original polygon collision. Tests probe all three actual pieces,
the empty space between them, and a ray entering its front piece at fraction 0.32.
`Buildings` retains its original BoxPhysics flag and overall box. Synthetic tests
cover partial shared edges/T-junctions, vertex contacts, separated components,
rotated contents, boundary sweeps, gaps, translations and geometry lifetime after
object removal/unload. Failures clear partial output and inspection counters.

This also exposed an older ground-query inconsistency: `MoveObjectToGround` tested
every object as a pivot-centered box. The new fence initially changed CityHub's
expected 18 model ground misses to 13. Reusing `sweep_object` restores actual polygon
gaps and offset box centers; the unchanged 18-miss assertion now passes. Dedicated
probes check landing on both polygon pieces, falling through a gap and the different
authored-box result. Original downward-ray, dimensions and placement rules remain.

The private door catalog creates **619 doors**, including 14 newly accepted
compound models, **149 SelfTriggers** and **114 polygon-mode doors**. The remaining
**79 rejected entries** are still checked individually: 47 TypeAction schemas,
19 ambiguous names, 12 missing models and one unresolved compound topology.
`worlds/multi/bb_condemned.dat: Buildings` still has four edge uses in a single
connected component and remains rejected. The initial catalog failure was its old
605/93 expectation; the new assertions additionally check all 14 recovered models
and the exact remaining topology failure.

The brush catalog additionally accepts three `startfences` instances across the
CityHub variants: **2,298 created / 157 rejected CDestructableBrush** entries;
all **98 supported RotatingBrush** instances remain unchanged. The two invalid
brush shells and every previous non-geometry rejection category remain checked;
explosion, spawn, motion and deferred lifetime checks are retained.

CityHub now has **421 created / 31 transferred sounds / 541 editor-only /
31 unsupported / two failed entries**. The two failures are still `fallengrills`
(missing named model) and CultistAI0 (unimplemented FLAREGUN). Destination structural
inspection remains 454 checked / 31 unknown / zero invalid; the temporary
initialization rehearsal retains the active source session and player. SubwayA
accounting is unchanged. None of these counts closes the M2 gate.

The common object inspector and the Containers/Doors panels now display closed
component and shared-edge counts, including hidden objects. CPU and Vulkan tests
load CityHub twice, inspect both recovered objects, check visible submissions,
hide them, remove them before deferred cleanup and reload with new identities.
The GPU check isolates each object's actual textured geometry and reads pixels;
hidden and removed submissions must produce background only. Private captures
stay in ignored build directories. The fence uses the existing translucent pass;
the first probe incorrectly submitted only opaque faces and was corrected to
exercise both existing passes, without relaxing its pixel checks.

## Validation

Linux validation completed:

- GCC Debug and inspector-free Release: **684/684 headless tests each**, including
  110 private-data cases (104.99s / 11.45s).
- **10/10 focused Vulkan GPU tests each**, including nine private-data cases
  (53.63s / 32.26s): compound geometry, containers, doors, interaction, brushes,
  objectives/destination rehearsal, world paths, depth, viewport and world alpha.
  Each of two loads produces 1,412 non-background fence pixels and 3,318 building
  pixels, then zero after hiding and pending removal. Release captures were
  visually checked. This is focused coverage, not a rerun of all 61 GPU tests.
- **129/129 Clang ASan/UBSan cases**, including 15 private-data cases, pass with
  leak detection (147.89s). Coverage includes objects/services, worlds/destination
  isolation, player entry, geometry/containers, doors, rotation, brushes, pushables,
  world paths, model bodies and draw-list lifetime. No sanitizer diagnostic or
  Vulkan validation diagnostic appears in the retained final logs.
- 12 changed C++ files pass formatting; 2,914 local Markdown links resolve.
  All 10,923 inventoried reference files match their hashes and all six reference
  repositories remain clean. Dependency pins and commercial assets are unchanged.

Initial catalog/GPU failures and the first full headless runs are retained alongside
successful final runs. The old brush totals and the ground-miss regression were
investigated as described above. No frame, collision, malformed-input, lifetime or
pixel assertion was removed to pass; no test timeout was increased.

Commands run from `Blood2/` (external content and GPU testing use the existing
ignored local configuration):

```sh
cmake --build build/debug -j4
cmake --build build/release -j4
cmake --build build/sanitize -j4
ctest --test-dir build/debug -L headless -j4 --output-on-failure
ctest --test-dir build/release -L headless -j4 --output-on-failure
ctest --test-dir build/debug -R '^gpu\.(compound_shell|containers|doors|interaction|brushes|worldpaths|viewport|world_depth|world_alpha_private|objectives)$' --output-on-failure
ctest --test-dir build/release -R '^gpu\.(compound_shell|containers|doors|interaction|brushes|worldpaths|viewport|world_depth|world_alpha_private|objectives)$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize -j2 -R '^(compound_shell|objects|services|containers|doors|interaction|brushes|pushables|worldpaths|world_view|world|player|destructable_models)\.' --output-on-failure
```

## Remaining boundary

This conservative topology check does not implement arbitrary self-intersection
repair, coincident-face union, retail BSP physics or whole-level gameplay parity.
The 31 unsupported CityHub entries, two initialization failures, other required
services and safe NEW_LEVEL transfer keep **M2 incomplete**. M3–M8 remain planned.
Only Linux is exercised; Windows/macOS/mobile/XR and a retail executable comparison
are not claimed.
