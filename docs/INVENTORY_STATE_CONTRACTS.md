# Original inventory Save/Load subset

> Continued by [damage state contracts](DAMAGE_STATE_CONTRACTS.md): original damage
> Save/Load values and death counter/delay/nigh-invulnerability now execute.
> Earlier blanket damage-save omissions are historical. Whole player restoration
> and safe level transfer remain unavailable; M2 stays open.

This ports the supported fields and ordering of original `CInventoryMgr::Save`
and `Load` into an owned typed `InventoryState`. It is an inventory aggregate
checkpoint, **not KeepAliveSave/KeepAliveLoad, a player save or a retail save-file
reader**. NEW_LEVEL remains refused before world/player teardown. See
[source selections](M2_INVENTORY_STATE_SOURCES.json) and
[validation](../../Documentation/Implementation/M2_INVENTORY_STATE.md).

## Original behavior

The snapshot owns ten right/left weapon type pairs, thirteen raw ammo pools,
strength/magic, infinite-ammo state, selected slot, damage multiplier and the
supported key strings/charges. Original Load recreates right then left weapons
in each slot, assigns raw ammo **after** recreation, and restores selection last.
Do not use pickup grants, capacity clamps or SetAmmoCount here: the last of those
refuses writes under infinite ammo. Counts above capacity are valid saved state.

`InventoryLoadType::KeepAlive` has source value 1 and **discards keys**. Ordinary
aggregate Restore (0) preserves their owned strings and charges. Neither mode is
general engine restoration. Source weapon objects are reconstructed from types;
last-shot time, shot counters, firing state and held-object identities are not
serialized and restart. Existing hand children are removed, and normal
UpdateHandModels creates new attachments against the destination owner.

Unported source fields (inventory god mode, bomb slots/items, selected active item,
fire-rate/melee multipliers and dropped-eye state) have no successful placeholder
serialization. Player profile requested_weapons is initialization input, not the
original saved inventory, and is cleared on Load. Player damage/health, movement,
relative timers, powers, client saved data and object associations are not captured.
A future full player checkpoint must explicitly account for those fields before
accepting transfer; it cannot treat this structure as sufficient.

## Validation and ownership adaptations

Save checks a live owner and no weapon currently executing its synchronous fire
callback. It validates the complete candidate before replacing the caller's output.
Load requires a live **unassociated** owner, a known load type and no active weapon
callback. Restoring an associated player's inventory is refused because its client
views and gameplay state are not yet transactionally restored.

Only supported Beretta/knife types are accepted. A left slot requires the same
right-hand type with source dual-hand support. Attributes must be 1..6, selection
0..9, ammo finite/nonnegative, and damage multiplier finite in the existing 0..1000
range. Empty selected slots remain representable. Keys are bounded to 256 distinct
case-insensitive names, nonempty/NUL-free strings of at most 100 bytes and positive
byte charges. Invalid data, including keys that keep-alive would discard, fails
before any object or existing inventory is changed.

Instead of source Term-before-read, Load stages the original Init and ObtainWeapon
operations. A muzzle-child or link failure destroys only the stage; old weapons,
keys, ammo, selection and revision remain intact. Commit swaps owned weapons/child,
rebinds every weapon to the destination aggregate, then defers old child removal
through existing engine lifetime rules. A stack-local aggregate address must never
escape in a restored weapon. Snapshots contain no engine handles, resource pointers
or borrowed strings and survive source destruction/unload.

## Session and inspection

At an accepted client exit request, outside callbacks, the server shell captures
the active associated player's inventory before the existing NEW_LEVEL refusal.
This follows KeepAliveSave's active-player selection restriction for the supported
single client. Capture failure clears the last candidate and exposes its error;
it never changes the current player or permits transfer. Dead/invalid client exit
requests never reach capture. PreStartWorld/unload clears the diagnostic record.

The Objectives tab shows capture attempts/success, validity/error, attributes,
selected slot, every saved weapon/ammo entry and keys. It explicitly labels
inventory-only coverage and missing full transfer. The data is captured during
simulation even with UI hidden; drawing the inspector performs no Save/Load.

Tests cover raw over-capacity/infinite ammo, nondefault dual slots, original key
removal, weapon transient reset, owned strings across source unload, malformed
values, fire guards, live-client refusal, injected child/link failures, pending
owner rejection and repeated cleanup. Private tests restore into a detached
CPlayerObj with real Caleb/held assets; they **do not** claim a whole player or
world transfer. The existing GPU exit scenario checks inventory capture while UI
is hidden and after world reload, keeping the current player/world unchanged.
