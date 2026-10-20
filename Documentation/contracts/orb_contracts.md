# Original Orb weapon and projectiles

This complete-level continuation supplies Ishmael's original weapon dependency.
It does not register an incomplete Ishmael class or relax world admission.
[Source hashes](../implementation/sources/all_levels_orb_sources.json) identify
original definitions, weapon/view callbacks, projectile code, effects and camera
branches. M2 closure and reservation M2-GPU-001 are unchanged.

## Weapon and ownership

Weapon 15 and OrbPU use the original definitions and commercial models/skins.
Focus cost is 20/200, damage 10–15, speed 500, reload 1/5 seconds and no radius
damage. The client rest animation loops as in COrb::UpdateFiringState. Both modes
use original FIRE_KEY delivery, typed inventory, pickup/drop/spawn and NPC aim
paths. No direct-damage replacement stands in for a flying projectile.

Both projectiles render orb.abc at scale .5, use point flight and the original
five-point damage aggregate. Their Update overrides have no ten-second base
expiry. Attached/flying/drilling loops use the original external WAV files and
object-bound lifetime; resource failure rejects the required service. Exploding
plays the detached original impact sound. Object removal and world unload release
voices, checked links and camera state through existing deferred ownership.

## Primary guidance and attachment

Every update chooses the nearest character head within 175 units, excluding the
shooter. Preserve the original signed component test: a target is ignored when
all three velocity components are less than .1, including fast negative motion.
Ties follow destructible-list order. Orientation interpolation is
`1 - distance/175`, independent of elapsed time; speed is preserved. An exactly
coincident head retains finite current orientation.

A solid character contact attaches immediately; there is no primary 15-unit
head-distance gate despite the unused source constant. The orb stops, follows
head position plus forward*7.5, and turns around the head's up axis by pi. It
requests damage strictly more than one second after the previous damage clock,
which starts at zero rather than attachment time. Original explicit blood-spurt
and flesh-spark flags produce real effects. Character armor/localized modifiers
still apply to the delivered damage. A missing head leaves the source attachment
without periodic damage, rather than synthesizing a node.

A missing/removed target, dead player target, or strictly greater than 5.1 seconds
attached sets Explode for the following update. Original base Explode still
performs its final direct-hit damage. Non-character contact requests explosion.
Neither contact mode invents damage on initial attachment.

## Alternate and camera

Alternate follows the firing player's pitch/yaw after its first checked player
update; a dead firing player requests explosion. Before that first update, keep
the current orientation instead of the source uninitialized rotation. Non-player
shots retain their current orientation. Attachment additionally requires an
actual head within 45 units. Any solid contact turns the local camera off, even
when attachment fails. Successful attachment also clears the player's Orb
listener link; explosion/removal clears remaining checked ownership.

The typed OrbView event is emitted immediately after successful weapon Setup,
replacing the original precreate client-ID routing without a raw user-data field.
Only the matching local shooter's live alternate projectile can enable a view.
An older off event cannot clear a newer camera. Removal, death and unload restore
normal camera selection. Seeing Eye retains priority over Orb, and Orb over
ordinary CameraObj selection. Orb view blocks player movement/new weapon fire,
uses current aim and projectile position, shows the player body, hides weapons
and the camera's own model, and requests default FOV +45 degrees. Input rotation
continues while movement is blocked. Client listener position follows the view.
The server's original Orb branch suppresses ordinary player pings unless an
active scripted camera takes precedence. Active Orb capture fails explicitly
because the existing partial player snapshot does not serialize projectiles.

## Original effects and bounds

Primary ObjectFX33 emits blue/white filled-sphere particles with radius 200,
position radius 3, count 3, density 7.5, delay .025, velocity -5..5 and lifetime
.25..35 seconds. Original streaming, stopped-owner deadline, previous-axis
rotation order and single shared RGB interpolation draw are retained.

Explosion21 creates ten actual gibmetal2/gibmetal3 fragments: scale .25,
rotation maxima 1, bounce .5, duration 7, gravity .125 per update, fade time 2,
spread 2..15 and speed 3..6. Each has ObjectFX34: radius 200, density 5, delay
.025, forward velocity 20..40, blue/white colors and lifetime .15..20. Original
fragment movement is per update, not multiplied by elapsed time. Detached
fragments survive the projectile; their trails follow only their fragment.
Missing resources and exhausted fragment/trail budgets remain explicit errors,
with already-created siblings retained for cleanup/inspection.

Existing bounded particle integrators, collision and deterministic RNG policies
remain replacements, not verified retail numerical/random parity. No shader,
renderer pipeline or dependency change is required. Inspector reports show
attachment, damage ticks, camera state, trail identity and component errors.
