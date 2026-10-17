# Original single-player death

Selected original CPlayerObj HandleDeath/DoDeath, corpse/gib creation and the
CPlayerCamera death orbit are connected for the existing Caleb session. See
[implementation evidence](../../../Documentation/Implementation/m2_player_death.md)
and [source selections](../implementation/sources/m2_player_death_sources.json).
M2 remains incomplete.

## Server ordering and lifetime

Player Update checks effective damage death after stats and smell, before the
aggregate clears accumulated frame damage. A null lethal attacker preserves the
original HandleDeath early return. The accepted lethal packet supplies the
WhoKilledMeLast identity; it is distinct from retaliation memory. An attacker
removed before Update remains comparable without dereferencing an expired object.

For the supported single-player profile, death commits once: remove supported
inventory items, request Suicide (8, self killer) or Death (4), send owned
PlayerDead/SMSG_DEAD, stop the player's retained character sound, hide the player
model and set the original PMOVE_DEAD value **16**. The persistent player retains
its client and solid body. View and held weapons hide; the original dead-update
tail selects slot 1, falling back to slot 0 when unavailable. Existing fire,
movement, item and weapon-selection death guards remain. **USE still casts its
original single-player ray**: OnCommandOn explicitly exempts it from the dead
command guard. There is no new revival binding.

DoDeath removes one inventory item per type 1..9. Medkit is discarded, an active
flashlight sends its original off action before its owned light is removed, and
only the first key is removed. Original inventory bomb selector items are distinct
from weapons: reserved weapons, ammunition and thrown charges remain. This port's
slot-backed selectors are disabled by the dead command gates. Loading an existing
live checkpoint reconstructs them; there is no unsupported dead-to-live shortcut.

The supported profile has no active timed power, multiplayer slow death,
humiliation, carried flag, zoom or parasitic attachment. Active-power, multiplayer
and non-Caleb death attempts refuse before committing and expose an error. This
is not a successful ResetPowerups, multiplayer death or full player-save service.
Full restoration of a dead player and its independent corpse remains rejected.
Field-only snapshots can represent movement 16; unported humiliation 15 still fails.

## Corpse and fragments

Preserve the source bitwise test against EXPLODE (5), followed by the literal
accumulated-damage-fraction threshold **50.0**. Do not substitute the NPC equality
and .90 rules. Thus Fire and Melee also satisfy this original bit test. FreeFall
skips a corpse unless the preceding gib condition succeeds.

Fresh Caleb's separate player node/side fields are initialized to zero and are
not written by his damage handler: ordinary death selects death animation slot 1.
The damage aggregate node is used separately for the original optional limb hide
and creation. Corpse creation copies current model/skin/rotation, preserves raw
animation dimensions, unit player scale, the original foot offset plus .001, and
hidden-limb masks. The player's original corpse constructor defaults mean limb
loss enabled, no burn source and an **empty sound directory**; no NPC sound root
is invented. Playback/physics use the existing independent CCorpse lifetime.

Initial placement uses the existing bounded nonsolid-body solver. This remains
replacement physics, not verified retail placement. Failed setup removes the
partial child. As in original DoDeath, failed presentation does not undo death or
repeat creation; the error remains inspectable. A corpse survives player removal.

Player gibs negate the last direction, subtract one from Y, normalize, multiply
by 50 + accumulated frame damage, then negate again through SetupClientGibFX.
The final vector is shared with limb creation. Scale 1 and integer mass>>5 are
preserved, as are the unused first RNG draw and strict >80/>70/>50/>70/>50 limb
thresholds. Existing typed fragment/limb services perform actual creation; failures
remain separate from successful requests. Zero normalization yields finite zero
velocity; invalid poses/directions/mass and existing resource budgets reject.

## Client death camera

The typed notification starts the original orbit once. Duplicate delivery cannot
restart it. Entry/exit reset its state; the target remains the checked player,
not a potentially removed corpse. Scripted CameraObj selection still has priority.
When ordinary view resumes, elapsed time is the session clock, independent of UI.

Preserved source values: .8 radians/second, 90*1.2 radius, target offset (0,30,0),
35 up, one-second linear transition from the existing first-person eye, and look
at that eye. Preserve the source double basis transformation: rotated forward
world X/Z components are multiplied by the target's right/forward axes again.
A ray from the target offset to the requested position moves an obstructed camera
to hit+normal. The existing checked solid-world/object ray explicitly excludes the
player; uninitialized historical query flags are not reproduced. At zero elapsed
time, the zero look vector retains the valid target rotation. Query failures are
reported, never accepted camera positions. Listener behavior is unchanged.

The Session inspector shows committed death, notification counts, voice selection,
corpse/gib/limb results, placement, actual orbit/transition/obstruction state and
errors. Queued voice requests may be refused by the existing VoiceMgr busy/camera/
cooldown gates; they are not counted as actual playback. Rendering reads the
resulting view and model states without advancing simulation.

## Verification scope

Generated fixtures exercise orbit/basis/transition/wall obstruction, duplicate
start, pending-target rejection, null-killer and active-power refusals, freefall,
dead field16 and command/respawn guards. Private external Caleb assets exercise
ordinary, fire, melee, freefall and instant deaths, real corpse animation, inventory
cleanup, retained weapons/ammo, removed-killer identity, independent corpse
lifetime, five live-checkpoint restorations and teardown. These are labelled
encounters on a diagnostic floor, not a retail playthrough.

The NVIDIA death-effects test additionally rasterizes the real player corpse from
the computed death camera, checks unchanged repeated readback and teardown.
See the implementation report for exact configurations/results and limitations.
