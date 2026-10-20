# M2: Original world-model debris

Project commit: `f790803891661f64d7871ae8ea4047147e4a2fe0`.

The original `WorldModelDebris` class and the brush destruction lookup now execute
in the clean project. CityHub's ten authored pieces bind to actual DAT geometry,
remain hidden until activation, and have independent checked lifetimes. This
continuation does not complete M2.

Read the [contracts](../../contracts/world_debris_contracts.md) and
[source ledger](../sources/m2_world_debris_sources.json). Original Start,
Update, property rules, Rotating ownership and brush death order are preserved.
The parent searches contiguous `namedebris0`, `namedebris1`, etc., starts all
matching fragments, and stops at the first empty list. Wrong-class matches do not terminate the sequence. The original terminal
empty-list leak is fixed without changing the search result.

Start heals, restarts spin-up, replaces all flags, sets authored velocity and
priority 100, and schedules .01 seconds. Landing switches the fragment off and
starts spin-down; it leaves acceleration, gravity and velocity intact. The
existing integrator remains the only translational physics path. Rotation and
translation retain checked deferred contacts and independent removal.

The source explicitly clears BOXPHYSICS at Start. Moving pieces use a documented
conservative enclosing-bound sweep, while incoming queries and rendering retain
the authored polygon geometry. Dynamic rotation additionally checks a bounded
angular envelope against the primary world before atomic commit. This is
replacement physics, not a recovered retail polygon solver. Ordinary dynamic
brushes still require BoxPhysics and reject rotation. General contact-force
magnitudes, stacks, crushing and restitution remain unavailable.

CityHub now has **432 created / 31 transferred sounds / 541 editor-only /
21 unsupported / one failed** entry. `fallengrills` still lacks a named world
model and remains a failure. Structural destination preparation reports
464 checked / 21 unknown / zero invalid entries. Destination rehearsals retain
the source session; no automatic NEW_LEVEL transfer is enabled.

In two actual CityHub cycles, original billboard destruction activates all six
named billboard pieces. They fall and reach support; the other four authored
pieces remain independently inspectable. No replacement mesh is synthesized.
The synthetic cases separately cover duplicate/wrong-class/gapped names,
authored StartOn override, exact activation flags, world/string property rules,
repeated starts, fractional gravity, spin-down, hidden/inactive stepping,
blocked and over-budget atomic rotation, deferred contact removal, failed support
and cleanup. These tests are not a claim of complete campaign debris parity.

Brushes exposes original activation/landing counts, velocity/gravity parameters,
rotation ramps, actual flags, current collision/placement and service failures.
Destruction history retains activation counts after the parent disappears. The
original queued parent KILL command exercises the path; rendering/UI never steps
simulation or directly starts fragments.

## Validation

Validation results and commands are recorded in [evidence](evidence/m2-world-debris.json).
Only Linux/Vulkan is being verified. Debug includes Dear ImGui; Release omits it.
GPU captures use real CityHub geometry, assert hidden/pending removal produces no
draw, and check a visible fragment before/after motion and world reload. Commercial
captures remain in ignored build directories.

| Check | Result |
| --- | --- |
| Debug headless | 700/700, including 113 private-data tests (141.56 s) |
| Release headless, inspector disabled | 700/700, including 113 private-data tests (14.45 s) |
| Debug GPU | 6/6, including five private-data tests (31.19 s) |
| Release GPU | 6/6, including five private-data tests (9.21 s) |
| Clang ASan/UBSan with leak detection | 68/69 initially; the timed-out private case passes separately in 127.50 s |
| Formatting/reference integrity | 23 C++ files checked; 10,923 reference files unchanged; six reference repositories clean |

All 69 selected sanitizer scenarios are therefore verified, without a final memory,
undefined-behavior or leak finding. The initial new private test exceeded its
90-second harness guard. Only its instrumented wall-time allowance was raised to
180 seconds: both load/unload cycles, 100 simulation steps per cycle, all inputs
and behavioral/lifetime assertions remain identical. Parser/motion bounds and the
normal 90-second guard are unchanged. Initial timeout evidence is retained.

GPU checks cover original brush/door interaction, compound geometry, application
lifetime and temporary destination entry/cleanup, alongside the new fragments.
The selected real fragment draws 4,008 non-background pixels both before and after
translation, with a camera following its current center; hidden and pending-removed
states draw zero. Both reload cycles and both build configurations agree. The
textured fragment captures and inspector capture were visually inspected. No GPU
validation error was reported. The objectives smoke also updates an obsolete
pre-Flare Gun destination count; all its state-preservation assertions remain.


## Remaining M2 work

The 21 unsupported CityHub entries include other pickups, conversation, sky and
particle-stream objects. Missing `fallengrills` geometry, required remaining
services, safe live NEW_LEVEL transfer and the unchanged M2 acceptance criteria
remain open. World-debris save/restore, client gibs, rotator audio and verified
retail dynamic collision are not claimed. M3–M8 remain planned.
