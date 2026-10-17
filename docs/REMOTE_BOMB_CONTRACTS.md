# Original remote bombs

Read [proximity bombs](PROXIMITY_CONTRACTS.md), [object lifetime](OBJECT_CONTRACTS.md),
[inventory state](INVENTORY_STATE_CONTRACTS.md), [player controls](PLAYER_CONTROL_CONTRACTS.md)
and [safe level transfer](LEVEL_CHANGE_CONTRACTS.md). See the
[source ledger](M2_REMOTE_SOURCES.json) and [evidence](../../Documentation/Implementation/M2_REMOTE_BOMBS.md).
**M2 remains incomplete.**

## Collection, selection and throwing

`RemotesPU` retains ItemPickupWeapon ancestry, item8, quantity one, name resource307,
`Remotes_pu` model/skin, ammo2.wav request and the original glow flag. The inherited
single-player/world-file restriction and rejected TypeAction schemas remain.
Actual pickup sound output, string-spawn bounce and multiplayer are unavailable.

Original AddInventoryWeapon uses weapon50, reserved slot11 and ammo11, capped at
ten independently of attributes. Slots0..9 remain ordinary weapons, slot10 remains
proximity and slot12 remains unimplemented. Neither normal weapon search nor AI
FindBestWeapon scans reserved slots. A full pool refuses collection. First creation
preserves the ignored first AddAmmo return. Owned weapon/pool state replaces the
separate CInvRemoteBomb menu node; original menu/PCX HUD remains absent.

Physical **R** selects collected remote bombs, **B** sends the separate original
COMMAND_DETONATE action. These are application bindings, not recovered retail
key configuration. Both use typed client identity, focus/text suppression, press
edges and player-change resets. Camera/movement/dead-player gates remain; RUN
is allowed. Detonation does not require remote selection or remaining ammo.
The inspector queues its detonation button at the owning frame boundary.

CWeapRemoteBomb shares the original charged-throw state sequence and special float
with proximity bombs. Holding reaches 1750 units/s after .75 seconds; the source
remote overhold-explosion branch is commented out and remains disabled. Release
uses actual end_fire FIRE_KEY. Secondary still **drops** a charge at body center,
downward with zero initial velocity; it is not the detonation command. Primary
special speed zero creates a stationary projectile rather than self-damage.
Malformed/nonfinite/out-of-range speed fails before ammo consumption. Normal
exhaustion uses the original next-ordinary-weapon fallback.

## Projectile rules and player list

CRemoteBomb uses c4.abc/c4_remote.dtx, original model/gravity/RAYHIT flags, HP5,
1-unit dimensions, two random angular velocity draws and .25-second damage-death
delay. Update never calls base Update, so it has no ten-second fuse. It has no
proximity detection, arming delay or stick/arm/detect sound.

Contact ignores the shooter and nonsolid objects. The first contact with any
CBaseCharacter, including a dead one, zeros velocity and marks Falling; subsequent
character contacts leave velocity alone. Gravity and spin remain active. Contact
with other solid geometry aligns to the normal plus a local-right quarter turn,
clears gravity and marks Armed; subsequent contacts do nothing. Armed Update zeros
velocity. No moving-surface attachment is invented.

The existing AdvanceModelProjectile service now also accepts CRemoteBomb. All
existing swept-box/substep/container/finite-input/deferred-touch limits remain;
armed bombs stop remaining substeps. This is replacement collision, not recovered
retail physics. Generic model physics must continue to exclude CProjectile.

A successfully configured player-fired remote is added to the original player
ObjectList in newest-first order and linked player-to-bomb. The bomb already holds
its separate shooter link. The implementation validates the exact class/owner,
prevents duplicate registration and unwinds list/link failure. Tokens are checked,
non-owning engine identities. MID_LINKBROKEN removes stale entries; explicit breaks
do not emit notifications. Empty lists persist until the source command/reset or
destruction releases them.

The engine also sends outgoing-link notifications to a pending owner before its
destructor. The player handles remote `MID_LINKBROKEN` removal only while alive:
a pending player must retain its list so original destruction removes the charges.
Live-player notifications still remove externally destroyed charges immediately.

COMMAND_DETONATE walks the list, latches each live bomb's explode bit, breaks the
player links, and releases the list. It performs no synchronous damage. The next
scheduled bomb Update removes itself from any still-existing owner list, then
runs original explosion/radius damage. Source EXP_DEFAULT_SMALL and time/explode.wav
are preserved. Existing independent FX component failures remain visible; the
shared small/medium publisher does not alter proximity behavior. Terminal reports
retain actual Armed/Falling/contact state and damage-delivery cancellation.

The player's original destructor and Respawn remove every still-listed charge,
then release the list. This differs intentionally from independent proximity
bombs. Already signalled bombs have left that list and may finish after the player
is removed, with null shooter attribution after link break. Removal during explosion
damage remains deferred; no callback destroys its current C++ instance.

## Restore and inspection

Supported Restore/KeepAlive preserve slot11, raw ammo and current selection. Invalid
slot/type/hand combinations reject before mutation; slot12 stays unavailable.
The original player save subset does not serialize the active bomb list. The old
player destroys listed charges; the restored player starts with no remote handles.
World projectile save/load and whole-world checkpoint restoration remain absent.

Weapons exposes selected ammo/charge, list size, command/signal totals and errors.
Objects exposes actual hidden/live model resources, Armed/Falling, spin, contacts,
shot damage/radius and the unused inherited fuse. Terminal reports survive removal.
Rendering reads existing ABC clocks and never advances gameplay. No new mutable
original global, dependency, shader or commercial asset is introduced.

FlashlightPU, missing fallengrills world geometry, full original item HUD and
complete M2 acceptance remain open. Counts are evidence of supported entries,
not a completed campaign or a verified retail playthrough.
