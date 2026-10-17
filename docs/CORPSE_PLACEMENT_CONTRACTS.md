# Initial ordinary corpse placement

The original CreateCorpse still supplies the cached CMovement transform, raw death
animation dimensions, visual scale and foot-offset formula. The new engine service
only resolves initial primary-world overlap caused by that shape change. It is a
bounded replacement policy, not recovered retail SetObjectDims2 behavior.
See [evidence](../../Documentation/Implementation/M2_CORPSE_PLACEMENT.md) and
[source ledger](M2_CORPSE_PLACEMENT_SOURCES.json).

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
units and the largest new half dimension. Exhausted budgets fail explicitly.

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
distance, correction count and last polygon. Failed candidates are never described
as committed positions. Unqueried means earlier setup failed or placement has not run.

The Skeletons panel shows these fields for NPC deaths and corpses, including hidden
objects. Reading them does not query collision or advance animation. Spawn history
keeps the final successful position or failed attempted candidate. Existing lifetime,
model-key audio, blood-pool diagnostics and unsupported damage/fire/gib services
remain as specified in [corpse contracts](CORPSE_CONTRACTS.md). M2 remains incomplete.
