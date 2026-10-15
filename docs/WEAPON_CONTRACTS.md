# Original Inventory and Vector Weapon Boundary

This M2 continuation ports selected original `CInventoryMgr`, `CWeapon`, server
`CBeretta`/`CMelee`, client `CViewWeapon` and client melee animation branches.
It supports the Beretta and Caleb's Chosen Knife. It does not claim a complete
inventory, complete combat or a playable session. See
[M2_WEAPON_SOURCES.json](M2_WEAPON_SOURCES.json) and
[validation](../../Documentation/Implementation/M2_WEAPONS.md).

## Ownership and entry

The player's original inventory aggregate owns ten right-hand and ten left-hand
weapon slots with `unique_ptr`. Weapons retain checked owner handles and a
non-owning pointer to their owning aggregate. They never own the player. The
existing linked muzzle-light object still belongs to the inventory; weapon
models use owned client resources, not invented server objects. Removing a
player releases both hands, models, strings and muzzle child through the existing
deferred lifecycle. Destruction inside a shot callback is deferred until dispatch
and client weapon processing finish. Exit, unload and reload also clear input.

The original initialization message obtains the knife in slot 0, then the nine
requested weapon slots. The default profile obtains a Beretta in slot 1 and
selects it. `DUAL_HAND_MASK` and a second Beretta populate the left hand in the
same slot. Unsupported weapon IDs fail before allocation. Failed initialization
releases its name and weapons; failed client model loading rolls back the whole
entry without changing the previously loaded world. Duplicate initialization is
rejected. Weapon selection loads required client resources before replacing the
old client views and restores the previous server slot on failure.

`ObtainWeapon` retains the original explicit-slot/first-empty/existing-dual-slot
search, including its commented-out strength restriction. All ammo pools retain
the source capacity table and strength/magic downgrade clipping. `AddAmmo`
clamps to capacity; `SetAmmoCount` is the original raw count setter, not a pickup
capacity operation. Invalid indices, negative/nonfinite amounts and absent
weapons fail. Infinite-ammo mode prevents decrement, as in the original setter.
World pickup objects, inventory items and bomb slots are not implemented.

## Client animation and server fire

`WeaponInput` is a device-independent primary/alternate/hand value. The inspector
supplies it at frame boundaries; animation runs in `GameSession::step` even with
no UI. Selection and held controls are diagnostic input, not the original full
combined movement/dual-hand fire controller. [MoveMgr standing movement](MOVEMENT_CONTRACTS.md) now executes before the camera and view weapons, supplying their same-frame origin. No keyboard/mouse assumption enters game code.

The client uses the source `CViewWeapon::UpdateFiringState` transitions for draw,
rest, idle, start/fire/end, alternate fire and holster, plus `PlayAnimation`'s
completion semantics. The original melee branch chooses `fire` or `fire2` while
resting. The existing ABC player delivers crossed **FIRE_KEY** markers, including
time zero and both markers of the knife's primary animations. Reload delays,
semiautomatic reset, alternate looping and missing-animation fallback follow the
selected original paths. Released input can still finish a committed attack.
Only firing-state markers emit a request, preventing stale draw/holster keys
from shooting; the selected retail assets have no fire keys in those clips.

The local message is an owned `WeaponFire` value: position, direction, byte seed,
weapon ID, alternate and hand. It goes through the server shell, live CPlayerObj,
`FireCurrentWeapon`, `CWeapon::Fire`, spread, eye alignment and vector intersection.
Wrong weapon/hand, dead/stale player, nonfinite vectors and unavailable collision
are rejected. The two imported `WeaponData` entries retain **all** source fields.

| Weapon | Primary damage | Alternate damage | Range | Ammo/use | Reload/alt |
| --- | --- | --- | --- | --- | --- |
| Beretta 92F | 8–12 | 10–20 | 3500 | bullet / 1 | 0.33 / 0.5 s |
| Chosen Knife | 15–25 | 35–60 | 75 | none | 0 / 0 s |

Damage is sampled before horizontal and vertical spread, as in the source;
spread divides by 2000 and the direction is normalized. An owned portable
`minstd_rand` replaces process-global `srand/rand`. Identical requests reproduce
identical shots in this implementation; **retail CRT random-bit parity is not
established**. Idle/knife/seed randomness is local to each view. The inspection
camera is the firing origin; legacy muzzle offsets, near-wall muzzle correction,
view bob and automatic dual-hand alternation remain pending. The source server
`Fire` computes a reload duration without enforcing it; cadence belongs to the
client animation state machine. Direct server test messages intentionally do not
add a second invented cooldown. This is local transport, not a multiplayer trust
boundary or network protocol.

`IntersectSegment` queries the primary solid triangle BVH and live solid object
AABBs, excluding the owner. The nearest hit wins; invalid bounds, stale ignore
handles and absent collision report distinct failure states. Unsupported queries
consume no ammo. A successful shot consumes ammo even on a miss. Melee uses the
same vector path without consuming an ammo pool. World hit position/normal,
polygon, object handle, fraction, seed and damage remain inspectable.

## Damage messages and limits

Object hits send a bounded typed `Damage` payload through the existing
`StartMessageToObject`/`EndMessage` and original object/aggregate callbacks using
source `MID_DAMAGE = 1001`. The payload owns its values and contains an opaque
attacker handle. Mixed/untyped, invalid, nonfinite and wrong-sender payloads fail.
The receiver borrows the message only during dispatch; every aggregate can read
it independently. A hit callback may remove either endpoint without freeing a
weapon while its method is executing. `damage_delivered` means delivery to the
object, **not** proof that it owns a damage aggregate.

The selected `CDestructable` ordinary normal/melee branch applies resistance,
source armor absorption thresholds (30/50/70/80/90%), armor exhaustion and health
reduction, god-mode/dead guards, and the dead/zero-health state. Health/armor/bullet
changes synchronize to the local client. This is a full-precision inspection
snapshot; the original HUD's packed integer flags and voice events are not ported.
AI hit limbs, damage impulses, death/gib reactions, damage triggers, difficulty/
powerup modifiers and multiplayer rules are not claimed. No AI target or pickup
class is fabricated to make SubwayA appear playable; damage receiver fixtures are
test-only original-aggregate owners.

Surface-aware glass/fire-through/liquid handling, transformed world-model/door
collision, attachments, hand models, skins, muzzle light rendering, decals,
tracers, shell ejection and audio playback remain pending. Sound paths and whether
they are indexed are exposed as requests, never reported as played sounds. These
limits and the remaining 190 required SubwayA instances keep M2's exit gate open.
