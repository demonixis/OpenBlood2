# CMovement parallel-axis alignment

This corrects the local engine-service adaptation used by original CMovement
clearance and obstacle-turn planning. See [source hashes](../implementation/sources/m2_ai_alignment_sources.json),
[navigation contracts](ai_navigation_contracts.md) and
[evidence](../../../Documentation/Implementation/m2_ai_alignment.md). M2 remains incomplete.

## Source behavior and implementation

Blood II's server SDK explicitly requires AlignRotation to choose another up vector
when the reference is parallel to the requested direction. The original movement
code invokes that service for both its destination direction and an obstacle normal.
A horizontal surface can therefore produce a valid parallel-axis request.

The previous port rejected this case as Invalid, which propagated through navigation
to a latched AI_Update failure. The local frame builder now normalizes direction and
reference up; when their absolute dot product exceeds 0.99, it perturbs and normalizes
the reference in the order z+5, then x+5/y+5 if needed, then x+5/y-2/z-5 if needed.
This order and threshold come from the available later LithTech
gr_GetPerpendicularVector/BuildFrameOfReference implementation. They are corroborating
engine evidence, not a verified trace of Blood II's 1998 binary.

Only the right vector is needed here. Normalizing cross(reference, forward) is
equivalent to first removing the reference's parallel component and then crossing.
Keep the port's existing +X right convention; do not import the later engine's
opposite cross order. No quaternion, object rotation or physical movement is changed.
The fallback is bounded and allocates nothing.

Zero, nonfinite and out-of-domain inputs, unavailable collision and stale owners
remain failures. Existing search budgets, right/left ray order, source turn offsets,
partial-route ownership and the failed-plan guard are unchanged. A valid frame does
not guarantee a route: the two recorded CityHub requests return Blocked, allowing
the existing original state transition to Idle instead of suspending AI_Update.
Never report partial points as a successful route or remove actual collision.

## Inspection and validation

Each CMovement instance counts successful parallel-axis resolutions and retains
the last normalized direction, original cached reference up and resolved right.
These are historical values, not necessarily the last plan; Init/Term retain the
counters like the existing sample/failure totals. They cannot mutate simulation and
expire with the owning object. The Skeleton inspector displays them, including for
hidden/inactive actors.

Synthetic tests cover parallel/antiparallel/near-parallel directions, a rotated
local up requiring the second perturbation, unchanged ordinary horizontal queries,
invalid vectors, missing collision, deferred removal and no owner motion. Private
tests recreate two recorded planner requests against CityHub DAT and require ordinary
Blocked outcomes through reload/unload with no retained objects, clients, messages
or strings. These are diagnostic inputs, not authored placements or a playthrough.
The existing application character smoke exercises the fallback outside the inspector
and retains its hidden/reentry checks in Debug and inspector-free Release.

This correction adds no AI states or classes and changes no shaders, dependencies,
commercial assets or reference files. Other unavailable AI actions, physics parity,
complete CityHub traversal and the remaining M2 gate are still open.
