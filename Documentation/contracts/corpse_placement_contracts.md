# Initial ordinary corpse placement

> Continued by [doorway clearance](corpse_clearance_contracts.md): a source-center
> alternative requires an additional swept living-box check. Existing raw dimensions,
> bounds and final collision checks remain mandatory.

The original CreateCorpse still supplies the cached CMovement transform, raw death
animation dimensions, visual scale and foot-offset formula. The new engine service
only resolves initial primary-world overlap caused by that shape change. It is a
bounded replacement policy, not recovered retail SetObjectDims2 behavior.
See [evidence](../implementation/reports/m2_corpse_placement.md) and
[source ledger](../implementation/sources/m2_corpse_placement_sources.json).

## Checked placement

PlaceNonSolidModel accepts a live, unattached, nonsolid character model. Pending,
stale, solid, container/world-model and reentrant movement requests fail. Missing
collision is unavailable. All positions/extents must be finite and within existing
physics bounds; all half dimensions must be positive.

The cached source box must already be clear of primary-world triangles. Preserve
that anchor rather than querying a later actor position. Work on an owned candidate,
leaving the object untouched until success. Each overlapping triangle supplies a
separating-axis correction only on a side that separated the clear source box.
Using just the nearest side can push a wide corpse through a train floor/seat.
Resolve in the existing deterministic BVH/polygon order. Add 0.001 units of clearance,
allow at most 16 corrections and cap cumulative distance at the smaller of 256
units and the largest new half dimension.

If that nearest-axis pass exhausts its budget, a bounded fallback tries the other
separating axes of each actual blocking triangle. It retains only directions that
separate the original clear source body. Up to 128 distinct candidates are tested
with a fixed 128-entry pending stack; near-identical positions (below 1e-5 units
per axis) are not revisited. Shorter corrections are explored first. Each path
retains the same 16-correction and distance limits. The fallback adds no .001 skin:
exact contact already counts as nonpenetrating under the unchanged sweep contract.
No extent or collision tolerance is reduced to obtain success. This handles both
an exact-width corridor and a wide corpse needing a different separation around
train seats. See the [playtest correction](../implementation/reports/m2_playtest_input.md).
Exhausted searches still fail explicitly.

Success requires a clear final full box and an unobstructed center segment from
the source anchor. No dimension shrink, wall crossing or partial position commit
is accepted. This is triangle-surface collision, not BSP solid-volume contents;
center visibility does not establish a swept path for every point of the expanded
shape. Intermediate candidates are queries, not simulated movement. The bounded
solver can refuse a locally possible fit. It does not rescue an already penetrating
source, resolve arbitrary dynamic-body stacks or claim retail collision parity.

Only primary-world geometry participates, matching the nonsolid corpse's existing
gravity integrator. Moving world models/other objects do not become solid supports
for corpses. Generic MoveObject, TeleportObject and live-character ResizeObject
policies are unchanged. Placement advances no clocks, dispatches no contacts and
commits only position/contact reset; later gravity runs through the ordinary single
integrator with the original dimensions and mass.

## Ownership and inspection

A successful transfer copies the bounded placement report into CCorpse before the
original NPC hide/removal branch. Failure removes the partial child deferred and
retains the visible actor and failed report. A source disappearing later does not
invalidate the independent corpse report. The report stores values, not borrowed
geometry or actor pointers: outcome, anchor, requested/candidate position, cumulative
distance, correction count, tested-candidate count, zero-margin fallback flag and
last polygon. Successful fallback counters describe the accepted path; candidate
count includes the unsuccessful first pass. If no candidate succeeds, the original
failed placement is retained with the total candidate count. Failed candidates are never described
as committed positions. Unqueried means earlier setup failed or placement has not run.

The Skeletons panel shows these fields for NPC deaths and corpses, including hidden
objects. Reading them does not query collision or advance animation. Spawn history
keeps the final successful position or failed attempted candidate. Existing lifetime,
model-key audio, blood-pool diagnostics and unsupported damage/fire/gib services
remain as specified in [corpse contracts](corpse_contracts.md). M2 remains incomplete.
