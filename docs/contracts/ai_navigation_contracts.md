# Original CMovement cache and obstacle paths

Selected original `ObjectDLL/movement.cpp` is now integrated as the CBaseCharacter
aggregate `m_MoveObj`. It is distinct from the player's client MoveMgr and from
PathMgr's authored PathPoint snapshots. See [source selections](../implementation/sources/m2_ai_navigation_sources.json),
[evidence](../../../Documentation/Implementation/m2_ai_navigation.md),
[object lifetime](object_contracts.md), [collision](model_and_collision_contracts.md)
and [AI state actions](ai_state_contracts.md). No production NPC is registered;
M2 remains incomplete.

## Transform cache and ownership

The aggregate is added after damage and inventory in the original constructor
order. It samples position, previous position, rotation and basis on MID_UPDATE,
after the character's own handler, without moving the body or advancing clocks.
Cache getters return that sample, not an implicit current-transform query. Sampling
does not refresh an existing route when position or animation dimensions change. Normal
inactive objects receive no scheduled update. An explicit Update still validates
and samples a live inactive owner. Pending/stale owners fail and invalidate the
sample; failure does not turn a historical value into a valid pose.

Init checks a live identity and immediately samples its transform. This deliberate
adaptation removes the original uninitialized current transform before its first
update. Previous position starts at zero. Valid reinitialization clears the prior
route safely. Source CPlayerObj does not initialize this aggregate; the port keeps
it uninitialized and silent for players. Original AI InitialUpdate calls Init, but
that full initialization path is still unported. Only explicit diagnostic actors
initialize the aggregate currently. Existing AI helpers retain their documented
current-transform adaptations; this increment does not silently switch their timing
to the new cache.

Path points are owned vector values, capped at 64, replacing DLinks whose original
removal leaked separately allocated DVectors. GetNextPathPoint copies the first
value and preserves output when empty. MoveToNextPathPoint consumes the first and
returns whether another remains; consuming the last returns false, and an empty
call is safe. The original accumulated point count is not decremented by consumption
or consolidation. Term clears points/count but does not disable transform sampling.
No owner pointer, point pointer or object ownership survives callback removal.
A returned path span is read-only and expires on mutation/destruction; callers
needing persistence must copy it and still discard it at world transitions.

## Preserved algorithm

CalculatePath rejects the world origin before clearing the previous route. The
same preflight preservation applies to invalid coordinates/owner/sample. After
preflight it clears the route, reads dimensions and uses the cached position.
ClearToPoint constructs right/forward from direction and cached up. Its width is
`sqrt(dims.x² + dims.z²) + 0.1`. It checks the right side first, then the left.
**There is no center ray, swept body, vertical/head clearance or floor test.** A
thin central obstruction can pass this original test; success is not proof that
an actor can physically traverse the route.

Clearance ignores the owner and all CBaseCharacter descendants and requires solid
objects. Its character filter does not exclude glass or VolumeBrush classes.
Current container services enforce nonsolid volumes; those therefore pass the
solid-only query. The existing AI visibility filter's glass/liquid rules remain
separate. Primary-world geometry is never removed by either object filter.

An obstruction yields a point offset along its normal by width minus 0.1.
FindShortestTurn casts left then right for maximum search distances. A missed
infinite ray means zero distance, as in source, not unlimited open space. Searches
start one width away, testing right before left at each increment, with forward
probe length width+5. A clear side overshoots by another half of the current search
width and turns facing the opposite side. FindTurn advances along the negated
obstacle normal, probes the facing direction, and adds an extra forward step after
a clear probe. On exhaustion it can add an intermediate point and rotate its search;
the source accumulated-count >=10 rule bounds recursion. Final two-ray clearance
must succeed before appending the turn and destination. Later failure preserves
partial points. Callers must check the return/result and must not follow those
partial routes as success.

ConsolidatePath is optional and is not called by CalculatePath (the source call
is commented out). It tests cached position to the second point, then successive
point-to-point shortcuts. These rays have no character/owner filter: even the
owner's enclosing solid body can prevent the first shortcut. Source removal/order
and accumulated-count behavior are retained.

## Typed service and safety bounds

CastRay uses the existing collision domain, clipping an infinite direction to the
first boundary of ±10 million units. It queries primary geometry plus live solid
objects with the existing closest-hit implementation. CastWorldRay stays primary-only;
both share endpoint construction. Invalid input, missing collision, hit and miss
remain distinct. No arbitrary navigation range or invented hit is supplied.

Zero/invalid direction or up, nonfinite/out-of-domain points or direction vectors and negative dimensions
fail. Valid parallel axes use the [original AlignRotation fallback](ai_alignment_contracts.md),
superseding the earlier port's blanket degenerate-axis rejection. Searches allow at most 4,096 iterations and 8,192 submitted ray queries per
operation; diagnostics include the rejected extra attempt when a budget is exceeded.
Precision-stalled increments fail. Service failure aborts the algorithm rather than
being interpreted as a clear ray. Public clearance/consolidation start fresh query
budgets; internal path queries share the entire plan's budget. Outcome/error report
the last query/plan operation; transform sample validity is shown independently.
Collision remains the documented replacement engine solver, not verified retail
physics. These limits are explicit portability/safety adaptations.

## Inspection and remaining scope

The Skeleton panel exposes initialized/sampled state, cached/current-previous
positions, forward vector, samples/failures, last destination/outcome, remaining
points versus accumulated count and ray/search work. Hidden/inactive/missing-model
records remain inspectable. No inspection control moves an actor or runs a query.
The existing GPU character diagnostic now plans an isolated direct route, samples
through callbacks and checks results while hidden and after reload. This is labelled
diagnostic placement; the headless private case separately queries real SubwayA
spawn geometry through two load/unload cycles.

FindPosAroundObj remains excluded because its required GetPointShade service is
missing. Original debug-line object spawning, save/restore, autonomous following,
full AI initialization/update/deactivation, production CivilianAI/CultistAI/Gideon,
required FX and ExitTrigger remain open. No authored class counts change.

The [autonomous locomotion continuation](ai_locomotion_contracts.md) now supplies
walk/run/moving fire, obstruction/ledge probes and Cultist close/far attack sequencing.
Earlier blanket omissions are superseded only for this subset. Full initialization,
AI_Update/deactivation, remaining states and shade recovery stay unavailable.

The [AI recovery/navigation continuation](ai_recovery_contracts.md) now executes
jump/crouch/crawl and seven destination/recovery/visual-search states. Earlier
blanket omissions for those bodies are superseded. Failed partial routes remain
unusable; shade recovery, other states and full production AI_Update remain open.
