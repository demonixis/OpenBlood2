# Corpse clearance at doorway ends

The [earlier placement policy](corpse_placement_contracts.md) could retain a dead,
solid Cultist when the living body straddled a doorway end. Its wider death box
needed a translation along the doorway. Requiring the entire living box to already
separate on every candidate axis excluded that direction, despite a clear path.

The existing bounded alternative-axis search now also considers a triangle axis
that separates the **source center**, when it does not separate the full source
box. Full-source choices remain preferred. Center-only choices must additionally
sweep the original living box from its cached anchor through the proposed placement
correction, excluding the original death-height offset. A hit or invalid sweep
rejects that candidate. The corpse's full final box and the original center segment
must still both be clear. This is replacement engine collision, not recovered
retail SetObjectDims2 behavior.

All existing bounds remain: 16 corrections per path, 128 visited alternatives,
128 pending candidates, displacement budget of min(256, largest new half extent),
and unchanged collision tolerances. Position-plus-policy identifies repeated
candidates because the swept-source requirement differs. No body shrink, hidden
actor deletion, collision disable or successful placeholder replaces a failed
transfer. Impossible fits still retain the failed source and deferred child cleanup.
Original CreateCorpse clip choice, raw dimensions, scale, cached transform and foot
offset remain unchanged. Generic sweeps and live-character resize are unchanged.

The owned BodyPlacement report marks `source_sweep` when this extra validation is
required; the Skeletons inspector exposes it for NPC attempts and independent
corpses. Inspection does not perform collision work or advance animation.

Generated geometry tests cover a doorway-end fit and refusal under a lintel:
the final short corpse fits, but the original taller source cannot sweep there.
Existing floor-side, wall-crossing, impossible-width, service-lifetime and ordinary
corpse cases stay enabled. The private mouse-combat regression approaches the first
SubwayA Cultist from a different position, aims lower, fires the original Beretta,
checks transfer, then kills the following Cultist and walks past both corpses. It
runs with 30 and 60 simulation updates
per second, including reload/cleanup. The SDL-window variant uses synthetic events,
not physical-device or human playtest automation.

This addresses the reproduced doorway case. It does not certify every later enemy
or difficult pose, fire/gib death, corpse damage, retail physics parity or complete
M2. See [implementation evidence](../../Documentation/Implementation/m2_corpse_clearance.md).
