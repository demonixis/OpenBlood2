# Original Inventory and Vector Weapon Boundary

> The [compact HUD continuation](HUD_CONTRACTS.md) now displays actual current ammunition and supports numeric owned-slot commands. Earlier blanket HUD omissions remain historical; original artwork and the M2 gate are still incomplete.

The [M16/grenade continuation](ASSAULT_CONTRACTS.md) adds ID 9 and mode-specific ammo, original grenade callbacks and explicit finite key-bound metadata warnings. Earlier supported-weapon limits are historical; remaining services and M2 stay open.

The [Flare Gun continuation](FLARE_CONTRACTS.md) adds original ID 3, primary sticking/burn ticks, alternate fragments, pickup/drop and sprite tint/alpha. Earlier blanket omissions are historical for this subset; remaining effects, death services and M2 stay open.

The [Mac-10 continuation](MAC10_CONTRACTS.md) adds original weapon ID 2, its pickup/drop, NPC use and both-hand view animation fallback. Earlier Beretta-only statements below are historical for this supported subset.

> Extended by [weapon impact contracts](WEAPON_IMPACT_CONTRACTS.md): original multi-hit
> material rules, selected impact audio and automatic impact hearing now execute.
> Earlier impact-audio omissions are historical; visuals and the M2 gate remain open.

> Extended by [weapon fire-audio contracts](WEAPON_AUDIO_CONTRACTS.md): Beretta/knife
> retained playback and automatic gunfire hearing now execute; source weapon voices
> survive disposable destination rehearsal. Earlier fire-audio omissions are historical.
> Required services and safe NEW_LEVEL still keep M2 incomplete.

> Continued by [damage state contracts](DAMAGE_STATE_CONTRACTS.md): original damage
> Save/Load values and death counter/delay/nigh-invulnerability now execute.
> Earlier blanket damage-save omissions are historical. Whole player restoration
> and safe level transfer remain unavailable; M2 stays open.

> Extended by [inventory state contracts](INVENTORY_STATE_CONTRACTS.md): supported
> original inventory Save/Load and exit-time capture now execute. Full player
> preservation/association and safe destination entry remain unavailable; NEW_LEVEL
> is still refused and M2 remains open.

> Extended by [NPC hit/recoil contracts](AI_HIT_CONTRACTS.md): original visible-node
> localization, region damage and nonlethal reactions now execute for initialized
> Cultist/Civilian NPCs. Earlier blanket localization/recoil omissions are historical;
> lethal death/corpse/drop services remain unavailable and M2 is incomplete.

> Extended by [AI weapon-action contracts](AI_FIRE_CONTRACTS.md): supported weapon
> choice, aiming, clearance, standing fire and AI fire_key now execute. Earlier
> blanket weapon/fire omissions are historical; full NPCs and M2 remain open.

> Extended by [AI perception contracts](AI_PERCEPTION_CONTRACTS.md): original
> candidate lists and stimulus/target selection now execute. Automatic NPC states
> and complete M2 remain unavailable; earlier acquisition omissions are historical.

> Later extension: [pickup contracts](PICKUP_CONTRACTS.md) supersede blanket
> pickup/SpawnObject omissions below for eight ammo and four health/armor classes.

> Extended by [BRUSH_CONTRACTS.md](BRUSH_CONTRACTS.md): RotatingBrush and
> selected destructible brushes are connected. Earlier omissions/counts below
> describe their previous increments; the full M2 gate remains open.


> Key inventory, pickup and query messages are extended by
> [TRIGGER_CONTRACTS.md](TRIGGER_CONTRACTS.md); prior key/pickup omissions below
> describe their earlier increment. General item/combat coverage remains pending.

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

[Camera contracts](CAMERA_CONTRACTS.md) extend input and late FIRE_KEY gating.
The selected weapon path retains the player body eye as its origin; the external
CameraObj view is separate. Blocked is an acknowledged no-shot result, with no
ammo consumption. View-weapon visibility is a presentation request.

The [AI visibility continuation](AI_SIGHT_CONTRACTS.md) adds selected original CanSee
and a typed ray object filter. Default weapon/USE ray behavior is unchanged.
Visibility queries do not implement target selection, AI states or complete M2.

The [held-model/attachment continuation](ATTACHMENT_CONTRACTS.md) adds original
Beretta/knife held children and typed visual attachments. Earlier blanket hand-model
and attachment omissions are superseded for this bounded subset. Dropping, physical
attachments, client visibility SFX and complete M2 remain unavailable.

The [animated model continuation](MODEL_VIEW_CONTRACTS.md) supersedes earlier model/skin rendering omissions for existing ABC/held/view models. It also corrects the absent-TransformInfo quaternion default at the reader boundary, shared by all pose/node/attachment queries. Explicit zero remains direct and nonzero flags remain unsupported. Close-view projection is replacement presentation; required gameplay/FX and M2 remain open.

The [Naga weapon continuation](NAGA_WEAPON_CONTRACTS.md) extends the supported NPC weapon subset with original eye-beam damage, head_gun aiming and red-small beam presentation. Gideon and the eye impact composite remain unavailable; earlier blanket weapon omissions are superseded only for this subset.

The [shield/powerup continuation](SHIELD_WEAPON_CONTRACTS.md) now implements original CGideonShield and owner-bound ObjectFX19/36. Earlier blanket shield/ramp/object-particle omissions are superseded only for these selected recipes. Boss actions, required remaining services and M2 stay open.

The [remote-bomb continuation](REMOTE_BOMB_CONTRACTS.md) adds weapon50 in reserved
slot11, ammo11/item8, RemotesPU, R selection and the separate B detonation command.
Charged/secondary throws share the source sequence without proximity overhold
explosion. Listed remotes belong to player teardown/Respawn; already signalled
charges have left that list. Restore/KeepAlive preserve weapon/ammo, never old bomb
handles. The shared model-projectile solver now admits CGrenade, CProximityBomb
and CRemoteBomb. Earlier remote omissions are historical; time bombs and the
complete M2 gate remain open.
