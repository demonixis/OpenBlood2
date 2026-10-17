# Original jump voices and landing sounds

This connects the original client MoveMgr VME_JUMP caller and server PlayerObj
HandleMovement/GetLandingSound block. [Evidence](../../../Documentation/Implementation/m2_movement_audio.md)
and the [source ledger](../implementation/sources/m2_movement_audio_sources.json)
record the exact subset. The existing [movement](movement_contracts.md),
[character audio](character_audio_contracts.md), [VoiceMgr](player_voice_contracts.md)
and [player state](player_state_contracts.md) contracts remain applicable. M2 is incomplete.

## Jump caller

Only the accepted grounded, non-steep jump branch invokes client VME_JUMP (11),
after the impulse and before setting the held-jump latch. Both standing and forced
crouch jumps use the same event. Midair presses, held repeats, ladder ascent and
swimming jumps do not enter this branch. The original shallow-water rule matters:
when grounded with the head outside liquid, UpdateInLiquid returns before marking
the body liquid, so that jump still follows the ordinary ground branch.

MoveMgr receives the current client VoiceMgr by reference for this call; it retains
no pointer, adds no server message, and creates no second player. The actual
VoiceMgr busy, external-camera and strict deadline gates still decide playback.
Jump uses buffered audio, PLAYER_MEDIUM priority 5, and the original selected
character group. A refused/missing voice never cancels movement. The Movement
panel separates requests from successful starts; full refusal/failure reasons and
retained voice state remain in the existing VoiceMgr inspector. Exit clears
MoveMgr counters and releases the client voice.

## Landing caller

The server player samples current checked support and body height once per original
object update. This uses the existing shared-body collision query in place of the
old client on-ground bit; it does not integrate motion. It runs after stats, before
smell/death handling, independently of drawing and the inspector. Effective death
and blocked server movement preserve the old fall history without updating it.

Preserve the original order: a non-free transition from falling to grounded records
maxY-minY, requests a landing only when that span is **strictly greater than 56**
and Incorporeal is inactive, then clears the fall-start sentinel. Free movement
(head liquid or a non-hidden first ladder) clears the sentinel too. A hidden
first ladder does not allow a later overlapping ladder to replace it.
Afterwards falling becomes !grounded; if airborne, initialize min/max at the current
height when the start is negative, otherwise extend the extrema. Thus free airborne
movement repeatedly resets the sampled span. No new cooldown, velocity threshold,
landing damage or client landing sound is invented. Frame sampling can yield
different measured spans at different simulation rates, as the source does.

GetLandingSound queries the foot point (centerY - halfHeight + 1) through the
existing raw first-container query. Liquid overrides the retained baked support
material even for a hidden container, matching footsteps. Nine source materials
map to wood/flesh/glass/metal/plastic/stone/terrain/water/carpet and **landm.wav**
for the admitted Caleb profile. Unknown materials fail without guessing or
interrupting an existing sound. Landing does not toggle left/right foot state.

The real PlayPlayerSound request uses outer radius 200, inner 20, volume .7,
PLAYER_LOW priority 4 and buffered attached audio. Its retained character voice
is interrupted before a replacement attempt, even if that WAV is missing.
Original player hearing alerts still run after a failed playback attempt. A
cosmetic failure does not fail the session or replay on subsequent grounded ticks.
Query/association failures remain explicit errors. Successful movement alone is
not evidence that audio played.

## State and inspection

The supported PlayerState now includes the original falling flag, relative
fall-start time and minimum/maximum Y. Load rebases the clock and retains the source negative-start initialization rule; nonfinite or
out-of-coordinate-range extrema, inverted ranges and nonfinite rebased times fail
before mutation. Fresh values are initialized instead of preserving historical
uninitialized storage. Live checkpoint and KeepAlive use this existing value
boundary; it is not a whole-world save service. The separate server-falling flag
and contact-force damage path remain unported.

Movement inspection exposes actual jump attempts/starts, landing contacts/requests/
starts, last span, tracker values, selected material/container/resource and errors.
Read-only inspection never advances these states. Player-owned sound cleanup and
client VoiceMgr cleanup remain independent and run during exit/unload.

## Validation boundary

Generated fixtures exercise exact 56/56.01 boundaries, nine material mappings,
raw hidden liquid precedence, free movement, low ceiling, dead/blocked/Incorporeal
refusal, held/midair commands, camera refusal and missing WAVs. Field tests cover
roundtrip/rebased clocks and rejected corrupt histories. Private external Caleb
assets execute one jump and landing at 30/60/120 Hz on an explicitly diagnostic
stone floor, asserting decoded audio, caller counts, alerts and cleanup. This is
not retail collision/trajectory parity or an authored traversal proof. GPU checks
are separately recorded regressions; human audible playback is not claimed.

Fall/contact damage still requires a real impact-force service, and interactive
IMA music, whole-world saves and other required M2 services remain incomplete.

The [stopping-contact and falling-damage continuation](fall_damage_contracts.md)
now supplies accepted ordinary-movement impulses, original player falling damage,
engine mass/friction and saved server-falling state. Earlier blanket omissions are
superseded only for this boundary; dedicated mover/body/projectile forces, retail
physics parity and the M2 gate remain open.
