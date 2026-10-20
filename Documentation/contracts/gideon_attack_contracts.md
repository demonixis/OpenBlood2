# Original Gideon attack, dodge and recoil sequencing

> Extended by [Gideon lifecycle contracts](gideon_runtime_contracts.md): production initialization, AI_Update, health delivery and fifth-table isolation are now integrated. Earlier blanket boss-registration omissions are historical. M2 remains incomplete.

Selected `AI_STATE_AttackClose`, `AI_STATE_AttackFar`, `MC_Dodge_Left/Right`
and the inline Recoil override now execute through the original virtual state
dispatcher. The shared original `MC_Taunt_Bold` is also connected. See the
[source ledger](../implementation/sources/m2_gideon_attack_sources.json),
[verification](../implementation/reports/m2_gideon_attacks.md),
[combat](gideon_combat_contracts.md), [teleport](gideon_teleport_contracts.md),
[locomotion](ai_locomotion_contracts.md) and [target](ai_target_contracts.md) contracts.

This is a boss dependency, not production registration or a completed encounter.
The diagnostic fixture explicitly supplies inventory, animation table, movement
cache and selected constructor values. Original boss initialization, health/damage
messages, shared-table ownership and save/load remain pending. No new mutable game
global or destination-transfer service is introduced. M2 remains incomplete.

## Dodge and taunt commands

A new left/right dodge stores server time, selects DODGE_LEFT/RIGHT, disables
looping, then writes cached right velocity multiplied by -1/+1 and roll speed.
It sets animating and command 21/22 only after the required services succeed.
Continuations rewrite that velocity and complete at **elapsed >= 1 second**,
regardless of animation duration or MS_PLAYDONE. Completion clears animating and
increments Metacmd once; it does not stop velocity. Changing direction/command
restarts the deadline. No RNG is consumed by the dodge method itself.

Original Gideon roll speed is 3.5; the fixture sets it explicitly. The pending
production constructor must supply it. Existing Move preserves trapped-zero and
unnormalized velocity semantics. Commands never integrate positions or refresh
the movement cache: the ordinary physics/CMovement owners do that once. Rendering
and inspection cannot advance the timer. The replacement physics catch-up bound
of 0.5 seconds remains unchanged; long tests use multiple valid ticks.

Bold taunt is TAUNT[4], command18. Like the existing Beg (TAUNT[6], command17), it
sets nonlooping playback and writes cached right*rollSpeed only on entry, then
waits for MS_PLAYDONE. Neither continuation nor completion rewrites/stops velocity.
Both source methods share a private implementation without changing Beg's behavior.
The existing finite speed bounds, checked clip/atomic foot resize and completion
overflow guards apply. Missing required services fail instead of starting an
animation successfully as the unchecked source would.

## Attack phases

Both states immediately choose SearchVisual when the target is absent. A pending
identity fails until the normal deferred link callback clears it. Distances use
CMovement's cached position and the previously sampled target position, not a new
body/target query. Raw distance overwrites SIGHT, as in source. Close calculates it
only in step2; Far calculates it on every call with a target, including no-op steps.
A never-sampled/nonfinite target or unavailable movement cache fails explicitly.

Close: step1 faces; step2 fires at distance <=90% seeing range or while command7
is already selected. Otherwise it faces, cancels that command increment and selects
Special1 beyond 40% seeing range, or Walk in the source's retained alternative.
Step3 continues an existing dodge direction; only a different command consumes
one 0..99 draw (<50 right, otherwise left). Step4 calls the real ComputeState.
The source's double .40 literal in Close is retained. No extra shot or timer fires.

Far: step1 faces, cancels the increment, then fires at <=30% seeing range or while
command7 is selected; otherwise Special1. Step2 runs Bold taunt. Step3 faces,
cancels the increment, selects Special1 at <=40%, otherwise Walk. Step4 computes
a new state. Existing Walk support/obstruction/animation behavior is unchanged.
A failed facing command never executes the cancellation or subsequent action.
Other switch steps retain the source's no-op behavior.

Recoil is the original immediate transition to Special1; it does not play the base
hit animation or wait for a recoil clip. The base method is now virtual so dispatch
reaches this override while Cultist/Civilian behavior stays unchanged. This does
not implement Gideon's distinct damage-message override or boss-health delivery.

## Lifetime, inspection and validation boundary

New methods require a live owner. A teleport contact cannot reenter attack/dodge
and mutate the moving actor. Model selection and velocity writes invoke no user
callbacks; model keys remain in the ordinary deferred animation batch. Existing
fire callback/removal tests remain applicable. Failed animation selection may retain
the source's earlier timestamp; no rollback of prior source effects is claimed.
Invalid clocks/speeds, missing clips/cache, pending identities and counter overflow
are observable failures. No stale actor can complete a command after removal.

Skeleton adds attack update count, last raw distance, historical dodge draw,
dodge calls/completions/elapsed time and lateral speed. Existing state, command,
velocity, animation and failure inspection remain authoritative. The draw field is
historical: continuing an existing dodge intentionally does not draw again. The UI
continues to label these methods as diagnostic, with boss initialization unavailable.

Synthetic tests cover exact timing, early animation completion, direction changes,
cached basis/position, trapped motion, one physics integration, retained velocity,
all range boundaries, in-progress fire outside range, no-target handling, recoil
virtual dispatch, required failures and deferred ownership. Three private cycles
run actual external Gideon shield keys, dodge and taunt clips through these states,
then unload. The extended SDL_GPU fade scenario also checks dodge displacement,
changed pixels, repeated render stability, one-second completion and removal.

SubwayA remains 203 created / 4 transferred sounds / 372 editor-only /
1 unsupported Gideon / 0 creation failures. Required boss/FX services and safe
NEW_LEVEL keep the actual M2 gate open. Linux checks do not establish retail
physics/randomness parity, a full boss fight, other-platform or XR support.
