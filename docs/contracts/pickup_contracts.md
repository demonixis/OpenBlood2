# Original pickups and destruction spawns

Continuation: [original collection audio](pickup_audio_contracts.md) supersedes the
collection-sound omission for supported pickups and keys. Trigger activation
sounds, character quips and other missing services remain separate.

> `SniperRiflePU` is registered as an original offer whose weapon is not integrated:
> the inventory refuses it and it stays in the world ([contracts](fanatic_prophet_contracts.md)).

> Extended by [NPC weapon-drop contracts](weapon_drop_contracts.md): the selected
> Cabal drop, independent falling hand and Beretta collection now work. Corpse/gib
> and remaining death services still fail explicitly; M2 is incomplete.

This is a single-player subset of M2, not complete inventory or gameplay.
[Source selections](../implementation/sources/m2_pickup_sources.json) record the original functions and
adaptations. [Implementation evidence](../../../Documentation/Implementation/m2_pickups.md)
records actual content coverage and verification. The preceding
[pushable contracts](pushable_contracts.md) still govern brush physics.

## Supported original rules

PickupObject derives from B2BaseClass. AmmoPickup retains its intrusive static
list and the eight active original ammo classes. PowerupPickup, HealthBase and
ArmorBase preserve the ancestry of the four selected health/armor classes.
Abstract/base pickups cannot create a usable object without a concrete model.
KeyPickup keeps its earlier, separate selected implementation.

| Class | Type | Amount | Model basename | Resource ID |
| --- | ---: | ---: | --- | ---: |
| BulletAmmoPU | ammo 1 | 100 | BulletAmmo_pu | 317 |
| ShellAmmoPU | ammo 2 | 20 | ShellAmmo_pu | 318 |
| BMGAmmoPU | ammo 3 | 20 | BMGAmmo_pu | 319 |
| FlareAmmoPU | ammo 4 | 20 | FlareAmmo_pu | 320 |
| DieBugDieAmmoPU | ammo 5 | 20 | DieBugDieAmmo_pu | 321 |
| HowitzerAmmoPU | ammo 6 | 5 | HowitzerAmmo_pu | 322 |
| FuelAmmoPU | ammo 7 | 10 | FuelAmmo_pu | 323 |
| BatteryAmmoPU | ammo 8 | 40 | BatteryAmmo_pu | 324 |
| HealthPU | powerup 1 | 25 | Health_pu | 309 |
| MegaHealthPU | powerup 2 | 100 | MegaHealth_pu | 310 |
| WardPU | powerup 3 | 25 | Ward_pu | 311 |
| NecroWardPU | powerup 4 | 100 | NecroWard_pu | 312 |

Original constructors supply model/skin basenames and sound names. The current
ABC animation supplies dimensions, with X at least 10 and Z at least X.
World-file creation calls MoveObjectToGround; string creation follows the
non-bouncing SpawnItem path and keeps its requested position. Visible, touch,
gravity, no-shadow, saveable and night-goggle-glow metadata follow source rules.
PowerupPickup also retains its constructor rotation request and bit 4. These
flags do not implement saves or visual effects. Missing ABCs remain
inspectable with touch/gravity disabled and an explicit diagnostic.

Touches send MID_ADDAMMO (1009) or MID_ADDPOWERUP (3504). The source accepts
contacts from any object. The connected CPlayerObj branch changes player stats;
[original NPC ammo recovery](ai_ammo_contracts.md) additionally accepts ammo
through a checked active offer and matching acknowledgement. Alive players accept ammo below their attribute-derived cap, truncating
partial pickups to that cap. Health/MegaHealth and Ward/NecroWard use their
separate original normal/bonus caps. A full reserve keeps the pickup available;
zero amounts retain the source acceptance behavior when below the cap. Dead
players cannot collect or revive themselves through this path.

## Typed messages and lifetime

PickupMessage owns kind, numeric type and amount. The adapter validates routing,
matching message ID, live sender, type range and finite amount in [0, 1000000].
It rejects mixed byte/damage/key/pickup payloads, multiple writes and stale or
non-borrowed reads. Transport is local and typed, not the original packet ABI.
The source MID_PICKEDUP (1005) float -1 means default respawn; this selected
single-player path represents it with a matching typed acknowledgement.
Multiplayer respawn and custom respawn overrides are not implemented.

The player acknowledges only successful additions, using its actual BaseClass
sender rather than casting the inventory aggregate to BaseClass. A pickup accepts
that acknowledgement only during its own outstanding touch, from that live
player, for exactly its current kind/type/amount. It marks itself consumed before
callbacks, disables visible/touch flags, records the sound request, sends its
named pickup trigger and requests deferred removal. As with KeyPickup, trigger
sending moves before RemoveObject so the checked sender remains live. Reentrant
contacts cannot consume the same object twice. Ammo list links are removed only
at destruction; unload clears them and all typed message/string allocations.
No callback reclaims its current object immediately.

## SpawnObject

The original CDestructable aggregate owns SpawnObject and SpawnObjectVel.
First death sends DeathTrigger, then creates the object, then sends DamageTrigger;
brush debris/removal occurs in the subsequent scheduled update. Preserve this
order and the existing recursive-damage guard. If DeathTrigger removes the
source, the adaptation cancels its spawn and records why.

The selected global helper from Spawner.cpp splits class name from properties
without mutating the authored string. Only the twelve supported leaves above,
the item pickups MedKitPU, FlashlightPU, ProximitiesPU and RemotesPU and the
weapon pickups BerettaPU, SubMachineGunPU, FlareGunPU and AssaultRiflePU are
admitted (SniperRiflePU's weapon is not integrated); the Spawner class and
arbitrary object spawning are not imported.
The request is bounded to 100 bytes and validated against the existing typed
property grammar before brush creation. Unknown classes/properties, repeated
properties and malformed strings reject creation. Spawn velocity must fit the
engine's finite vector bounds (absolute components at most 10000000).

String creation receives the parent's current position/rotation. Explicit Pos
or Rotation properties may override them; missing string properties no longer
replace them with default zero/identity values. Velocity is applied after
INITIALUPDATE, matching the helper. Failed velocity assignment removes the new
object. A spawned pickup has an independent lifetime, surviving parent removal
and being reclaimed on world unload. It is not a SelfTrigger-style owned child.
Resource failure is reported separately from successful object allocation.

## String-created bouncing pickups

ItemPickup and WeaponPickup set the original `m_bBounce`; ammo, health and ward
pickups do not. On string creation (INITIALUPDATE_STRINGPROP) the base initial
update runs first and schedules no update, exactly as the source does while
`m_bBouncing` is still false. SpawnItem then stores the rest position, sets the
bounce acceleration to 500 and `bouncing`, and moves the pickup 10 units up with
the collision-checked MoveObject; FLAG_GRAVITY lets it fall back. Because no update
is ever scheduled, the source spring (`50 * (rest.y - y) - 3 * vel.y`) is never
reached in single player, so it is not ported. A failed position query or move is
recorded in the pickup error. Item pickups keep the single-player restriction;
SpawnWeaponPickup drops keep `SetBounce(false)` behavior (normal creation).

## Inspection and limits

The Pickups tab lists live, hidden, inactive and pending objects, type/amount,
resource ID/fallback name, model/skin, position, velocity, dimensions, trigger,
attempts and diagnostics. It also shows player pools/caps and the latest 32
consumption and destruction-spawn records. Reports own strings and retain only
checked child handle tokens. Unload clears them. The inspector does not execute
game rules; the GPU smoke collects ammo with inspection hidden and also runs
without Dear ImGui.

This increment does not implement weapon pickups, timed bonuses, inventory items,
multiplayer scaling/respawn, bouncing/rotating pickup FX, localized HUD strings,
audio, AI pickup seeking, save/restore or a full original session. It does not
make ammo-consuming weapons beyond the current Beretta/knife subset available.
ManaAmmoPU is commented out in the source; RocketAmmoPU is not guessed as an alias.
Legacy TypeAction schemas, missing/ambiguous world models and polygon dynamics
remain explicit failures. Rendering, retail-executable comparisons, additional
platforms and XR validation remain pending. M2's exit gate is unchanged.

The [medkit continuation](medkit_contracts.md) adds original MedKitPU acquisition,
CInvMedkit recharge/healing, H direct-item input, local activation audio and owned
charge preservation through Restore/KeepAlive. Other items, original inventory HUD,
pickup audio and safe NEW_LEVEL remain unavailable; M2 stays incomplete.

The [proximity continuation](proximity_contracts.md) adds original weapon49 in reserved slot10, item7 collection, P selection, charged/secondary throws and independent CProximityBomb lifecycle. Owned snapshots now contain thirteen slots, with only the first reserved slot implemented. The shared grenade/bomb service is named AdvanceModelProjectile. Earlier omissions/counts remain historical; M2 stays incomplete.

The [remote-bomb continuation](remote_bomb_contracts.md) adds weapon50 in reserved
slot11, ammo11/item8, RemotesPU, R selection and the separate B detonation command.
Charged/secondary throws share the source sequence without proximity overhold
explosion. Listed remotes belong to player teardown/Respawn; already signalled
charges have left that list. Restore/KeepAlive preserve weapon/ammo, never old bomb
handles. The shared model-projectile solver now admits CGrenade, CProximityBomb
and CRemoteBomb. Earlier remote omissions are historical; time bombs and the
complete M2 gate remain open.
