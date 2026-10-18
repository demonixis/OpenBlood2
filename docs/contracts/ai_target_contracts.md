# Original explicit AI targets and facing

> Extended by [AI perception contracts](ai_perception_contracts.md): original
> candidate lists and stimulus/target selection now execute. Automatic NPC states
> and complete M2 remain unavailable; earlier acquisition omissions are historical.

M2 remains incomplete. This ports AI_Mgr::SetTarget, SetNewTarget,
MC_FaceTarget, MC_FacePos and its target MID_LINKBROKEN branch. The isolated
RefreshTargetPosition call selects the GetObjectPos statement from AI_Update;
it does not implement that update, autonomous perception or a state machine.
See [source selections](../implementation/sources/m2_ai_target_sources.json),
[execution evidence](../../../Documentation/Implementation/m2_ai_targets.md),
[object lifetime](object_contracts.md), [sight](ai_sight_contracts.md) and
[attachments](attachment_contracts.md). No production NPC is registered.

## Source rules

SetNewTarget changes only the checked target identity and directed engine link.
Reassigning the same live target is a no-op; it does not sample its position or
change orientation. Clearing the target breaks the old link without removing
that object. The target member is private so new callers must use this path.
GetTarget returns an identity, not a borrowed C++ object pointer.

SetTarget searches an exact, case-sensitive name and takes the first returned
match. The current engine's list prepends matches from slot enumeration, including
hidden/inactive live objects; it excludes pending removals. Its tie order is not
claimed equivalent to retail engine enumeration. An empty result is the source's
silent return: it retains the old target, counts an unmatched name (inspected
beside failures) and succeeds, so authored TARGET commands still select AttackFar
and its null-target guard. A named actor may already be dead or removed; this is
not a service failure. An unavailable list service still fails. The copied name is limited to the
engine's 100-byte object name boundary; empty, oversized and embedded-NUL names
reject. Lists are always relinquished, including failures after selection.

After selection, SetTarget samples the target position and calls MC_FaceTarget.
It cancels that method's Metacmd increment, preserving the original net zero
command advancement. MC_FaceTarget uses the stored position; it does not silently
refresh a moving target. With no target it only increments Metacmd. A selected
but never sampled target rejects instead of using an uninitialized/zero vector.
RefreshTargetPosition marks a failed sample unavailable and retains its old
coordinates as historical inspection data. No automatic tracking loop is added.

MC_FacePos computes atan2(direction.x, direction.z) minus the observer's current
forward yaw, then rotates about its **local up axis**. The angle is not wrapped or
rate-limited. Existing pitch/roll therefore affect the result; replacing this
with a world-Y look-at would change behavior. Vertical noncoincident directions
retain atan2(0,0)'s zero yaw. The original normalization does not affect this yaw
ratio and is omitted to avoid overflow; coincident/nonfinite directions reject.
The MoveObj aggregate is not integrated, so checked current position/rotation
replace its cached transform. The existing quaternion helpers implement the
rotation; later LithTech matrix code was consulted as reference only.

Successful direct face commands increment Metacmd once. Counter overflow, invalid
owners and a rejected rotation service apply no rotation or increment. Attached
observers cannot rotate independently, per existing attachment contracts. Facing
does not translate, integrate physics, advance an animation or dispatch model
keys. Explicit commands can run on hidden/inactive live objects.

## Lifetime and service failures

Unlike the original unchecked break/assign/create sequence, SetNewTarget validates
and acquires the new link before releasing the old link. Creation invokes no game
callbacks. A rejected link leaves the previous target and link intact. Null clears
are supported; self, pending, stale and foreign identities reject. The same-target
no-op does not repair an externally broken link: callers must not break an AI's
target link behind its back. There is no ownership of the target object.

Removal immediately makes target queries fail; the original MID_LINKBROKEN callback
clears the identity and sampled-position flag at deferred reclamation. Unrelated
broken links leave the target alone. The callback then forwards through the original
base/aggregate chain. Reentrant callback target changes/removals use the existing
safe worklist. Owner removal/world unload releases links without killing an
independent target. Target assignment followed by a later position/rotation failure
can retain the selected target, matching original operation order; it is reported
as a failure, never an executed face command.

GetObjectPos now returns a typed DRESULT. Existing ignored-return callers retain
their output behavior: invalid identities and unresolved attachments write zero;
valid records return their current position. A null output is invalid. This does
not change the existing read-only access to a pending record before reclamation;
AI callers separately require Alive identities. Required AI target samples check
the result. CanSee also checks its origin fallback and target position: unresolved
poses produce Unavailable with zero rays instead of querying an invented origin.
No generic successful SDK stub or unbounded raw pointer is introduced.

## Inspection and verification scope

The Skeleton panel reports target lifetime, last named request, outgoing link
count, stored position/availability, Metacmd, completed turns, last yaw delta,
assignment/link-break/failure counts and error. This panel remains available even
when the selected actor's model is missing. It reads recorded state only.

Synthetic checks cover duplicate names, exact case and no match, cached versus
refreshed positions, no-target commands, cardinal/tilted/vertical facing, inactive
objects, overflow/nonfinite/degenerate inputs, injected service failure, unresolved
attachments, deferred and reentrant removal and repeated unload. Existing AI
model-key checks verify pending-target rejection followed by the no-target branch
after the actual link callback. Private tests sample all six NPC model catalogs
and exercise real player leave/re-entry and world reload in SubwayA. These are
labelled probes, not replacements for the missing authored NPCs.

The existing GPU character scenario binds its diagnostic AI probe to the real
player, samples/faces it while the UI is hidden and checks fresh identity after
reload. Headless and inspector-free checks exercise the same commands. No AI
selection rule is inferred from the probe's explicitly assigned target.

SubwayA source accounting remains 187 created, four transferred world sounds,
372 editor-only, 17 unsupported and zero failed, plus seven SelfTriggers. Missing
CivilianAI, CultistAI, Gideon, ExitTrigger, required client FX and other services
keep --start-game at exit 6. Autonomous target acquisition/retention, death/state
transitions, script execution, navigation, attacks, track/smell links and retail,
other-platform or XR parity remain unverified/unimplemented.

The [escape/guard continuation](ai_escape_contracts.md) supplies selected original
escape/hide/guard and beg bodies, plus checked non-owning tracking. Earlier blanket
omissions for those bodies are superseded; full NPC initialization/update remains open.
