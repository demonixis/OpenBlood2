# Original triggers, door SelfTrigger and keys

Continuation: [original collection audio](pickup_audio_contracts.md) supersedes the
collection-sound omission for supported pickups and keys.
[Trigger audio](trigger_audio_contracts.md) now connects activation and lock cues;
character quips and other missing services remain separate.

> Extended by [conversation contracts](conversation_contracts.md): original dialogue sequencing and typed client communication are connected; authored missing mappings remain failures.

> Extended by [level-exit contracts](exit_contracts.md): original ExitTrigger and
> shell exit/load messages now execute. NEW_LEVEL still explicitly refuses missing
> keep-alive transfer before teardown; this is not a completed level transition.

> Extended by [objective contracts](objectives_contracts.md): original mission
> objective broadcasts and external localized client strings are connected.
> Other localization/audio/HUD and full M2 acceptance remain pending.

> Later extension: [pickup contracts](pickup_contracts.md) supersede blanket
> pickup/SpawnObject omissions below for eight ammo and four health/armor classes.

> Extended by [interaction_contracts.md](interaction_contracts.md): original player USE and RotatingDoor
> are connected with bounded atomic collision. Earlier omissions below describe
> their previous increments; the full M2 gate remains open.

This extends [door_contracts.md](door_contracts.md),
[weapon_contracts.md](weapon_contracts.md) and
[object_contracts.md](object_contracts.md). M2 remains open.
See [source selections](../implementation/sources/m2_trigger_sources.json) and
[execution evidence](../../../Documentation/Implementation/m2_triggers.md).

## Original rules retained

`Trigger`, obsolete `BigTrigger` (same behavior) and `ToggleTrigger` are selective
ports of `ObjectDLL/Trigger.cpp`. They retain ten target/message slots,
`MessageDelay` for slot 1 and `MessageDelay2`–`10`, reset timing, activation
counts, touch/player/AI/object/relay/named filters, lock/key queries and toggle
on/off messages. Generic target/message properties retain the original
200-character limit; the old general filename limit of 100 is insufficient.
Commands and key/named-filter comparisons are ASCII case insensitive. Named
message delivery still uses the existing exact-name object lookup and stable
list snapshot. A target named by `REMOVE` is removed through the original helper.
Slot send counts mean dispatch attempts, not successful delivery to a live target.
The inspector separately counts matching live targets, including zero matches.

Important source behavior:

- `TouchActivate` gates contacts, not incoming `TRIGGER` messages. Sender type
  filters apply to both. Trigger relays, AI and ordinary objects can bypass a
  player lock when their respective activation flags permit them. Disabling
  relay activation still allows a Trigger through the ordinary-object branch
  if `ObjectActivate` is enabled. Player and AI branches do not fall through.
- Only the player's locked branch queries a key. The synchronous response can
  unlock before the same activation continues. The named-object filter runs
  **after** this query, so a use can be consumed even if that filter then rejects.
  The query also precedes the activation counter.
- The original early `active` guard applies to **all** incoming commands:
  `RESET`, `LOCK` and `UNLOCK` do not reactivate an inactive trigger. RESET on an
  active trigger cancels its timer but does not clear its counter/pending slots.
- With no delayed slots, activation sends the batch and schedules ResetTime.
  With delays, zero-delay slots send immediately, pending slots send when due,
  and the reset interval starts after the last delayed slot. An update with no
  pending slots sets active again. The session still dispatches at most one
  scheduled update per object per step.
- ResetTime=0 leaves an activated trigger inactive indefinitely. Authored -1
  values are retained and mapped to the same disabled timer. This is an explicit
  adaptation informed by later LithTech's documented nonpositive-update rule,
  not a verified observation of the Blood II executable. The engine's general
  timer API continues to reject negative delays. Negative message delays keep
  their source arithmetic: they become pending only when some slot has a
  positive maximum delay; otherwise the immediate batch path runs.
- ActivationCount and its counter keep 32-bit unsigned semantics, including a
  negative authored integer converting to its unsigned value.
- ToggleTrigger changes on/off only through the immediate `SendMessages` path.
  The original delayed `SendMessage` path uses the on-message without toggling.
- Sound requests are recorded. The source key-query response does not play the
  commented-out unlocked sound; explicit UNLOCK can request it.

## Door ownership and lifecycle

A Door with SelfTrigger creates an original Trigger at its initial pivot, before
StartOpen displaces the door. All-zero TriggerDims uses the door's half-dimensions
plus one on each axis; otherwise the authored dimensions are retained. The sensor
stays at that initial position, rather than following the moving door. Setup
copies activation/key/sound properties and targets the door's name with TRIGGER.
SetOpening disables it; SetClosed re-enables it. LOCK/UNLOCK forwards to this
child with the original sender. No child means no lock relay. Direct TRIGGER on
Door remains independent of the child's lock, as in the source.

The new implementation explicitly owns the child through a checked inter-object
link. Removing either endpoint clears the parent's handle; removing the parent
also removes its sensor. Pending parents cannot activate the sensor or consume
keys before deferred destruction drains. Child creation failure rejects the door
instead of creating a silent, nonfunctional SelfTrigger request. Snapshot work,
generation-checked UI selections and deferred destruction remain mandatory.

An activation guard covers immediate, delayed and toggle sends. Recursive target
cycles cannot repeatedly increment counters or consume keys. This extends the
source's narrower SendMessages guard to paths that were unprotected. Once a
trigger is pending removal, remaining slots stop sending. Last sender and owner
are safe tokens, not raw pointers. They are checked before inspection/use.

## Key inventory and pickup messages

Selected `CInventoryMgr::AddKey/QueryKey`, `CInvKey::Setup`, and CPlayerObj's
MID_KEYQUERY/MID_KEYPICKUP branches use a plain vector of owned key records.
Each keeps its name, display name, normal/highlight icon paths and byte use count.
The already ported `Key` class is a path/animation marker, not an inventory key.
Case-insensitive duplicates do not stack or replenish counts. Zero uses rejects.
A successful query decrements exactly once, removing the entry at zero. Inventory
is limited to 256 distinct keys. Player exit destroys it. Dead players do not
answer queries or accept pickups. Key inspection does not consume a use.

Original IDs 3500/3501/3502 and MID_PICKEDUP=1005 are retained. Typed, owned
`KeyMessage` values replace the legacy string/byte/float wire layout. They cannot
mix with byte streams or damage payloads; strings are bounded and embedded NULs
reject. Sending/reading requires a live sender. The actual player BaseClass sends
replies, replacing the source's invalid cast of the inventory aggregate to a
BaseClass. A trigger accepts a response only from its current synchronous queried
player; a pickup accepts acknowledgment only from its current toucher. These are
local lifecycle checks, not a multiplayer authentication protocol.

`KeyPickup` includes the needed single-player `PickupObject` branches directly;
there is no claim that the general PickupObject/item framework is ported. It
reads original asset/key/use-count/target properties, uses real ABC animation
dimensions (X at least 10, Z at least X), requests ground placement for world
creation, and delivers a key on player contact. Successful acknowledgment hides
it, sends its pickup target/message, and requests deferred removal. There is no
respawn in this supported single-player branch. Original SAVEABLE/night-goggle
flags are retained, without claiming save or glow rendering.

The source default `models/powerups/key.abc` is absent in this installation.
Actual authored pickups use key_pu, keycard_pu, skullkey_pu and parachute_pu models.
Missing/undecodable models remain inspectable, with touch/gravity disabled;
no guessed pickup box is substituted. Nonzero ResourceNum keeps its numeric ID
and explicitly displays ObjectName as a fallback until localized resources are
ported. Icons, pickup audio and effects are metadata/requests, not rendered or
played. General pickup respawn, thrown/bouncing items, inventory HUD selection,
level-transition/save persistence and other item types remain pending.

## Collision and inspection boundaries

Ordinary movement already sweeps nonsolid touch sensors over the accepted body
path, including a stationary overlap. This extension also checks transported
solid bodies against nonsolid sensors after an atomic world-model translation.
Blocked pusher/rider plans send no contacts. Callbacks and removal remain deferred;
recursive motion is rejected. Direct world-model-to-sensor contacts, rotational
sweeps, retail contact priorities and full physics parity remain unimplemented.

Nonsolid objects carrying GRAVITY now collide with the primary world unless
GOTHRUWORLD is set, so keys rest on the floor without becoming solid player
obstacles. This bounded extension does not implement all nonsolid world-model,
platform or impulse interactions. The existing replacement integrator remains.

The **Triggers** tab shows all live sensors, including inactive and door-owned
ones, positions/dimensions, filters, counters, timers, delayed slots, target
matches, key queries, sender, toggle state, audio requests and player keys.
World key pickups expose models, icon paths, ResourceNum and diagnostics.
The **Doors** tab links to its actual child. Commands queue at a frame boundary
and use the live player as sender. An explicitly labeled debug key grant bypasses
pickup and is separate from original rules. Simulation also runs without ImGui.

Inspector TRIGGER is a direct test command, not the original 82-unit player USE
ray, now connected alongside RotatingDoor by [interaction_contracts.md](interaction_contracts.md).
Complete AI, audio/localized feedback, save/restore and full gameplay remain pending.
Original B2BaseClass difficulty/game-type filtering now carries an inspection
marker so initial-update removals are reported as filtered, not creation failures.
