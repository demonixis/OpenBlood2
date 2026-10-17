# Play entry and first Cultist combat corrections

> Continued by [doorway clearance](corpse_clearance_contracts.md): a source-center
> alternative requires an additional swept living-box check. Existing raw dimensions,
> bounds and final collision checks remain mandatory.

See [evidence](../../../Documentation/Implementation/m2_playtest_input.md) and the
[source ledger](../implementation/sources/m2_playtest_input_sources.json). M2 remains incomplete.

## Mouse ownership

The inspector's player-entry actions now enter play: enable controls, request
relative mouse mode and hide inspection after SDL accepts capture. Play / resume,
F3 and hiding/closing the inspector explicitly request the same ownership. Folding
retains the cursor for the title arrow; keyboard movement remains available.
A startup play request waits for focus. Explicit focus loss, Escape/F1 inspection,
disabled controls, absent player and text editing cancel capture as documented in
[player control contracts](player_control_contracts.md). Focus gain alone does not
resume a previously released session. The focus/entry click is not delivered as a
shot. SDL failures remain visible; logs record actual capture changes and driver.

Mouse motion, left/right buttons and keyboard fallback still feed the original
client rotation and view-weapon FIRE_KEY path. No new damage or fire-rate rules
are introduced. The authored intro retains its camera restriction and duration.

## Original Cultist dodge and rolls

Gun-impact hearing could already select StateDodge (10), but dispatch previously
failed and suspended the actor. CultistAI::AI_STATE_Dodge now uses the original
three phases: CalculateDodge, a lateral roll if ROLL is set, then ComputeState.
RIGHT wins over LEFT; DODGE, forward and backward cases skip the roll, as in source.
This is not a successful placeholder for other subclasses' missing dodge states.

CalculateDodge normalizes impact-minus-cached-position and compares cached right,
left, forward and backward dot products. Forward/backward override the lateral
choice only with strict greater-than comparisons. The opposite direction is cast
for RollSpeed units. The original INTERSECT_OBJECTS request includes nonsolid
objects. The typed ray uses the existing SkipInterior entry-face policy, no object
filter and no fabricated owner/world sentinel. That is replacement-engine policy,
not verified retail intersection parity. Miss selects ROLL (0x10), hit DODGE (0x20);
missing collision is an error, never a clear route. Coincident and vertical-only
impacts are explicit failures instead of using the original uninitialized endpoint.

MC_Roll_Left/Right retain cached right-vector movement, speed, commands 15/16,
nonlooping clip selection and the initial velocity write. Continuation checks
obstruction, independently refreshes velocity when standing with gravity, then
checks playback completion. Obstruction clears animation and advances the phase
without an early return. Playback completion stops horizontal velocity and advances
again, so both increments may occur in one call. Existing typed standing state
represents primary-world support without inventing an object token. Clip/resize,
cache, support and pending-owner failures remain explicit. Commands never integrate
position themselves. Reentrant execution and phase overflow are rejected.

The Skeleton panel exposes dodge state/query counts, flags, ray and result alongside
existing locomotion errors. Queries do not run from the UI. No mutable global is
added, so destination rehearsal retains its existing isolation boundary.

## Death placement and validation boundary

A second failure followed an accepted lethal shot: nearest-axis corpse placement
oscillated between train-seat faces and retained the suspended source NPC at zero
HP. The [placement contract](corpse_placement_contracts.md) now permits a bounded
search among other separating axes, including exact touching fits. Original raw
body dimensions, source-side constraints and final box/center-path checks remain
mandatory. Successful source removal and the independent corpse are both checked.

The private mouse-combat test loads the authored SubwayA world twice, waits through
the real intro, walks from the normal spawn with W/E, aims via relative-motion
input and holds only left or right mouse fire. The selected enemy is the authored
Cultist gak. It changes no NPC health, armor, flags, transform, inventory, activation
or target; it sends no damage packets directly and uses no player teleport. It
checks player-attributed health loss, zero HP, source removal, a valid corpse,
FIRE_KEY/ammo/audio, and cleanup across reload. The secondary run must execute
Dodge. A real SDL-window variant checks native relative-mode state; input events
are synthetic SDL events, not physical-device automation or a human walkthrough.

This proves the selected encounter, not the whole level/campaign. Other missing
AI actions, fire/gib deaths, required world services and safe NEW_LEVEL remain open.

## Subsequent user report — 17 October

The user confirms mouse control works after capture, but reports intermittent frozen
dead Cultists still blocking the train, including later enemies. This is an open
regression despite the deterministic cases above. Broader positions/impact regions
and death placement must be reproduced; no new fix is claimed by this note.
