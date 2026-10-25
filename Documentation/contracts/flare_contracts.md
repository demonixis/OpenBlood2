# Original Flare Gun and projectile family

Read [weapon](weapon_contracts.md), [projectile](energy_projectile_contracts.md),
[audio](weapon_audio_contracts.md), [NPC firing](ai_fire_contracts.md),
[drop](weapon_drop_contracts.md) and [object lifetime](object_contracts.md) contracts.
The [source ledger](../implementation/sources/m2_flare_sources.json) records original selections;
[implementation evidence](../implementation/reports/m2_flares.md) records validation.
The original report retains its dated scope. M2 is now closed with reservation M2-GPU-001;
this VFX continuation does not accept M3.

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
Runtime classes reject authored/string creation. Save/load lot 9 now covers this family;
see the [save contract](save_load_contracts.md#implemented-pickups-and-projectiles-lot-9).

## Inspection and remaining gaps

Objects exposes tracking/start/tick/voice state, contact identities, cached
alternate velocity and child count. Terminal history retains burn deliveries,
tracking and fragment counts after removal. Scene explosions records requested
component types and actual WAV playback; absent components are failures, not FX.
The diagnostic weapon grant is applied at the existing frame boundary.

Owner-light illumination (radii 200/75), chrome, muzzle illumination and complete
combat/raster parity remain outside this recipe lot. The alternate source trail is
intentionally disabled. Replacement flight and body-eye adaptation are not a claim
of retail physics/presentation parity.

## Original VFX completion — lot 5 (2026-10-25)

[Verification](../implementation/reports/flare_effects.md) and
[source hashes](../implementation/sources/flare_effect_sources.json).

- Primary publishes `OBJFX_SPARKS_1` (6); burst fragments publish `OBJFX_SPARKS_3` (8).
  The alternate parent never publishes `SPARKS_2`: its original call is commented out.
  Each high-detail burst emits four/two yellow particles in a filled sphere of radius 5,
  at speeds 25–50/50–75, system radii 350/400, gravity −250 and lifetime .5–.75 s.
  Emission runs every .01 s without interpolated stream samples. The cached right axis
  precedes the two random rotations, as in the source; one ratio controls all color channels.
- First update emits nothing. Stopping freezes the emission origin and sets duration to
  elapsed + .25 + .75 s, with alpha fading only in the final .25 s. Emission continues
  during that interval. Strict duration expiry, immediate owner removal, bounded particles
  and explicit missing-texture errors apply. Render assembly uses the fixed creation
  origin for every `MOVINGSOURCE` effect, avoiding a second application of owner travel
  (also corrects the existing fire/bug/orb/corpse trails).
- Fizzle (13): eight orange ring particles, radius 2500, velocity 50, duration .5–1 s,
  offset 15; smokepuff2 at the same offset, .25→1 scale, 3 s, alpha .85, waveform 1.
  Light states are the 1.25 s radius-100 flash and 25 s radius-50→50 scorch state.
- Burst (14): debrisimp at the endpoint, .25→.4 scale, .25 s; .75 s radius-150 flash.
- Fragment (15): explodesmall at offset 10, .25→.75 scale, .5 s, alpha .75; smokepuff2
  at offset 15 with the fizzle settings; 1 s radius-75 flash. The commented scorch
  light stays absent. All sprite fades are type 1; billboards remain unaligned.
- Publishers request only authored component families. No phantom model/flame/debris
  failures are recorded for these recipes. Actual component failures and owner-light
  omissions remain inspectable; cosmetic failures do not change projectile gameplay.
- Load republishes one owner-bound trail for primary/burst objects, without a payload
  change. Original unsaved tracking/clock behavior and the saved fragment color remain.
  Primary attachment damage, alternate eight-child order, FRAG-before-BURST quirk,
  audio and damage attribution are unchanged.

As for existing recipes, these are the original high-detail parameters. Portable RNG
and the replacement particle/sprite renderer do not certify pixel-identical retail
output. Evaluated light states still do not illuminate geometry (lot 10).
