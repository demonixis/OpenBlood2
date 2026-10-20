# Original NPC ammunition recovery

`AI_Mgr::AI_STATE_FindAmmo` and the original `MID_ADDAMMO` receiver are now
connected. M2 remains incomplete. See [source hashes](../implementation/sources/m2_ai_ammo_sources.json),
[execution evidence](../implementation/reports/m2_ai_ammo.md),
[navigation contracts](ai_navigation_contracts.md) and
[pickup contracts](pickup_contracts.md).

## Original decisions and phases

`MC_BestWeapon` already selected FindAmmo when the current ordinary weapon had
an empty raw ammo pool. AI_Update previously suspended on the missing dispatcher
case. The state now runs through the existing original navigation/animation calls:

1. With no current weapon, request Idle. Otherwise select the original ammo class,
   search the ammo intrusive list using AvoidTarget, Visible and Specific flags,
   and escape when no candidate exists. Preserve the original `RocketAmmoPU`
   spelling for Howitzer ammo; the absent class is an empty search, not an alias
   for `HowitzerAmmoPU`. Unsupported ammo types likewise have no candidate.
2. Face the selected pickup and calculate a path, then face its first point on
   the next call. An empty path requests EscapeRun.
3. Run toward the flattened point using the original inclusive half-diagonal
   arrival threshold. Preserve the first ledge check, short-circuited second
   check after consuming a point, phase 2 for further points and facing the
   cached combat target at the last point. Movement sets velocity only; normal
   engine integration remains single.
4. Recompute the original subclass state. An empty phase-3 route also recomputes.

The source ignores CalculatePath failure. The port clears partial points on an
ordinary blocked result so the next phase uses its original empty-route escape.
Invalid input, unavailable collision and exhausted bounds remain failures.
No rejected partial route becomes successful navigation.

Specific class selection leaves `fObjectDp` equal to zero in the original
FindObjectInRadius. Its AvoidTarget predicate can therefore never reject, but
it reads an uninitialized forward vector before reaching that result. The port
skips that unused computation. It preserves the reversed IsKindOf arguments,
list order, ties, distance and visibility checks; other avoidance queries retain
their sampled-target validation. This supersedes the earlier blanket rejection
of Specific+AvoidTarget. It is a defined safety adaptation, not a retail binary
random-stack-behavior claim.

Original CanSee still requires a target ray hit. Ordinary nonsolid ammo pickups
are not made solid or ray-hittable by this increment, so a search may legitimately
find nothing and choose escape. The positive route test explicitly adds RAYHIT
to a synthetic pickup; that is branch coverage, not proof of retail pickup vision.

## Collection and checked lifetime

The original AI message receiver accepts ammo only while alive and without
NOAMMOCOLLECT (0x200). AddAmmo retains its attribute cap and full-reserve refusal.
Only accepted amounts produce the typed picked-up acknowledgement. The pickup
accepts the AI acknowledgement only for ammo, during its matching active contact.
Player health, weapon, item and key handling retain their existing restrictions.
No NPC powerup collection or invented health-pickup list is introduced.

The typed receiver additionally checks a live AmmoPickup sender, matching payload
and active offer to this actor. Replayed/out-of-contact packets cannot grant ammo.
Removal remains deferred. Sound and authored trigger dispatch keep the existing
sound-before-trigger order; the detached voice outlives pickup and actor removal.
No borrowed sender access occurs after the acknowledgement callback.

A pickup tracked during FindAmmo may disappear because this actor or another
collector removes it. AI_Update identifies that exact selected pickup even after a state change and
clears its stale non-owning tracker while
retaining the owned route and historical coordinates, then continues the state.
It does not dereference the dead object or suspend the actor. This is a bounded
lifetime adaptation to the original unchecked tracked-position query. Other
tracked-object failure contracts remain unchanged; no general resurrection or
silent failure recovery is added.

## Inspection and limits

The Skeletons navigation section shows search count, ammo type/class, accepted
and refused offers, removed tracked pickups, and the last checked collection
source. Existing phase/path/target/query/error inspection remains available for
hidden and inactive actors. Counters advance through gameplay, never UI drawing.

Private tests use real Cultist/player/pickup models and WAVs in an explicitly
synthetic arena, repeated through unload/re-entry. The NVIDIA pickup smoke also
collects through MoveObject contact with the inspector hidden and removes both
participants before checking audio. Neither test claims autonomous authored
pickup pursuit, full campaign traversal, or a fix for every reported idle NPC.
FindHealth/AssistAlly and other missing selected services remain separate. No
shaders, dependencies, commercial assets or reference sources are changed.
