# Original proximity bombs

Read [object lifetime](object_contracts.md), [weapon rules](weapon_contracts.md),
[M16 model-projectile physics](assault_contracts.md), [pickup transport](pickup_contracts.md),
[inventory preservation](inventory_state_contracts.md) and [device input](player_control_contracts.md).
The [source ledger](../implementation/sources/m2_proximity_sources.json) and
[implementation evidence](../implementation/reports/m2_proximity.md) record this bounded port.
M2 remains incomplete.

## Acquisition and selection

`ProximitiesPU` retains ItemPickupWeapon/ItemPickup ancestry, `Proximities_pu`
model/skin, item ID 7, resource 306, quantity one and `ammo2.wav` pickup request.
The weapon-pickup glow bit is set before the base initial update; the base now
preserves preexisting user bits as the original does. Single-player world creation
is supported. Multiplayer respawn/scaling and actual pickup sound output remain
unavailable; unsupported schemas still reject. The later [pickup contracts](pickup_contracts.md) supersede the string-creation refusal: string-created item/weapon pickups follow the original bouncing SpawnItem.

Original AddInventoryWeapon creates weapon 49 in reserved slot 10, distinct from
the ten ordinary slots. Later pickups add ammo pool 10, capped at ten independently
of attributes. A full pool refuses the checked pickup acknowledgement; removal
is deferred. First creation keeps the original ignored AddAmmo result. The simple
owned weapon/pool representation replaces the separate CInvProxBomb menu-list
node; localized menu names and PCX inventory icons are not implemented.

P selects a collected bomb through the typed client command and original
SelectInventoryWeapon branch. It respects focus, text editing, dead-player and
camera/movement restrictions. Unlike the medkit direct-item command, the original
bomb command is not blocked by RUN. Empty ammo cannot select a different bomb
slot, but an already selected weapon returns CURRENT before the ammo test.
The inspector can also select slot 10. Normal exhaustion follows original
SelectNextWeapon: reset an inventory slot to zero, search ordinary slots starting
at one, wrap, then switch. The overheld early-return branch deliberately does not
perform this fallback. No weapon is deleted while its Fire callback is active.
Ordinary ChangeWeapon/FindBestWeapon searches remain restricted to slots 0..9.
Inventory-bomb acquisition and selection reject an owner pending removal before
changing ammo or selection.

Typed inventory snapshots contain thirteen slots; only slot 10 accepts proximity
weapon 49, only in the right hand. Slots 11/12 remain reserved and empty; other
weapons stay in 0..9. An empty selected reserved slot rejects before mutation.
Restore/KeepAlive reconstruct the weapon and preserve raw ammo and selection,
including zero ammo; key removal remains unchanged. The bomb menu entry is derived
from the weapon/pool, with no serialized pointer. Throw charge, active projectiles,
full inventory menus and retail save-file ABI are not captured.

## Throwing and contact rules

All original WeaponData fields are retained, including the unusual resource 368
in the flash-radius column and zero name-ID field. Each mode consumes one bomb,
uses damage 80–120 and radius 250. The client subclass preserves its before/after
base-state ordering: primary loops while held, release reaches end_fire's FIRE_KEY,
then redraws. Holding charge yields `250 + 1500 * min(held/.75, 1)` units/s;
strictly more than 1.25 seconds sets special speed zero and finishes the throw.
The typed special float is finite and bounded to 0..1750 before consumption.
Power-bar state is inspectable; the original power-bar HUD is unavailable.

Speed zero takes the original in-hand branch: EXP_DEFAULT_SMALL, actual
`time/explode.wav`, NORMAL damage to the owner only and no projectile/radius pass.
A normal throw creates CProximityBomb with the charged speed. Secondary places
it at the owner's body center, directed downward, with original alternate speed
zero; gravity supplies the motion. Both retain original fire WAVs and existing
hearing. The secondary body/down branch does not execute an invalid zero-length
hitscan query. Primary uses the existing player eye alignment/close obstruction
check. FIRE_KEY on end_fire is accepted specifically for this inventory weapon.
A selection change during a callback prevents later markers from firing the old
weapon. No damage is produced merely by rendering the view model.

CProximityBomb uses actual c4.abc/c4_prox.dtx, HP5, 1-unit dimensions, model/RAYHIT/
gravity flags and a 0.25-second death delay. Source constructor spin consumes two
random draws in +/-2pi. Runtime creation only; missing models fail. Its Update
never calls base Update, so the inherited ten-second lifetime is **not** a fuse.
Original subclass-before-aggregate ordering also applies to death-delay countdown.

An unarmed bomb ignores its shooter, nonsolid objects and dead characters. A
living character uses base contact, then bypasses the detection delay. Other
solid/world contacts align to the plane normal plus a local-right quarter turn,
clear gravity, start the arming clock and play stick.wav at the cached update
position. The cached position starts at zero as an explicit initialization repair.
Already armed contacts do nothing. Bombs are not parented to moving surfaces.

Armed Update zeros velocity. A still-valid AI shooter changes getaway time from
0.75 to two seconds; the value remains two if that shooter later disappears.
At the deadline it plays arm.wav once and scans the original destructible list
in order for a living CBaseCharacter within **inclusive** center distance 225.
The shooter is eligible; there is no faction, solid, visibility or wall filter.
Detection latches the explosion even if the target subsequently moves or vanishes.
The next callback plays detect.wav once while time < detection + .5; at that
boundary the bomb explodes. Death bypasses detection delay once IsDead is true.
The original medium explosion uses actual proximities/explode.wav, independent
available FX components and existing original squared-distance radius damage.
Unavailable composite/illumination components remain explicit.

## Engine boundary and inspection

The grenade solver is now named `AdvanceModelProjectile` and accepts only the
implemented CGrenade/CProximityBomb classes. Existing swept-box gravity, bounded
substeps, shooter exclusion, post-contact velocity projection and deferred target
callbacks are unchanged. Bomb flight stops after a sticking contact; generic
object physics still excludes all CProjectile instances. This is replacement
1-unit-box collision, not recovered retail physics, moving-surface attachment or
verified original-game parity. Embedded/invalid starts and excessive catch-up fail.

The original shooter link clears on either-end removal; projectiles remain
independent. Detection stores a checked historical token and does not own the
character. Detonation damage uses the existing checked callback snapshot and
reentrancy guard. Deferred removal never frees a C++ object inside its callback.
Sound failures are retained; flags still prevent invented retries. Unload frees
owned resources and voices. Destination rehearsal adds no mutable game global.

Objects shows arming/detection, contacts, spin, delay/radius, sound status and
current resources, including hidden/inactive instances. Weapons exposes slot10,
ammo, held duration, speed and firing state. Terminal projectile history retains
arming/contact/detection scans and failures after removal. Model rendering uses
existing SDL_GPU buffers/shaders and does not advance gameplay. No dependency,
shader, reference repository or commercial asset is added.

Remote bombs, flashlight, missing fallengrills geometry, full composite lighting,
bomb projectile save/restore, original item HUD and complete M2 acceptance remain
open. This increment is not a completed CityHub playthrough or campaign transfer.

The [remote-bomb continuation](remote_bomb_contracts.md) adds weapon50 in reserved
slot11, ammo11/item8, RemotesPU, R selection and the separate B detonation command.
Charged/secondary throws share the source sequence without proximity overhold
explosion. Listed remotes belong to player teardown/Respawn; already signalled
charges have left that list. Restore/KeepAlive preserve weapon/ammo, never old bomb
handles. The shared model-projectile solver now admits CGrenade, CProximityBomb
and CRemoteBomb. Earlier remote omissions are historical; time bombs and the
complete M2 gate remain open.
