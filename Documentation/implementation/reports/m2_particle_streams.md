# M2 — Original authored particle streams

The original server `CClientParticleStreamSFX` and client `CParticleStreamFX` now
create, toggle, ramp and render authored particle streams. CityHub has **443 created
source objects / 31 transferred sounds / 541 editor-only entries / 10 unsupported /
one failed entry**, totaling 1026. Its eight formerly missing emitters now create
with decoded external DTX resources: shack smoke, three steam streams, three spark
streams and fire. **M2 remains incomplete; CityHub NEW_LEVEL still refuses.**

## Implementation and source comparison

The new port retains original server property/default handling, no scheduled server
updates, FORCECLIENTUPDATE/SAVEABLE/visible flags, publish-before-filter ordering,
and case-sensitive ON/OFF/TOGGLE prefixes. A typed owned descriptor replaces the
legacy SFX message. Source hashes and explicit adaptations are recorded in the
[import ledger](../sources/m2_particle_stream_sources.json).

The client keeps its four original states, immediate startup burst without a ramp,
strict burst deadline, per-field ramps, integer count truncation and full ramp
restart on reversal. It emits at most one burst per effect callback. Original
random draw order, the unusual red-derived color formula and the equal-red
minimum-color branch are tested. Existing particles move with the system's
translation; only new velocities follow later owner rotation. Alpha remains
constant. Existing particles finish after OFF, whereas source removal ends the
whole system.

The original SFXMgr reads only through ParticleFile despite the server sending a
sound radius and three sound strings afterward. The port exposes this unused tail
without fabricating audio. It initializes undefined clocks and handles zero-length
velocity safely. All original game references remain unchanged. Existing bounded
particle integration and SDL_GPU alpha billboards are replacement engine services,
not verified retail raster/physics parity. No new renderer, dependency or ECS is
introduced; the existing Vulkan/Metal shader artifacts are reused unchanged.

## Inspection and ownership

The Scene explosions panel has an Original particle streams tree exposing each
source/effect identity, hidden/on/off/ramp state, clocks, parameters, external
texture and unused sounds, actual live/emitted/prepared particle counts, and all
particle positions, velocities, remaining lifetimes and colors. Historical errors
remain visible after source removal. The ordinary source property inspector also
retains all authored fields.

Typed handles are checked at update and render; pending owners cannot render.
Simulation runs with the inspector hidden or absent. Explicit bounds cover system,
particle, burst, clock, path and numeric sizes; exhaustion fails visibly. Initial
NEW_LEVEL preparation checks stream textures even when off and includes them in
its shared presentation budget. Source/candidate effects remain owned by their
respective sessions. See [contracts](../../contracts/particle_stream_contracts.md).

## Validation

GCC Debug (inspector enabled) and Release (inspector disabled) build successfully.
Across the complete suites and final focused reruns, each verifies **749 distinct
headless tests**, including **122 private-data cases**. Each verifies **9/9 GPU/window
tests** (5 private) on **NVIDIA GeForce RTX 3060 Laptop GPU**, including mouse
combat, corpse traversal, objectives, NEW_LEVEL, particle readback and lifecycle.
Clang ASan/UBSan with leak detection verifies **34 distinct cases** (five private).
All **29 changed/new C++ files** pass the repository formatter; **10923 reference
files and six independent reference repositories** remain unchanged.

The initial native suites each had one faulty new test assumption: a few random
samples need not reach the outer half of the offset interval. Its correction uses
the exact expected draw and retains independent flag assertions; the seven stream
cases then pass in both builds. The initial sanitizer CityHub run exceeded 180
seconds under concurrent work. Its sanitizer-only limit is now 360 seconds, with
all frames and assertions retained; the final run passes in 155.03 seconds. Other
selected sanitizer cases passed initially. The existing ItemPickups.cpp enum/float
warning remains unchanged. Complete commands, initial failures and final outcomes
are retained in the [validation record](evidence/m2-particle-streams.json).

GPU readback verifies 256 exact tint/alpha texels and 60/67 actual CityHub particle
quads across reloads (11151/10967 covered pixels). The isolated particle capture was
visually inspected. This is Linux/NVIDIA evidence; native Apple automation is not
claimed. The shared Vulkan/Metal shader artifacts are unchanged.

Generated fixtures check the original random layout and color formula, strict
cadence, translation/current direction, independent particle aging, every ramp
flag, reversal/completion, command case/prefix rules, unused audio, malformed
settings/resources, creation reasons, duplicate/system/particle budgets, deferred
removal, difficulty filtering and monotonic identities across reloads.

Three real CityHub cycles at 50/60/20 Hz check all eight emitters, exact initial
29 particles, rendered resource preparation, OFF expiry, all-emitter ON rearming
(42 particles), and complete teardown. GPU readback separately checks tint/alpha
against known generated texels, then actual CityHub particle textures/positions
across hidden simulation, source removal and reload. Existing world-transition,
source-lifetime and gameplay regressions remain in the validation selection.

## Remaining M2 work

CityHub still lacks four SkyPointer entries, one DemoSkyWorldModel, two
ProximitiesPU, two RemotesPU and one FlashlightPU. `fallengrills` still has no named
world model and remains an explicit failed CDestructableBrush. These are preserved
failures, not filtered objects or success-shaped substitutes. Required services,
full shell startup/acceptance and original presentation remain open. This increment
does not close M2, establish native Metal automation, or claim a full campaign.

Implementation commit: `724484c5c8f43d333f03386f2de579a6c469a22e` in the independent Blood2 repository.
