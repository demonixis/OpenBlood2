# NPC activation visibility and the authored Gideon intro

This extends [NPC runtime](ai_runtime_contracts.md), [AI visibility](ai_sight_contracts.md)
and [Gideon lifecycle](gideon_runtime_contracts.md). See the
[source ledger](../implementation/sources/m2_activation_visibility_sources.json) and
[execution evidence](../implementation/reports/m2_activation_visibility.md).
M2 remains incomplete.

## Replacement visibility policy

The original ServerDE header describes PingObjects as activation of objects seen
by its observer. It also gives manual INACTIVE/INACTIVE_TOUCH priority over pings.
The original player selects the active server camera, otherwise itself, strictly
after each 0.5-second deadline. AI_Update configures a three-second deactivation
period and initially auto-deactivates; its existing sleep-gap desynchronization
and pending update deadline remain unchanged.

The port does not decode retail visibility/PVS data. Its existing center-ray
estimate now uses SegmentFilter::AIVisibility, reusing the original CanSee
IGNORE_LIQUID | IGNORE_GLASS object filter. This is a documented replacement-engine
policy, not an assertion that retail PingObjects used these rays or this callback.
It fixes the authored SubwayA camera-to-Gideon ray through nglass1.

Every VolumeBrush descendant is excluded. A CDestructableBrush is excluded only
when Destructable is true and GetSurfaceType returns SURFTYPE_GLASS. CanDamage and
FireThrough do not determine transparency. Indestructible glass, wooden FireThrough
brushes, doors and primary-world glass polygons still block. The filter is based
on original game properties, not decoded texture alpha or renderer visibility.

Preserve SolidOnly candidate intersection, observer exclusion, actual primary
world/world-model geometry and object bounds. A miss or hit on the candidate is
visible. This differs deliberately from CanSee, which requires a target hit and
can retry a head ray. No range/FOV, extra head ray, proximity activation or forced
active state is added. Weapon/USE rays keep their existing filters.

## Atomic refresh and historical diagnostics

Ping snapshots checked live identities with automatic deactivation enabled and
activity ACTIVE or AUTODEACTIVATE_NOW. It excludes the observer and both manual
inactive states. Collect visible candidates before refreshing any activity,
deactivation deadline or successful-ping counter. An invalid/unavailable query
fails the whole refresh; it must not partly wake actors. No callbacks run inside
these queries. Commit by checked identity, preserving deferred removal.

Each eligible candidate retains one bounded ActivationProbe: query count, simulation
time, copied endpoints, checked observer/hit IDs, polygon, fraction, outcome and
whether that batch committed its refresh. Diagnostics are updated as queries run;
a visible result with refreshed=false truthfully records an aborted batch. Candidates
not reached after an error retain older records. Invalid observer or unavailable
collision at preflight does not fabricate a new per-candidate query. The player's
existing activation_ping_failures records the failed service call.

Probe IDs own nothing and remain historical after removal. No handle dereference,
object link, message or resource ownership is added. Unload removes the record
with its object. The NPC lifecycle inspector resolves IDs before displaying names
and labels absent identities as removed. It shows outcome, committed refresh,
time, endpoints and hit details without issuing queries or advancing simulation.
Blocked results and unqueried objects stay visible in inspection.

## Observable acceptance and limits

Synthetic tests exercise destructible/indestructible glass, CanDamage=false,
wood/door FireThrough, solid diagnostic Water/Ladder volumes, baked glass geometry,
manual inactivity, invalid-ray batch rollback, unavailable collision and observer/
world removal. The ordinary solid ray still hits the same glass object.

Three 13-second actual SubwayA runs retain the authored objects, scripts, cameras,
health and timing. At two seconds camera1's ordinary ray hits nglass1 while the
committed activation probe sees Gideon; original script updates have progressed.
Gideon reaches PASSIVE before camera1 changes, and the authored camera sequence
returns player control. The GPU viewport repeats the script-state check after
initial entry and world reload; the normal hidden-inspector rendering checks remain.

This supersedes the previous two-update/zero-ping Gideon observation for this
sequence. It does not prove a complete boss encounter, death parity, portal/PVS
activation, all camera/occlusion arrangements or missing world services. Safe
NEW_LEVEL, remaining required services and the actual M2 exit gate stay open.
