# Original NPC weapon drops and Beretta pickups

This extends [attachments](ATTACHMENT_CONTRACTS.md), [pickups](PICKUP_CONTRACTS.md)
and [NPC hit reactions](AI_HIT_CONTRACTS.md). The selected single-player source
path is `DropCurrentWeapon -> DropHandModel -> CHandWeaponModel::Drop/Update ->
SpawnWeaponPickup -> BerettaPU -> CPlayerObj MID_ADDWEAPON`.
[Source selections](M2_WEAPON_DROP_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_WEAPON_DROPS.md)
record its boundaries. M2 remains incomplete.

## Rules and ownership

Cabal lethal HandleDamage attempts the current weapon drop before reaching the
still-unavailable corpse/gib/death branch. Its original Dying/removal/node -1
and limb-injury order is unchanged. A refusal is ignored by this source caller;
the inventory records the reason. The NPC still suspends explicitly at the
missing death services. This is not a completed NPC death implementation.

Only single-player mode accepts drops or collection; multiplayer weapon pickups
remove themselves during initialization. A live current held weapon is required. Melee cannot be dropped. A dual inventory
drops the left slot first, retaining the right current weapon; otherwise the right
slot is removed. NPC Fire resets to rest synchronously in the original. The port
uses an active-Fire guard, including the left slot, to prevent a nested damage
callback from deleting the weapon whose Fire method is executing. Player-initiated
DropCurrentWeapon is explicitly unavailable until client view-state/input
synchronization is connected. Merely acquiring a pickup uses the existing real
inventory revision and next-frame client synchronization.

The hand detaches, resets to its owner's current position/rotation, receives
current animation dimensions with X=Z=max(X,Z,10), and gets velocity
150*(forward+up). Visible, gravity, no-sliding and remove-if-outside flags and a
one-second first check follow the source. The original unbraced SetObjectDims2
failure branch otherwise leaves rotation uninitialized; the port always reads a
checked rotation. Dimensions use the existing centered raw setter, not a new
retail depenetration claim. Existing rendering has no per-object stealth tint;
the source color reset has no connected tint state to reset.

Ownership transfers only after successful body setup. The weapon clears its
hand/attachment fields before destruction; the dropped hand clears its owner.
The hand survives NPC/weapon removal and unload reclaims it. Held models still
follow their original first-update visibility branch, even if they were dropped
before that callback. Checked handle tokens replace all borrowed weapon pointers.

The first update after one second checks fresh support, then retries every .01s
while airborne. Primary-world polygon support is represented by its real polygon
identity, not a fabricated world HOBJECT. On support, the original horizontal
facing and position +10Y create a non-bouncing BerettaPU. NPC drops include 12
bullets; the helper's player-owned variant includes zero. The hand requests
deferred removal only after successful pickup creation. Missing support services,
vertical-only facing or missing pickup model leave a stopped, inspectable hand
with a diagnostic; they do not discard the weapon or report a transfer. Failed
pickup allocations are reclaimed and do not retry indefinitely.

## Engine and collection

For ordinary normal/model bodies, FLAG_NOSLIDING (original bit 25) stops
MoveObject at its first contact without
spending the remaining displacement tangentially. It disables the stair branch;
normal velocity projection remains unchanged. This is a bounded replacement
solver, not verified retail physics. Dropped nonsolid bodies use existing generic
physics once, with primary-world collision. Support on arbitrary moving solid
bodies, retail slope response and collision recovery remain unverified.

WeaponPickup and BerettaPU retain their ancestry, filenames, resource ID 350,
weapon1.wav request and glow bit 7. World creation uses existing ground placement;
runtime drops retain their supplied position. The source bouncing string-spawn
path is rejected explicitly, and the general destruction SpawnObject whitelist
is unchanged. Other weapon pickup classes are unavailable.

MID_ADDWEAPON=1004 carries an owned typed Weapon pickup, type 1 and finite ammo
amount. Routing, kind/type/amount, mixed payload and outstanding acknowledgement
checks are the same as existing pickups. A living player first calls the original
ObtainWeapon selection (including a second Beretta). Successful acquisition draws
the original 15% optional voice chance; requests are counted but not played.
Unless there are no free slots, positive ammo is then offered to AddAmmo; accepting
ammo permits collecting an already-owned gun. A full dual inventory/ammo pool
refuses it. Zero-ammo duplicates remain available. Dead players cannot collect.
Successful pickup acknowledgement disables contacts before named callbacks and
removes the pickup only after callback completion. Pickup sound, client voices,
glow/powerup FX and HUD feedback remain requests, not working audio/FX services.

## Inspection and remaining work

The Pickups panel shows Beretta model/skin, amount, flags, attempts, consumed
records and detached hands including hidden/inactive/pending identities, position,
velocity, dimensions, next deadline and failures. It also shows NPC drop attempts
and successful detachments; recent spawn reports distinguish actual pickup
transfers and retain independent pickup handles.
These are read-only observations. GPU smoke collection runs with the UI hidden
and after reload, also in the inspector-free build.

No corpse, gib, dismemberment, death voice or other weapon implementation is
claimed. Gideon, ExitTrigger, required FX and the other M2 services remain open.
SubwayA source-entry accounting stays 202 created, four transferred sounds,
372 editor-only, two unsupported, zero failed. Dynamic pickups/hands are runtime
objects, not additional supported authored entries. Linux tests do not establish
other-platform, XR, retail timing or original-executable parity.
