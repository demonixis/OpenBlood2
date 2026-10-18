# Collision traversal and post-transition work

The replacement collision engine now rejects BVH branches and leaf triangles
using conservative segment-versus-expanded-box intervals before the unchanged
triangle SAT. This addresses measured CityHub activation-query spikes; it is not
a change to original AI cadence, range, visibility filters or state transitions.
See [evidence](../../../Documentation/Implementation/m2_collision_traversal.md).

## Query invariants

Keep the existing coarse node AABB rejection. Intersect the actual center segment
with bounds expanded by the query half dimensions and 1e-6 spatial tolerance.
Use the same float displacement as SAT, double interval calculations, 1e-10
stationary threshold and inclusive 1e-9 interval tolerance. This is deliberately
conservative: SAT still decides contact, initial penetration, normals and timing.
The triangle bounds check must not use the old strict coarse node test; narrowphase
stationary-axis tolerances can accept points just outside a triangle's raw bounds.

Once a hit is found, use its fraction as the upper interval bound without changing
the segment or narrowphase displacement. Retain left/right and leaf visitation
order for surviving candidates. Equal-time contacts remain eligible; the lower
source polygon index still wins. Initial overlap at fraction zero must not prune
other overlapping triangles. No radius, skin, dimension or collision rule changes.
Body placement, transformed world-model indices and container boundaries use this
same traversal. The existing bounded placement searches and lifetime/atomic-motion
contracts remain in effect.

## Inspection and limits

`CollisionWorld::work()` reports cumulative node tests/rejections, triangle-bound
tests/rejections and full triangle SAT tests since build/clear. Counters allocate
nothing, retain no objects and advance no clock. They include internal placement
queries, not just public sweeps. They are diagnostic data on the existing single
simulation thread; reading the inspector performs no additional collision query.
Transforms preserve totals; clear and successful/failed rebuild reset them.

Services displays the primary-world index's totals. Separate world-model indices,
object-box intersections, rendering and GPU work are excluded; this is not a total
frame profiler. Invalid/empty sweeps perform no node/triangle work.

The renderer still skins and expands visible ABC geometry on the CPU every frame,
including packed vertex deformation, then uploads it through SDL_GPU. Hidden
objects skip geometry work; visible off-screen models have no frustum rejection.
CPU skeleton poses are also needed independently for model-node queries and
attachments. This collision change does not claim GPU skinning, rendering culling,
retail physics equivalence, universally smooth transitions or M2 completion.

## Departure from deep initial overlap

This extends the shallow [contact overlap tolerance](contact_overlap_contracts.md):
overlaps up to `allowed_penetration` (0.01 in MoveObject) keep that policy. Deeper
start overlaps follow a `StartOverlap` policy. `Block` (the default) keeps them a
`start_solid` failure. `Leave` ignores a triangle the box already overlaps when a
nonzero displacement leaves it along its outward normal (`outward . delta > 0`).
`LeaveOrSlide` also ignores it for motion parallel to the face (`>= 0`). Moving
deeper and zero-length queries always fail, and a box center inside a closed
container component stays start-solid. Triangles are wound toward their polygon
plane normal at build (the empty side); all retail DAT polygons
already wind that way, synthetic brushes may not, and the two-sided SAT is unaffected
by the order. Container boundaries are built from rotated planes. Ignored faces are
counted in `CollisionHit::departed`; later hits are unaffected.

`MoveObject` uses `Leave` for solid bodies and `LeaveOrSlide` for non-solid passive
objects (pickups, debris), which never block others and only need to settle; stair
probes, movers, world bodies, body placement, resize clearance and every other query
keep `Block`. A solid body that slides along or pushes into a deep overlap therefore
still fails, as the contact-overlap contract requires.

Evidence: GoToStartPoint teleports the player to `05_egress` (retail 05_CDMB),
112 units above the `endelevator` floor, where the standing box
crosses the car ceiling by 25 units. The source then lowers it with
MoveObjectToGround, a move leaving that ceiling. In 12_Train_Station the authored
`SniperRiflePU` lies with its 33-unit half-length through a wall; its placement and
gravity steps move parallel to that face. The original pickup also falls back to a
raw `SetObjectDims` when its size does not fit. Authored data, start rules and
positions are unchanged; nothing is teleported or pushed out. This mirrors later
LithTech's mover, whose one-sided world-model polygons ignore contacts facing along
the motion (`moveplayer.cpp`); it is replacement policy, not a verified 1998 engine
trace, and general depenetration remains unavailable.

Movement snapshots and the Movement inspector report the faces a body's moves left
or slid along (`start_departures`). Tests: `services.start_departure` (synthetic
ceiling/floor/wall, solid and non-solid), `services.contact_overlap` (deep solid
slides still fail) and `services.elevator_entry` (retail 05_CDMB player entry);
`keyframe_audio.private` steps 12_Train_Station.
