# Original Ancient One and tentacles

This continuation integrates original AncientOne/AncientOneTentacle, their two
weapons, the red-large beam and the high-detail primary napalm impact recipe.
The required models, skins, animations, sprites and particle textures remain
external; missing required resources fail. It does not supply multiplayer,
complete campaign traversal, NPC save support or point-light illumination.

## Profiles, health and animation

AncientOne retains 22000 HP, 2000 armor, strength 5, mass 250, zero movement
speeds, hearing/sense 10000, sight 18000, smell zero, ALWAYSCLEAR and non-Cabal
membership. It disables damage impulses. Its fixed AncientOne model/skin,
independent animation/sound table and base scale five precede the original
per-axis random scale adjustment. The first scheduled update publishes full
health and captures current difficulty-adjusted HP. Damage applies once before
the health ratio and base reaction. A missing audience is distinct from failure.

Tentacles retain 200 HP, zero armor, sense 1000, sight 2000, hearing/smell zero,
zero movement speeds, base scale 2.5 and disabled impulses. Each instance draws
model ancientone_t1..t4 with the common AncientOne skin; the first instance owns
the source's shared animation indexes. The two classes have separate tables.
Their actual models lack the humanoid head node: the existing explicit torso
body-damage policy replaces the donor's uninitialized hit-sphere read. No
fabricated skeleton/sphere is supplied. Accepted body damage now reaches the
virtual subclass notification; existing other nonhumanoid reaction behavior is
unchanged. These two classes explicitly run their original base reaction.

AncientOne alternates idle/search without stimuli, otherwise attacks far. Every
attack phase faces the stored target; idle precedes fire. Fire chooses tentacle
strictly below 300 units, beam otherwise, with MELEE nonlooping animation and
actual model fire keys. Its original loop sound and initial random tentacle-spawn
loop are commented out in the source and remain inactive. The SpawnTentacle
method uses two signed 0..500 draws, the original world-coordinate 300-unit
origin exclusion and an independent checked child. Both classes ground spawned
objects as well as authored world objects.

Tentacles attack at distance <=500, scale up before facing/fire, and retain fire
continuation after losing a target. Idle uses looping idle[0]; the source's timed
hide branch is commented out. MC_Dead restores exactly 200 HP, records the clock
and chooses Special2. Retraction starts from 2.5, clears solid, enables gravity,
and changes scale/dimensions by .125 each update to .01. It then waits until
elapsed time >=10 and grows back by .125 to 2.5, lifting 200 units and grounding
each growth step. Completion restores solid/gravity and looping idle. Source
animating flags and command increments remain intact. Special4 retracts and
marks hiding; Special1 clears it. Fade helpers retain per-call 1/64 alpha changes.
Ordinary gib/removal rules remain those of the base AI.

Damage whose **message sender** is AncientOne is intentionally ignored by a
tentacle, without reacting or changing HP. The typed transport reports accepted
ignore rather than interpreting the donor's zero return as a delivery error.
Other senders, including projectiles, do not gain this immunity.

## Beam, tentacle weapon and impact

Weapon 46 retains 35..70 damage, 12000 range and one-second primary reload.
The head_gun aim replaces normalized target XZ with cached forward, then aligns
rotation; weapon 47 aims from the body at the stored target. Both omit the
ordinary CheckClearShot gate. Beam rays ignore AncientOneTentacle subclasses,
retain solid/world occlusion and skip the owner, equivalent to the source's
self-hit continuation. The first remaining hit receives ordinary damage and
EXP_NAPALM_PRIMARY with upward normal; a miss only creates the beam. Red-large
uses the original beam model/red skin, scale .1..2.5, one-second lifetime,
.35-second growth/fade and alpha .5. Impact audio after the source's returning
loop is unreachable and is not invented.

Weapon 47 retains primary 15..20 and alternate 20..40 damage, 800 range,
.01-second reload and Melee damage. Its inclusive 0..4 sound draw writes the
**opposite** fire mode's persistent slash sound, exactly as in the source.

Primary napalm impact now creates three original Exp_sphere_2 models (scales
20→400, 35→350, 50→450; duration .75/.75/1; alpha 1/.75/.5; waveform 0/2/1),
napalm and smoke sprites, and a 32-particle Firepart64 ring with gravity 350.
Two light states retain flash/scorch colors and the duplicate Radius1 assignment
(leaving Radius2 zero). There are no fragment or flame-system branches. Multiple
models share an event with all instance IDs inspected; all model components
prepare before committing, so a missing component cannot leave a partial model
batch. Existing single-model callers retain their first-instance report field.
The evaluated lights remain distinct from unavailable world illumination.

## Later platform placement

AncientOne2 is authored at Y=-192. Before DeathTrap exists, initial grounding
lowers it to about -659.8, where its scaled body overlaps the later slab centered
at -544. The completed-scene placement can return vertically using the authored
spawn as a bounded anchor: XZ must still match, the body must have descended,
and the candidate may rise at most one body half-height above the authored Y.
The existing total distance limit, endpoint clearance and swept route still apply.
This return is permitted only outside object callbacks before the first AI update;
tentacle scale/grounding during runtime cannot use the authored-anchor exception.
This extends initial placement only; runtime collision is unchanged. It is a
replacement box-physics adaptation, not a claim of verified retail physics.
