# Collision traversal and post-transition work

The replacement collision engine now rejects BVH branches and leaf triangles
using conservative segment-versus-expanded-box intervals before the unchanged
triangle SAT. This addresses measured CityHub activation-query spikes; it is not
a change to original AI cadence, range, visibility filters or state transitions.
See [evidence](../../Documentation/Implementation/m2_collision_traversal.md).

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
