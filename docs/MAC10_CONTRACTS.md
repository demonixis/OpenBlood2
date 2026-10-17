# Original Ingram Mac-10

The [M16/grenade continuation](ASSAULT_CONTRACTS.md) adds ID 9 and mode-specific ammo, original grenade callbacks and explicit finite key-bound metadata warnings. Earlier supported-weapon limits are historical; remaining services and M2 stay open.

Read the [source ledger](M2_MAC10_SOURCES.json), [weapon](WEAPON_CONTRACTS.md),
[fire audio](WEAPON_AUDIO_CONTRACTS.md), [impact](WEAPON_IMPACT_CONTRACTS.md),
[drop](WEAPON_DROP_CONTRACTS.md), [inventory-state](INVENTORY_STATE_CONTRACTS.md)
and [AI fire](AI_FIRE_CONTRACTS.md) contracts. This extends their supported weapon
set; it does not replace their lifetime, damage or original-code boundaries.

## Original rules and animation

Weapon ID 2 uses the complete original Mac-10 WeaponData, pistol animation column,
bullet pool, one round/vector per shot, 8–12 damage, range 3500, primary spread
200/100 and alternate spread 75/25. Both fire animations loop. The .1/.5-second
reload fields govern entry into the source firing state; they do not impose an
invented per-marker rate limiter. FIRE_KEY events drive each client shot, so the
actual animation duration governs sustained firing. NPCs retain their original
standing-fire cadence and aiming, including the case-sensitive SUBMA prefix.

CSubMachineGun uses the existing generic CWeapon vector/impact/damage/hearing/audio
services. Four retained fire-sound slots, ammo acceptance, active-Fire guards and
deferred callback removal stay unchanged. No new global or dependency is added.

Original CViewWeapon::SetupViewModel maps absent idle to static_model. If alt_fire
is absent, all three alternate indices copy the corresponding primary indices.
The port now includes these previously omitted source rules for every supported
view weapon. The installed left Mac-10 has only static_model, draw and fire;
its right model also has start_alt_fire, alt_fire and end_alt_fire. Do not invent
an alternate clip or suppress the test: the real left alternate action must emit
primary-animation FIRE_KEY events with the alternate weapon parameters/audio.

## Acquisition, ownership and restore

SubMachineGunPU retains the original WeaponPickup ancestry, Mac10_pu ABC/DTX,
resource ID 354, display name and 20 bullet value. MID_ADDWEAPON accepts supported
weapon IDs 1 and 2 only. The first acquisition adds a right weapon, the second
adds a left weapon; later pickups require accepted ammo. Full dual inventories
with full ammo refuse collection, and zero-ammo duplicates remain in the world.
Existing dead-player gates and single-owner acknowledgement apply unchanged.

NPC DropCurrentWeapon detaches a Mac-10 hand with the existing checked ownership
transfer, original velocity, first-update wait and support check. It survives
owner deletion, then creates the actual SubMachineGunPU with 20 bullets and only
then removes the hand. The helper's no-ammo variant stays zero. Spawn history
records the actual pickup class. Player drops, string-spawn bounce and arbitrary
SpawnObject weapon aliases remain unavailable. The ordinary box solver remains
replacement physics, not a retail-equivalence claim.

Typed inventory snapshots accept ID 2 in both hands and rebuild owned weapons and
inventory back-pointers; no transient shot state or raw pointer is serialized.
Unchanged negative tests now use unsupported Flaregun ID 3 instead of supported
ID 2. Other unsupported IDs still fail. World preparation and isolated destination
rehearsal see the new class without granting permission for a live transition.

## Inspection, evidence and limits

Weapons shows the real definition, primary/alternate loop and reload values,
current ABC clip, fallback, FIRE_KEY/shot/ammo/audio state and original resource
paths. The explicit diagnostic grant can acquire a second Mac-10; authored pickup
collection remains separate. Existing pickup, attachment, NPC and spawn panels
expose hidden, removed and failed objects. Rendering never advances gameplay.

Tests cover real private ABC/DTX/WAV resources, both hands and firing modes,
release/dry fire, health damage, reentrant shooter removal, pickups/full pools,
independent dropped-hand lifetime, restore and autonomous Cultist attacks in a
labelled synthetic arena. The GPU test renders both actual textured view models,
checks pixels on both sides, and unloads/reloads with no inspector dependency.
Private generated captures stay in ignored build directories.

Chrome/environment mapping from CSubMachineGun's original constructor is not
rendered; the multiplayer-only damage boost is not implemented because multiplayer
is unavailable. Muzzle flashes/illumination, recoil, casing ejection and stock-fold
SOUND_KEY playback remain unimplemented (the latter is only indexed). Actual fire
and selected impact WAVs use existing playback. No full encounter, complete
CityHub gameplay, retail timing parity or additional platform support is claimed.
Required remaining services and safe NEW_LEVEL keep M2 incomplete.
