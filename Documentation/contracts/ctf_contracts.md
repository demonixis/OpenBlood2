# Original CTF objects and local inspection

This continuation supplies the two original FlagStand entries required by each
of Cabana, Dead Cities, DMX and Jam, their independent FlagObject children and
player capture rules. It extends explicit multiplayer-map inspection; it does
not implement network sessions or accept M3. Source provenance is recorded in
[the ledger](../implementation/sources/all_levels_ctf_sources.json), with checks
in [the all-level report](../implementation/reports/all_level_loading.md).

## Objects and original state transitions

FlagStand preserves original PRECREATE base-first order; other messages call the
base after its own handler. FlagObject preserves its base-last order. TeamID and
ObjectName are the original properties. Both objects are visible/touchable,
without gravity or solidity. The source skin branch assigns team 2 for non-blue
values *after* choosing the model; an invalid team with no model fails explicitly.
No substitute model or blue/red alias is supplied.

Real flagbase/flagblue/flagred ABC and blue/red DTX assets stay external. Dimensions
come from the current original animation. These nonsolid objects keep their
center when sized, following the nonphysical branch of later LithTech
ChangeObjectDimensions; this is supporting engine evidence, not a 1998 executable
trace. Character ResizeObject intentionally preserves feet and is inappropriate
for this branch. The stand then uses checked MoveObjectToGround and spawns one
flag at world offset (-16, +32, 0), retaining its rotation. Destroying the stand
does not destroy its independently world-owned flag.

Each flag stores its initial stand pose. The last initialized flag of each team
wins the original static registry; destroying that flag clears the slot without
falling back to an older object. The registry, scores, scoring options and
four-second server notification clock belong to the world, participate in staged
world-context exchange, and reset on unload. There are no dangling cross-world
original pointers.

A flag touches players only when not marked carried, delivering an owned
`flaggrab` trigger. A stand delivers `flaggive`. The player trigger dispatcher now
preserves sender identity for these commands and supports `flagdrop` without a
sender. Command token/case rules remain the existing original rules.

GiveToPlayer marks carried and clears ground/stand. DropToGround grounds the
flag, marks it dropped and schedules a one-shot update after 30 seconds. That
update returns it only if it is still on the ground; picking it up does not reset
or cancel the old deadline. RETURN is case-insensitive and has no effect while
already in the stand. Return restores the saved pose and plays the actual
CtfFlagRespawn WAV, radius 2000, miscellaneous-medium priority, without reverb.
An explicit RETURN while still attached retains the attachment's resolved pose,
matching the next observable original attachment transform; its stand/carried
flags and stand-position sound still change as in the source. No independent
teleport of an attached child is introduced.

## Player, attachment and score

Grab refuses dead/spectator players and the same already-carried object. Touching
one's own flag returns it if it is away; touching the enemy flag attaches it to
the player origin at (0, 75, 0), identity rotation. The original replacement
branch removes an old attachment without explicitly dropping that older flag.
The supported one-flag-per-team map flow does not fabricate additional flags.

Attachments now admit touch-only nonsolid children. Attached records remain
excluded from physics/contact traversal; solid/gravity/ray-hit children,
velocities, chains, cycles and independent movement still reject. Their touch
flag is preserved for detachment. Checked inter-object links clear a removed
flag from its player. Unexpected carrier removal returns the surviving flag;
normal client departure drops it first. These are explicit safe-lifetime
adaptations of the source's unchecked handles, not child ownership by the stand.

Capture requires a carried flag, a stand of the player's team and the registered
other flag in its own stand. A missing friendly flag emits NEEDYOURFLAG only
when `last + 4 < now`; the client independently prints only when `last + 20 < now`.
Capture adds the original default five points. The source UseTeamSize option
uses at least one opposing connected player; inspected local player/team scores
are actual counters. Bounded arithmetic rejects overflow before scoring. Actual
CtfFlagCapture audio follows the player with miscellaneous-medium priority and
requested reverb; the existing mixer supplies dry output. Capture broadcasts
typed events to locally connected player objects, detaches and returns the flag.

Death drops the flag before the original null lethal-attacker guard. Client
exit drops without notification. Saved player-field snapshots explicitly refuse
CTF team/carried state because match/flag restoration is not implemented; handles
are never serialized into an unrelated world.

Explicit CTF inspection uses an authored marker's team, or blue for a neutral
marker, as a deterministic local entry policy. This is not a network team-manager
implementation. The inspector can switch between blue/red (dropping an existing
flag) and drop the current flag; it exposes team/player scores and each flag's
stand/ground/carried state and counters. Ordinary campaign entry remains unchanged.

## Actual presentation and visible limits

GRABBED and RETURNED decode/play the original local CtfFlagGrab WAV at miscellaneous-medium priority without reverb. The client
resolves original resource strings 7000, 7001, 7005 and 7009, retains explicit
resource/audio errors and exposes notices in inspection. The original console
text overlay and remote-player name/scoreboard presentation remain unavailable.
DROPPED and CAPTURED clear the carried-flag icon; the capturing local player's
score changes on the actual typed capture event.

Selective original SetTeamID/DisplayFlag/DrawTeamIcon/DrawFlagIcon rules load
teamblue/teamred and flagred/flagblue PCX from interface_multipatch. Blue carries
the red icon and red carries blue. Native-size team artwork sits five pixels
from the top/right; the carried icon sits below its height plus ten pixels.
Exact magenta remains transparent. The HUD atlas grows to 1024x128 to hold these
external images without changing shaders or world-renderer behavior. Both
inspector and inspector-free applications execute this path. Missing art fails
rendering, and unchanged state does not upload again.

Opening, three-second simulation and focused capture tests do not certify full
multiplayer matches, campaign traversal, all original screens, platform parity
or the existing M2 GPU sanitizer reservation.
