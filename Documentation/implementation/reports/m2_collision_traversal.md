# M2 — Reduce post-transition collision work

Date: 2026-10-17. Baseline: `bbfc2f46c5c4e8c0590379e2f1e9c9ccd63dc1aa`.
Implementation: `d1ddceb972fa0052b93abe27eaed2f786c1531df`. **M2 remains incomplete.**

## Finding and change

The reported sustained CityHub stutter is reproducible in a stationary diagnostic
session after NEW_LEVEL. A large periodic cost occurs in original object updates:
CPlayerObj's half-second activation ping performs visibility rays through the
larger destination world. The old BVH traversal only intersected each node with
the enclosing AABB of the entire ray. Long diagonal rays therefore tested many
triangles far from their actual trajectory. Attachment refresh was measured
separately and was small relative to these collision queries.

The engine now also intersects the actual swept center segment against expanded
node and triangle bounds, with conservative tolerances. After an impact is found,
branches beyond that impact can be rejected. The full triangle SAT, original
visibility filters, activation period, object order, callbacks, hit tie-breaking
and body-placement policy are unchanged. No geometry or NPC updates are omitted.
This is an optimization of replacement engine code, not recovered retail physics.

Services exposes cumulative primary-world BVH/triangle work, including rejected
bounds and full triangle tests. Reading it does not issue queries or advance
simulation. Totals reset on build/clear and exclude other world-model indices,
object boxes and rendering. See [contracts](../../contracts/collision_traversal_contracts.md)
and [source ledger](../sources/m2_collision_traversal_sources.json).

## Measured behavior

Representative first paired run; milliseconds, before → after:

| Configuration / world | Simulation p50 | Simulation maximum | Model preparation mean |
| --- | ---: | ---: | ---: |
| Debug / SubwayA | 3.35 → 3.13 | 12.01 → 7.13 | 4.80 → 4.79 |
| Debug / CityHub | 22.07 → 21.94 | 145.46 → 55.12 | 12.70 → 12.82 |
| Release / SubwayA | 0.47 → 0.43 | 2.41 → 1.49 | 0.46 → 0.45 |
| Release / CityHub | 3.44 → 3.32 | 30.46 → 11.93 | 1.26 → 1.24 |

Three paired Debug runs agree: CityHub's worst simulation step falls from
145–147 ms to 54–55 ms. The median of its ten expensive object-update callbacks
per measured run falls from 122–124 ms to about 32 ms. The regular physics,
container/audio and model-preparation costs remain. No pure-skinning timing or
end-to-end frame-rate claim is made.

These are CPU measurements from the retained [frame probe](evidence/m2-collision-traversal-frame-probe.cpp),
not displayed frame rates or GPU timings. Each world runs 360 fixed 1/60-second
ticks, discarding the first 60 for whole-stage statistics. SubwayA transfers to
CityHub at six simulated seconds, during its intro, with the original player and
world objects. This is a diagnostic transfer, not an authored playthrough.
Audio PCM inspection includes offline mixing; no audio device or renderer is
opened. Model/world/sprite preparation runs, but GPU upload/drawing and ImGui do
not. The Debug diagnostic wrapper and app preparation translation units use -O2;
engine/game libraries use the actual unoptimized Debug configuration. Release
engine/game libraries use the project's -O3. The out-of-tree timing patch changes
only measurement, retaining source call ordering and short-circuit failure rules.
Paired final measurements run without concurrent builds/tests.

The original broadphase-only prototype already reduced the periodic cost; the
committed version additionally rejects leaf triangles and geometry behind a hit.
Absolute times depend on compiler, system load and location. The remaining steady
simulation/model cost is real, especially in Debug. This is evidence for one
identified cause, not proof that every user-observed transition stutter is fixed.

## Skinning clarification

ABC skinning is currently CPU work in `sample_vertices`: interpolated hierarchical
node transforms, rigid per-node vertices and packed per-vertex deformation in
source write order. `ModelView::add` expands visible faces into a triangle buffer
and the dynamic SDL_GPU renderer uploads those vertices every frame. Flag-hidden
objects skip geometry; visible off-screen models have no frustum culling yet.

The measured model stage includes skinning, validation and mesh construction;
it must not be described as pure skinning time. GPU transfer/drawing is excluded.
CPU node poses are also needed for attachments and gameplay queries even if visual
skinning later moves to the GPU. This increment leaves that architecture unchanged.
Future rendering work should measure culling, static/indexed geometry and deformation
uploads before selecting a GPU path that preserves ABC vertex animation and both
Vulkan/Metal support.

## Verification

A deterministic [comparison probe](evidence/m2-collision-traversal-equivalence.cpp)
performs **48,000** sweeps on actual SubwayA/CityHub primary-world geometry: rays,
player-size boxes, tiny boxes, large boxes, stationary queries, tiny displacement
and shifted endpoints. Baseline and changed binaries produce identical hashes
of every query result, fraction, point, normal, polygon, penetration and surface
flag. This is finite sampled equivalence, not a proof for all inputs.

The new synthetic traversal test checks a long diagonal through 2,048 triangles,
requiring fewer than 128 full SAT tests for a miss, precise contact, equal-time
polygon selection under penetration, transformed geometry, invalid/empty queries
and diagnostic reset/accounting. Existing movement, stairs, visibility, doors,
world paths, body placement, corpse clearance, lifetime and transfer checks remain.

**98/98 selected headless cases pass** in GCC Debug, inspector-free Release and
Clang ASan/UBSan with leak detection; 15 cases use external retail data. For
Debug/Release this includes the initial 97 passes plus the corrected world-path
fixture and collision cases passing on retest. The sanitizer suite passes in one
final run. No sanitizer finding was observed.

**Six GPU scenarios pass in each graphical build** on NVIDIA GeForce RTX 3060
Laptop GPU: lifecycle, movement, world paths, mouse combat, corpse clearance and
NEW_LEVEL. No Vulkan validation error was recorded. Transition captures and the
inspector lifecycle capture were visually reviewed; the new Services text itself
was source-checked, not separately captured. Native Metal was not retested.

The initial Debug/Release suites exposed a pre-existing stale world-path fixture:
its 232-object expectation is now 233. Re-running the old collision implementation
against the same current session reproduced that failure. With the exact count
updated to 233, old and new traversals both retain two cycles of 297 train moves,
two blocks, the same polygon/fractions/final position, eleven attachments and clean
unload. The obsolete printed runtime=188 is replaced by the actual checked count.
No movement, contact, lifetime or resource assertion was removed. Clang also caught
implicit narrowing in the new synthetic fixture's 16-bit polygon indices; those
bounded conversions are now explicit.

The [evidence manifest](evidence/m2-collision-traversal.json) retains commands,
logs, comparison sources, timing instrumentation and implementation hashes.
Commercial resources and GPU captures remain external/ignored. All 10,923
inventoried reference-file hashes and six reference Git states remain unchanged.
Native Metal execution and a human playthrough were not repeated for this change.

The idle-Cultist/durability reports, possible encounter-specific weapon failure,
missing CityHub water presentation, remaining frame costs and required M2 services
remain open. Original game settings and gameplay values have not been retuned.
