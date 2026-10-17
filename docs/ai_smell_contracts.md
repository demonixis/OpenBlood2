# Original player smell trail and AI search state

The player now executes the original periodic SmellHint block, and the shared AI
dispatcher executes `AI_STATE_SearchSmellTarget`. Earlier navigation transitions
could select that state and then fail because its body was unavailable. This is
a bounded service continuation; **M2 remains incomplete**.

See [source selections](m2_ai_smell_sources.json),
[evidence](../../Documentation/Implementation/m2_ai_smell.md),
[perception](ai_perception_contracts.md), [navigation](ai_recovery_contracts.md)
and [player entry](player_contracts.md).

## Player trail

After the existing player update work, a living, nonspectating, client-associated
player checks the strict `now > smell_time + smell_delay` deadline. The normal delay
is 0.5 seconds. Compare the current position against the **last global list entry**,
not a player-owned previous hint. The original distance is three-dimensional and
inclusive at 10 units. No existing hint means an unconditional drop after the
deadline. Being too close does not advance the clock. Creation uses identity
rotation and the current player position; the clock advances before creation,
including on failure. No movement or extra physics step is added.

Dead/spectator guards skip the block. The spectator boolean supplies the original
guard only; it does not implement spectator gameplay or extend supported save
profiles. Nonfinite/negative clocks, missing transforms/lists and failed creation
return failure and retain a specific diagnostic. As with other required player
update failures, the existing callback boundary rejects the update; there is no
success-shaped resource fallback.

The existing initial hint and original 20-second timeout remain. The list appends
in creation order. Checked bounded traversal skips pending removals before the
end-of-frame destruction; raw list links never survive across callbacks. Completed
entry leaves hints owned by the world, including after the player disconnects.
Failed entry still rolls back its initial hint. Existing player snapshots retain
relative smell time and delay, not old hint identities or diagnostic counters.
Destination preparation retains its existing independent list ownership.

## Search state and source reachability

The source order is significant: compute stimuli; decide again if there is no
combat target; decide using another positive stimulus; decide again if there is
no smell cursor. Normal smell perception clears the target and **does not seed a
cursor**. Consequently this port does not invent automatic trail acquisition.
Registered NPCs also retain their original initialized smell range of zero.

For a valid explicitly seeded cursor, phase 1 repeats the source visible-hint
query, updates its link and faces that position. Ordinary SmellHints are not
changed into ray-hit geometry. A missing visible hint requests a new decision.
Phase 2 preserves the double, throttled ledge query, cached actor Y, inclusive
`(hypot(dim.x, dim.z) + 0.1) / 2` arrival threshold and original Run command.
Arrival advances in insertion order. Facing a successor increments phase 2 to 3;
it does not reset phase 2. Reaching the sentinel also increments to 3, whose
next update requests a decision. No path sorting or extra integration is added.

The old source cursor is a retained DLink with inconsistent link bookkeeping:
it breaks the successor's link instead of the old link, fails to keep the linked
handle synchronized and clears a different tracking field on link break. The port
uses one checked non-owning HOBJECT, acquires a successor link before releasing the
previous link, and clears the cursor before the original Idle transition when the
hint disappears. Stable IDs reject removed/reused handles. Owner removal leaves
world hints alive. This is an explicit lifetime repair, not claimed binary parity.

Failed perception remains a failed service, never a useful stimulus or decision.
Existing navigation guards reject recursion, overflow, missing movement caches
and owner removal during virtual decisions. Inspection reads historical counts,
exit reason, cached position and current checked identities without querying or
advancing AI. The player view exposes the actual clock, drop/check counts, global
list count, last checked drop identity and failure text. Hidden/inactive hints
remain selectable through ordinary object inspection.

## Validation boundary

Synthetic tests cover strict time/distance boundaries, global-tail behavior,
pending removal, death/spectator guards, invalid clocks, independent lifetime,
expiry/reentry, source decision order, actual perception failure, visible-query
refusal/success, double ledge throttling, Run commands, successor/sentinel phases,
link replacement, stale handles and recursive decisions. Route-body fixtures use
actual damage memory of a dead character to retain the previous target, as the
source does; they do not override ComputeStimuli. A diagnostic ray-hit hint is
explicitly limited to the visible-query positive test. Neither setup is presented
as autonomous authored smell tracking.

Private checks complete the original SubwayA intro and move the actual player
through the existing MoveMgr on two load/unload cycles. Six real character models
also exercise the state exit. Debug, inspector-free Release, sanitizers and NVIDIA
regressions are recorded in the evidence. Retail smell reachability, full AI save,
spectator/multiplayer and the remaining M2 services are not certified by these tests.
