# Allowed contact overlap in MoveObject

The swept solver separates a body from the face it hits by 0.001 units along
that face's normal, using float positions. At a crease or a converging wedge the
rounded contact point, or the separation itself, can leave the box a few ulps
inside the adjacent face. CityHub reproduced this within seconds of roaming: a
floor/steep-wall crease left 1.7e-5 units and a wedge between faces whose normals
have a dot product of -0.894 left about 2.4e-4. The next sweep then reported a
start penetration and `MoveObject` stopped the whole simulation step. The same
failure also suspended a CivilianAI.

Later LithTech's `collision.cpp` classifies planes with a 0.001-unit tolerance
and pushes an intersecting start back along the motion, or keeps the object in
place, instead of failing. That corroborates a tolerance. It is not a trace of the
1998 executable.

## Policy

Sweeps take an opt-in `allowed_penetration` (default 0). Only `MoveObject` passes
it, with **0.01** units, for its main, pushable-clearance and first stair-probe
sweeps. A start overlap up to that depth, against a world triangle, a box or a
world-model shell, is handled by its penetration (escape) normal:

- motion leaving the face, parallel to it or zero-length is not blocked;
- motion going deeper becomes a contact at fraction 0 with that normal. The body is
  lifted out by the whole overlap plus the usual 0.001 separation, and the remaining
  motion slides.

The body therefore never moves deeper into a tolerated overlap. Lifting out only
the usual 0.001 was not enough. In wedges, alternating separations from the other
face ratcheted an overlap past 0.01 within minutes (3 of 40 five-minute roams,
including one against a CivilianAI box). Full lifting resets each tolerated face
whenever it is contacted.

Every other caller (resize, support, placement, movers, rotation, projectiles,
world bodies) keeps the previous start-solid semantics. At equal fractions a
tolerated overlap loses the tie against any other contact, so it never masks a
deeper start penetration. A world-model shell that contains the body center
remains start-solid regardless of depth. Overlaps deeper than 0.01 still fail
explicitly, except that a move leaving such a face (and, for non-solid passive
objects, sliding along it) is allowed; see
[collision traversal](collision_traversal_contracts.md). The failure now names the world polygon or object, the depth, the
escape normal and the position. This is replacement physics, not general
depenetration: no position is searched for and ignored overlaps change nothing.
A contacted overlap behaves like any fraction-zero contact (velocity projection,
stopping force and touch callbacks).

`CollisionHit::allowed_overlap` marks such a contact. Each record counts the ones
`MoveObject` acted on (`overlap_contacts`, `last_overlap_depth`). The Movement
inspector shows both. Ignored overlaps (leaving or sliding) are not counted.

## Tests

`services.contact_overlap` checks unchanged default start-solid results, free
leaving/sliding/static queries, a fraction-zero contact for deeper motion (triangles
and boxes), full lifting, a deeper overlap that still fails with its contact named,
and a tolerated floor overlap that cannot mask a deep wall overlap. It also
drives 2000 randomized moves into a synthetic CityHub-like wedge at the observed
coordinates. That case fails without the tolerance. The private
`locomotion.cityhub_roam` replays four seeded 30-second CityHub roams at ~100 Hz
(walk/run/strafe/jump/crouch/turn/fire). Each of them stopped on a start
penetration before this change. `locomotion_tests roam DATA SEED CHARACTER SECONDS`
reproduces any longer roam. A macOS sweep of 40 seeded five-minute roams (200
simulated minutes, all four Chosen) completed without failure. It recorded 20,897
acted contacts, and a per-step static check never found an overlap deeper than
0.002 units. This is randomized coverage of reachable CityHub areas, not a
guarantee for every geometry, and it is not a retail feel comparison.

## Authored placement nudge

`MoveObjectToGround` keeps every existing path, including lowering a body out of a
ceiling it starts through. Only when a solid box's descent itself starts blocked (a
wall or floor it overlaps more than the 0.01 tolerance, under `StartOverlap::Leave`) does
it first nudge the box out of the primary world: a zero-length sweep reports the
overlapped face's separating normal and depth, the box moves by depth + 0.001, at most
four corrections, and never further in total than its smaller horizontal half extent.
The nudge is committed (teleport, no contacts) only if the final box is clear; otherwise
nothing moves and the following MoveObject still names the overlap. Objects other than
the primary world are not nudged. The record keeps `placement_nudge`/`placement_nudges`,
shown in the NPC runtime panel.

03_Museum's Cultist "pud" is authored 2.86 units into a wall; the original one-sided BSP
planes let it settle, while this sweep cannot start inside a solid body. A thin polygon
separates towards its nearer side. This is a bounded replacement placement policy, not
depenetration during simulation: later moves keep the rules above. Other 06_Airship1 and
11_Slaughter_House placement failures have different causes and remain open.

## Spawned NPC placement

`ResolveInitialWorldOverlap` extracts that same four-correction, smaller-horizontal-
half-extent budget into a typed service. `MoveObjectToGround` still invokes it only
when its original descent starts blocked. Non-world NPC initialization (except
Gideon) now invokes it once after initial animation dimensions, before simulation.
This handles Demo_01's `spawn101` at (-1888, 2144, -2384): its newly created
Fanatic's raw 19-unit half extent overlaps primary polygon 9174 by 3 units.
Separation moves Z by -3.001 and Y by about -6.135 to clear a second overlap
with the ceiling. X and launch velocity are unchanged; there is no ground drop.

The source explicitly skips the ground drop for non-world creation
(`AI_Mgr.cpp` MID_INITIALUPDATE and InitialUpdate). That skip remains: this service
only separates an initial box from primary-world faces. It does not lower a
spawned actor, alter the Spawner pose/data, weaken movement collision, or implement
dynamic-object depenetration. Clear boxes retain their pose; unresolved candidates
return failure without committing any correction. Existing placement counters
remain visible for hidden/inactive actors. This is replacement engine placement,
not verified retail physics.

`demo.spawned_overlap` deliberately triggers the real authored Spawner, verifies
the original polygon/depth and ceiling overlap, corrected clear box, unchanged velocity, real
physics at the reported delta and three seconds of subsequent session updates.
`services.start_departure` also checks no-ground placement, idempotence and atomic
failure beyond the correction budget. See the [retake evidence](../../../Documentation/Implementation/demo_retakes.md).
