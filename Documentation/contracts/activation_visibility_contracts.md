# NPC activation visibility and the authored Gideon intro

This extends [NPC runtime](ai_runtime_contracts.md), [AI visibility](ai_sight_contracts.md)
and [Gideon lifecycle](gideon_runtime_contracts.md). See the
[source ledger](../implementation/sources/m2_activation_visibility_sources.json) and
[execution evidence](../implementation/reports/m2_activation_visibility.md).
M2 remains incomplete.

## Original activation rules

The original ServerDE header describes PingObjects as activation of objects seen
by its observer. It also gives manual INACTIVE/INACTIVE_TOUCH priority over pings.
The original player selects the active server camera, otherwise itself, strictly
after each 0.5-second deadline. AI_Update configures a three-second deactivation
period and initially auto-deactivates; its existing sleep-gap desynchronization
and pending update deadline remain unchanged. Damage and sound alerts still reach
inactive actors (object messages bypass activity, as in the retail SendToObject) and
still do not wake them. No range/FOV, proximity activation, wake-on-damage or forced
active state is added. Weapon/USE rays and CanSee keep their existing filters.

## Retail leaf visibility (decoded)

Disassembled from the retail Server.dll (SHA-256 in the ledger; never executed, no bytes
copied). `WorldVisibility` (`src/engine/visibility.*`) reproduces it for the root BSP.

DAT v56 data, read by `read_world`:

- Each leaf record is `u16 count`, then `count` lists of `u16 portal`, `u16 size` and
  `size` bytes; `count` 0xFFFF is instead `u16 leaf` sharing that leaf's lists. Retail
  copies them at load time, so a reference to the same or a later leaf copies its
  still-empty lists (nothing is visible from it).
- Nodes are `u32 plane`, `u32 polygon`, `u16 leaf` (0xFFFF: none), two `u32` sides
  (-1 inside, -2 outside, else a node index) and 16 unused bytes (centre, radius).
- After the points come a cell grid (three `u32` dimensions, two vectors, per cell
  `u16 count`, `u16`, `count * 6` bytes; skipped) and the `u32` root node.
- The first byte after a surface's flags is `Surface::visibility_skip`.

The lists are bitstreams over a tree built once from the BSP, preorder, back side first:
a node whose polygon surface has `visibility_skip` is dropped with its whole subtree; a
node without a leaf whose back side is outside and front side is not inside is replaced
by its front side. Walking a list from the root, each tree node reads one bit when it owns
a leaf (that leaf is visible), then one bit per tree child (descend; zero prunes the
subtree), back child before front. Bit n is byte n/8, mask `1 << n%8`. Every list of all
58 mounted worlds (base game, multiplayer, credits) consumes exactly its own bytes. A node
reached twice, or a list needing bits past its end, fails world preparation.

PingObjects then:

- finds the observer's leaf by descending the BSP from the root, taking side 0 when the
  signed distance is below -0.0001 and keeping the last leaf met; no leaf pings nothing;
- links every candidate where retail inserts objects into that tree: from the root,
  descending while its sphere (position, `|dimensions| + 0.5`) lies entirely behind
  (d < -r) or in front (d > r) of the node plane, at the first node it straddles or whose
  front side is not a tree node; one that falls behind into a non-tree side is not linked
  (never pinged);
- walks the observer leaf's first list, adding the objects linked at each node it enters,
  marking the current objects visible on a set leaf bit and passing them to a child only
  when they touch that side (back d < r, front d > -r).

There is no ray, range, field of view or object occlusion. Later lists of a leaf apply
only through an open portal (`SetPortalFlags`, set by original Door). Portal flags are
not modelled, so they never apply (the retail default is closed); doors with authored
portals therefore never widen activation. Assumption: retail inserts every pinged object
with this rule (its insertion routine was traced; the per-class choice of objects in the
tree was not).

Worlds without a tree or lists (synthetic test geometry) keep the former replacement: up
to four primary-world segments (centre to centre, observer box top to centre, centre to
candidate box top, top to top, box top = centre + dimensions.y), the first clear one wins.
Only baked primary-world polygons block (glass included); world models, props, characters
and volumes never do.

## Atomic refresh and historical diagnostics

Ping snapshots checked live identities with automatic deactivation enabled and
activity ACTIVE or AUTODEACTIVATE_NOW. It excludes the observer and both manual
inactive states. Collect visible candidates before refreshing any activity,
deactivation deadline or successful-ping counter. A failed visibility query (work bound)
or an invalid segment fails the whole refresh; it must not partly wake actors. No
callbacks run inside these queries. Commit by checked identity, preserving deferred
removal.

Each eligible candidate retains one bounded ActivationProbe: query count, simulation
time, copied endpoints, checked observer ID, outcome and whether that batch committed its
refresh. With leaf lists it records `leaf_visibility` and the observer leaf; with segments,
the first segment's polygon and fraction. Diagnostics are updated as queries run; a visible
result with refreshed=false truthfully records an aborted batch. Invalid observer or
unavailable collision at preflight does not fabricate a new per-candidate query. The
player's existing activation_ping_failures records the failed service call.

Probe IDs own nothing and remain historical after removal. Unload removes the record with
its object and clears the world's visibility. The NPC lifecycle inspector resolves IDs
before displaying names, labels absent identities as removed, and shows outcome, committed
refresh, time, endpoints and the leaf or segment details without issuing queries or
advancing simulation.

## Ping cost

The 0.5-second cadence stays; queries are not spread across frames. A ping descends the
tree once per candidate and walks one list (at most three bits per tree node), with no
object scan. The adapter exposes `activation_ping_ns`/`activation_ping_calls`; `--bench`
reports the per-frame cost ([hitch report](../implementation/reports/main_thread_hitches.md)).

## Observable acceptance and limits

`visibility.decode` covers bit order, pruning, collapsed and skipped nodes, forward share,
sphere linking and side filters, overrun and cycle failures. `visibility.private` decodes
every mounted world. `ai_runtime.ping_private` recomputes each committed probe of 40 pings
in SubwayA and CityHub from the lists. `visibility.train_station`: touching `Trigger10`
wakes the walled-off Fanatics `pride` and `wrath`, whose `FollowPath` scripts take them out
of the cubby. `visibility.cityhub`: from the north ramp, where a wall blocks every segment,
the lists wake Gideon, so `gidtrigger`'s 15 s `Wait` keeps an idle clip. Each repeats with
the lists cleared and reproduces the tester reports (guards never move; Gideon waits in
`run_knife1`, clip 0). The synthetic segment tests and the SubwayA camera1 run are kept.

This does not prove a complete boss encounter, door-portal activation, death parity or
missing world services. Safe NEW_LEVEL, remaining required services and the actual M2 exit
gate stay open.
