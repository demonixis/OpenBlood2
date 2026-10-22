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

Retail PingObjects (disassembled from the retail Server.dll: the ServerDE entry calls a
routine that finds the observer's BSP leaf and walks that leaf's compressed visibility
bitstream) refreshes every object in a visible node: no range or field-of-view limit, no
ray, and objects never occlude. The port does not decode the DAT visibility data. Its
replacement tests up to four primary-world segments between the observer and the
candidate (centre to centre, observer box top to centre, centre to candidate box top,
top to top, box top = centre + dimensions.y) and the first clear one makes the candidate
visible. Only baked primary-world polygons block (glass ones included); world models
(doors, destructible brushes, train cars), props, characters and volumes never do. This
replaces the earlier single SolidOnly center ray with CanSee's glass/volume filter,
which any fence, prop or train brush on the line blocked, so that about half of the NPCs
a player could see 1000 units away never woke and could be shot without reacting.

Damage and sound alerts still reach inactive actors (object messages bypass activity,
as in the retail SendToObject) and still do not wake them, as in the original; the
retail behaviour only looked different because everything visible was already awake.
No range/FOV, proximity activation, wake-on-damage or forced active state is added.
Weapon/USE rays and CanSee keep their existing filters.

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

## Ping cost

The 0.5-second cadence stays; queries are not spread across frames. Each candidate costs
at most four primary-world sweeps and no object scan, which replaces the earlier prepared
IntersectSegment batch. The adapter exposes `activation_ping_ns`/`activation_ping_calls`;
`--bench` reports the per-frame cost ([hitch report](../implementation/reports/main_thread_hitches.md)).
`ai_runtime.ping_private` recomputes the four segments for every probe of 40 pings each
in SubwayA and CityHub and checks the committed refreshes.

## Observable acceptance and limits

Synthetic tests check that solid objects, destructible/indestructible glass,
wood/door FireThrough and solid diagnostic Water/Ladder volumes never block, that a tall
baked wall (glass included) blocks and a low one is cleared by the box-top segment, and
cover manual inactivity, invalid-ray batch rollback, unavailable collision and observer/
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
