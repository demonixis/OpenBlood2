# M2 — Reduce repeated container enumeration

Date: 2026-10-17. Baseline: `d1ddceb972fa0052b93abe27eaed2f786c1531df`.
Implementation: `2c82c1fc2c744bced9847dc66b9c4b7b51f1bd93`. **M2 remains incomplete.**

Following the collision traversal correction, profiling the stationary CityHub
session after NEW_LEVEL identified repeated enumeration as another steady CPU
cost. In six simulated seconds, `container_snapshot()` was called 201,292 times.
The instrumented Debug run spent about 3.49 seconds inside this function alone;
actual point-contents and overlap geometry were much cheaper. These nested timers
include instrumentation overhead and must not be added together or interpreted
as end-to-end frame time.

Every query previously rescanned all allocated object slots to select a handful
of bound volumes. `Objects` now maintains only their checked identities in slot
order. Shape binding adds an entry before original initial update; destruction
removes it after the original destructor. A private shape owner and read-only
accessor prevent callers from changing binding without updating the index.
Each query still receives a fresh owned copy and resolves current object state.
No physics, animation, sound, AI update or rendered geometry is skipped.

Nested creation affects subsequent queries. Removal is immediately excluded by
existing life checks, while pending records remain accounted for until deferred
reclamation. Reused low slots preserve first-volume precedence, and generations
prevent stale reuse. Hidden-first head behavior, four body records, original
volume damage cadence, liquid gravity/current and the single player integrator
are unchanged. The Containers inspector shows the bound-index count and live
brushes, including hidden ones. Rejected world entries remain visible in World.

See [contracts](../../contracts/container_index_contracts.md) and
[source ledger](../sources/m2_container_index_sources.json).

## Validation scope

Two new synthetic cases exercise owned snapshots, slot reuse/generation checks,
precreate and geometry rejection, initial-update self-removal, nested creation,
destructor-time creation/removal, current-pass versus next-pass physics callbacks,
and repeated unload/reload. Their oracle scans actual object records independently
of the index. The keyframe rejection fixture now uses a genuinely bound Water
object instead of assigning a synthetic shape directly to a record.

The existing private CityHub volume test also compares the index against all
bound records before swimming and ladder movement. Source rules and gameplay
values are not retuned. Native Metal and a human playthrough are not part of
this automated validation.

The full application and test targets build in GCC Debug, GCC inspector-free
Release and Clang ASan/UBSan. **122/122 selected headless tests pass in each
configuration**, including eighteen cases using the external retail data.
Leak detection and sanitizer halt-on-error are enabled; no sanitizer finding was
reported. No test timeout or gameplay assertion was weakened.

**Seven GPU scenarios pass in each graphical configuration** on NVIDIA GeForce
RTX 3060 Laptop GPU: lifecycle, movement, containers, conveyor, keyframes, mouse
combat and NEW_LEVEL. No Vulkan validation error was recorded. Native Metal was
not retested. Commands, logs, comparison probes and implementation hashes are
retained in the [evidence manifest](evidence/m2-container-index.json).

## State comparison and graphics

A deterministic diagnostic compares 360 fixed 1/60-second ticks in SubwayA and
360 after NEW_LEVEL to CityHub. At every tick it hashes checked object identities,
life/activity, flags, transforms, velocity/acceleration, dimensions, scheduling
and fresh container queries at each object's position. Both worlds produce the
same digest before and after the change. This is sampled equivalence of the
listed state, not proof of every internal game field or retail behavior.

The control compiles the baseline `objects.cpp` scan with the current record
layout and read-only accessor, ahead of the same Debug engine/game libraries.
It therefore exercises actual source callbacks, world entry and physics while
changing only the container bookkeeping implementation. Commercial data remains
external. The diagnostic transfer occurs during the introduction, not during a
human playthrough.

The NVIDIA Containers capture visibly reports eleven bound CityHub volumes;
the water membership, live volume list and controls remain present. The repeated
level-change capture renders the destination world and models. Captures stay in
ignored build output; only their hashes are retained in the evidence manifest.

## Isolated CPU measurements

Three paired runs per configuration, with no concurrent build or test process.
Values below are medians of the three runs' statistics, in milliseconds:

| Configuration / world | Simulation p50, before → after | Simulation maximum, before → after | Model preparation mean, before → after |
| --- | ---: | ---: | ---: |
| Debug / SubwayA | 3.12 → 2.08 | 7.34 → 6.11 | 4.74 → 4.77 |
| Debug / CityHub | 21.83 → 14.11 | 54.29 → 47.18 | 12.70 → 12.82 |
| Release / SubwayA | 0.44 → 0.41 | 1.33 → 1.39 | 0.45 → 0.46 |
| Release / CityHub | 3.30 → 3.20 | 11.52 → 12.06 | 1.27 → 1.28 |

The steady CityHub simulation cost falls about **35% in Debug**. The Release gain
is small (about 3% at the median); its peaks do not show a consistent improvement.
This is not evidence that Release playtest stutter is solved. Model preparation
is effectively unchanged, as expected. No displayed frame rate or pure-skinning
performance is inferred from these numbers.

The retained frame probe runs 360 fixed 1/60-second ticks per world, discarding
the first sixty for stage statistics. It performs real game simulation, offline
PCM mixing and world/model/sprite mesh preparation, but excludes GPU upload/draw,
ImGui and the real audio device. Loading and destination preparation are outside
these stage measurements. Debug diagnostic wrappers and scene preparation use
-O2, with the actual -O0 engine/game libraries; Release uses the project's -O3
libraries. Both Debug versions use the same retained session timing patch. The
before timing binaries were preserved at the baseline commit before rebuilding.
The equivalence control described above is separate from these timing binaries.

All 10,923 inventoried reference-file hashes and six reference repository states
remain unchanged. No commercial asset was copied into either repository.

## Remaining work

Visual ABC skinning, vertex deformation and mesh expansion still run on the CPU;
model preparation and other frame costs remain. This increment addresses one
measured steady simulation cost, not every cause of transition stutter. Idle
Cultists, encounter durability/possible firing failure, missing CityHub water
presentation and required M2 services remain follow-ups. No completed M2 gate or
universal frame-rate claim is made.
