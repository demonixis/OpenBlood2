# Cathedral, Sewage Treatment Plant and Slaughter House

This separately requested continuation fixes the reported world-entry failures for
07_cathedral, 10_sewagetreatmentplant and 11_slaughter_house. M2's closure and
M2-GPU-001 reservation are unchanged. This is not full campaign traversal or retail
parity. [Source hashes](../implementation/sources/cathedral_sewage_slaughter_sources.json)
identify the selective original imports; reference trees and commercial data remain external.

## Actors and combat

Thief uses the existing BoneLeech jump/attachment machinery with its original
profile, Thief world/view models, 7–15 damage per second, three bite sounds and
view offset (0,-0.4,2). It does not apply BoneLeech's progressively darkened view.
The typed client handshake, checked owner links and deferred cleanup remain shared.
Thief and Naga have no head node in the mounted models: body damage explicitly
replaces the donor's uninitialized hit-sphere dependency, as for BoneLeech.

Naga retains its hidden entrance, 64-update fades, boss-health messages, held
spike attached to l_gun, spike model keys and original close/far attack choices.
The eye beam uses the existing beam weapon. Spike and ceiling-debris attacks are
now implemented. Debris spawns 3–5 scaled stone gibs near the target's ceiling,
with original per-update rotation, contact damage and reversed lifetime subtraction.
No ordinary ten-second expiry is invented for those rocks. Model children and
attachments are owned and removed through checked handles.

Zealot retains Divine health/damage multipliers, floating, close GroundFire/
Shockwave selection, far EnergyBlast, healing on animation completion, teleport
fade/ray/backoff and death explosion. Shield only requests its source particle
presentation; it grants no invented armor. Energy impact selects the divine recipe
after Setup; initial projectile presentation retains the source creation-before-
Setup ordering. Teleport uses one bounded solid-body movement attempt before
reappearance, rather than restoring solidity inside an unchecked object overlap.
The shockwave endpoint is initialized (undefined in the donor); the source's
commented-out SetVelocity stays absent. Its calculated impulse is damage direction.

## Weapons, pickups and projectiles

Full original definition records are imported for Howitzer, Napalm Cannon,
DeathRay, Time Bomb and the new NPC weapons. Existing shared ammunition, spread,
weapon animation keys, collision, damage and sound services execute them. Howitzer,
Napalm, DeathRay and TimeBomb pickups now resolve through authored SpawnObject too.
Time bombs use reserved inventory slot/ammo 12; T selects collected charges using
an edge-triggered command, with focus/text-input suppression. Primary charges and
throws, secondary drops; overholding explodes in hand. After sticking, a strictly
more-than-five-second fuse marks explosion for the following update. There are no
proximity scans or arm/detection sounds. Snapshot validation admits slot 12 without
claiming a new save-game frontend or projectile save support.

Howitzer alternate preserves the repeatedly redrawn 5–10 fragment-loop bound,
quarter parent damage and half radius. Its original length-30 direction multiplied
by speed 30 becomes equivalent normalized flight at speed 900. Napalm alternate
creates eight fireballs at effective speed 1225, one-third damage and the parent's
radius (the donor's smaller local radius is unused). The radial fan uses incoming
velocity as its up reference. Fireballs retain three-contact termination and the
unused randomized lifetime. Their bounded gravity/contact integration is replacement
engine code, not verified retail physics.

DeathRay primary traces/reflects after each 0.1-second interval, emits green beams,
and stops on characters/sky or its accumulated-distance limit. A 64-reflection
work guard bounds degenerate surfaces. Alternate FOV damage deliberately occurs
before the next shot's damage draw, using previous-shot damage (initially zero),
then follows the source shared vector path. Ordered destructible snapshots and
checked message delivery protect callbacks that remove objects.

Unimplemented emitter/light/particle recipes remain explicit effect diagnostics,
including Zealot staff/ground trails and DeathRay rotating rings. Combat success
does not claim those effects rendered. Source Naga screen shake is also unavailable.
The [animation contracts](ai_animation_clearance_contracts.md#invalid-requested-animation-ordinals)
now preserve the original invalid-ordinal no-op for Slaughter House's CivilianAI0.
Its missing request remains inspectable; no clip-zero mapping is used. Full
scripted traversal is unverified.

## Loading and shared engine rules

Initial NPC ground-ray misses are distinct from collision/service errors. Like
the original ignored ground-drop return, a miss retains an already clear authored
body for gravity/REMOVEIFOUTSIDE. Only a successful full-body clearance check admits
that case. Initial overlap escape extends the existing bounded solver to solid
non-world models: six sorted neighboring positions, including vertical escape,
with full-scene clearance and a checked route. Character-to-character placement
retains its horizontal candidates. This releases the Zealot authored inside the
Bigtruck in Slaughter House without changing the truck or the world mesh.

Sky registration now replaces an occupied slot, following later LithTech's
si_AddObjectToSky. The replaced object retains its INSKY membership until removal;
re-adding it does not move it to another slot. Generation identities, validation,
preflight copies and deferred lifetimes remain checked.

Cathedral's lightning SPR contains different-sized DTX frames. Each frame keeps
its native size; geometry retains the first frame's UV period as in the later
renderer's texture-swapping design. Missing/malformed frames still fail. The empty
built-in Sprite filename and absent models/worldobjects/shiklung1.abc are explicit
MissingResource omissions, with no path aliases or invented replacements. Sewage
retains one missing Container BSP omission. Slaughter House has no omitted row.
Unknown classes, unrelated resource failures and required services still block entry.

Track1's 14 explicit DLS velocity connections (source 2, control 0, destination 1,
transform 1, scale -960*65536) declare the default squared-velocity gain already
implemented by the sampler. They replace that implicit default; they do not apply
it a second time. Other transforms/scales/controllers still fail, as do duplicate
edges. Track1 also authors a 6387-timecent release (~40 seconds); the finite release
limit becomes 60 seconds. Other synthesis bounds and sample-clock ownership stay
unchanged. This is not a whole-synth listening equivalence claim.

## Verification boundary

The campaign tests use the inspector's start_new_game transaction, including
replacement of a running session, in all three difficulties. They verify zero
unknown/failed entries, checked presentation, tiny first-frame intervals, three
seconds of movement/simulation, new actor runtime state and complete unload.
Focused private-data tests exercise actor attacks/healing/teleport/attachment,
pickups, weapon firing and the time-bomb fuse. Synthetic tests cover sky replacement,
unequal SPR dimensions, explicit/default velocity PCM equality and release limits.
Keep headless, sanitizer and real-display results separate when reporting a run.

[Save/load lot 12](save_load_contracts.md#implemented-bosses-and-special-enemies-lot-12) saves Naga (the DWord-written,
Float-read whomp count; the spike its restored initial update creates is kept, a recorded
departure), Zealot (float, staff-start and divine flags; the unsaved staff start is cleared by the
next fire update) and Thief through BoneLeech's block.
