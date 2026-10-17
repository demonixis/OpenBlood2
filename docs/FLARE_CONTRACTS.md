# Original Flare Gun and projectile family

Read [weapon](WEAPON_CONTRACTS.md), [projectile](ENERGY_PROJECTILE_CONTRACTS.md),
[audio](WEAPON_AUDIO_CONTRACTS.md), [NPC firing](AI_FIRE_CONTRACTS.md),
[drop](WEAPON_DROP_CONTRACTS.md) and [object lifetime](OBJECT_CONTRACTS.md) contracts.
The [source ledger](M2_FLARE_SOURCES.json) records original selections;
[implementation evidence](../../Documentation/Implementation/M2_FLARES.md) records validation.
M2 remains incomplete.

## Weapon, acquisition and firing

ID 3 retains the complete original WeaponData: pistol animation column, flare
ammo pool 4, primary consumption 1/damage 10–15, alternate consumption 8/damage
20–30, speed 2000, range 5000, radius 100 and reload fields .75/1.5 seconds.
Both definitions emit one projectile. The later alternate projectile supplies
its eight fragments; the weapon does not shoot eight full-damage flares.
Insufficient ammunition refuses the entire shot. Existing marker-driven view
animation, both hands, sound/hearing, checked inventory restore and active-Fire
lifetime rules apply. Source FLARE prefix selection uses the existing original
NPC aiming and standing-fire path. The full stored definition is immutable.

The original 60-unit player-only FiringTooClose query uses the typed body-eye
origin, since this port has client-owned view models rather than the original
server view-model proxy. A close hit selects that eye before alignment. This is
an explicit interface adaptation; ordinary local shots already originate there.
The existing collision preflight prevents ammo consumption on unavailable
queries; eligible hearing callbacks revalidate before creating a projectile.
Portable owned randomness does not claim retail CRT bit parity.

FlareGunPU keeps original FlarePistol_pu resources, display name, resource 355
and eight rounds. The typed weapon pickup boundary now accepts IDs 1–3. Full dual
inventories/full ammo refuse duplicates. NPC hand drops use the existing detached
lifetime and support-to-pickup transfer. Player-initiated dropping remains absent.
No new mutable global or dependency is added.

## Primary projectile

Original CFlareProjectile selects one of red, orange, yellow and white, begins
with scale zero, then uses .25 after elapsed .01. Its sprite uses the actual
external lensflare32 SPR/DTX. Sprite Set/GetObjectColor now accepts bounded RGB
and alpha; sprites initialize to multiplicative white while models retain their
separate additive-zero convention. Presentation consumes this state read-only.

The flight phase remains the bounded continuous point solver, integrated once.
Contact ignores the shooter, nonsolid objects and the exact projectile class.
A character/player/corpse contact retains a checked target link and world-space
position offset, resets its own start clock and lifetime to five seconds, and
clears TOUCH_NOTIFY. It follows translation without rotating the offset. No
second flight segment is integrated between sticking and the next Update.
A solid target with TOUCH_NOTIFY receives one deferred original contact callback.
Unlike base projectile contact, the primary override has no sky-discard branch.
Other contacts schedule the original fire-type radius explosion on the next update.

Tracking sets velocity zero. Damage uses the source strict condition
`time > last_damage_time + 1`, with last_damage_time initially zero rather than
the contact clock. Each tick sends FIRE damage with zero direction, projectile
sender and checked shooter attribution. Target death sets the explosion flag but
still completes that update; the next update fizzles without additional damage.
Target unlink removes without fizzle. Lifetime expiry is strict greater-than and
also emits no explosion. Losing the shooter preserves null-attributed primary
damage through the existing typed projectile authorization.

The attached looping projectile WAV starts/retries when no voice exists. Touch
kills it; the next tracking Update restarts it, following the source quirk.
Expiry, fizzle, removal and unload release it. Impact WAVs use detached lifetime.
Callback removal remains deferred; no endpoint is accessed after tick delivery.

## Alternate and fragments

CFlareAltProjectile caches velocity on its first Update, scales to .5 after .01,
and bursts only when elapsed is strictly greater than .1. Eight independent
CFlareBurstProjectile children retain the source order/offsets, normalized
velocity directions, half damage, full original speed/radius and parent color.
Fragments have source god mode, scale .25 and the base ten-second lifetime.

The alternate calls AddImpact while its type is still EXP_FLARE_FRAG (15), then
sets EXP_FLARE_BURST (14), calls base Update and returns false. Thus a pending
contact/death at the burst deadline can emit FRAG followed by BURST; a normal
in-flight burst emits only FRAG and no parent damage. Early contact explodes as
FRAG without children. AddImpact retains its original stored-direction magnitude
20 and optional surface-normal query; Explode remains the damage owner.

Checked Setup requires a live shooter. If the alternate loses it before splitting,
it fails explicitly rather than inventing a shooter. Child creation/setup failure
retains already-created independent children and records failure/count. This
bounded adaptation differs from the original unchecked allocation/null paths.
Runtime classes reject authored/string creation and do not claim save support.

## Inspection and remaining gaps

Objects exposes tracking/start/tick/voice state, contact identities, cached
alternate velocity and child count. Terminal history retains burn deliveries,
tracking and fragment counts after removal. Scene explosions records requested
component types and actual WAV playback; absent components are failures, not FX.
The diagnostic weapon grant is applied at the existing frame boundary.

Original sparks trails (OBJFX_SPARKS_1/3), owner lights of radius 200/75 and flare
explosion recipes 13–15 remain unavailable. The alternate source trail is disabled.
No generic effect substitutes for those recipes. Chrome, muzzle illumination,
view-key sounds, fire/gib death presentation and complete combat parity remain
unimplemented. Replacement flight and body-eye adaptation are not verified retail
physics/presentation parity. Safe NEW_LEVEL and remaining required world services
still keep M2 open.
