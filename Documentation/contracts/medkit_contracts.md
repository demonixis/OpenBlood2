# Original MedKit pickup and inventory action

This selected original `MedKitPU` / `CInvMedkit` path extends
[pickups](pickup_contracts.md), [inventory preservation](inventory_state_contracts.md)
and [device controls](player_control_contracts.md). See the
[source ledger](../implementation/sources/m2_medkit_sources.json) and
[evidence](../implementation/reports/m2_medkit.md). M2 remains incomplete.

## Source rules

`ItemPickup` / `ItemPickupCharged` / `MedKitPU` preserve their class ancestry,
original model/skin `Medkit_pu`, resource ID 302 and `inventory1.wav` pickup sound
request. Single-player touch carries original MID_INVENTORYITEMTOUCH (1006), item
ID 1 and quantity 1. The existing checked request/acknowledgement prevents double
consumption, rejects dead players and defers removal until callbacks finish.
Full medkits refuse collection. Both first acquisition and subsequent recharge
set 100 charges, regardless of the incoming byte quantity. Zero-charge inventory
items remain present and rechargeable. Additional implemented item IDs are covered by
the [inventory-use contracts](inventory_use_contracts.md); unsupported IDs fail explicitly.

Activation uses `min(100 - health, charges)`, calls original Heal for positive
values, queues the player action and subtracts the **integer requested gain**.
Keep the original differences between requested healing, actual healing and the
integer return value: Heal may clamp/refuse at a lower maximum but charges still
get spent; a fractional heal below one changes health and sends audio without
spending a charge or returning success; health above 100 may return a negative
nonzero success without healing, charge consumption or audio. Do not normalize
these quirks to modern gameplay expectations. Nonfinite/out-of-bounds engine
health and missing/pending owners fail before arithmetic or mutation.

Original direct-item command gates reject dead players, movement/camera blocking
and the source CTRLFLAG_RUN exclusion. H maps to COMMAND_INVITEM_1's selected
branch through a typed client command. It is press-edge activated, independent of
movement integration, and respects window identity, focus loss, F2, text editing,
held-key suppression and player replacement. Release Shift before use. Bindings
are application policy, not recovered retail configuration. Inspector drawing
never heals or consumes inventory.

## Audio and inspection

Medkit activation sends InventoryAction type 1. The shared transport also accepts
the implemented flashlight, night-goggle and binocular actions; TheEye uses its
separate view message. The existing
256-event client queue remains bounded. Queue failure is visible and does not
undo original healing. The client handles the action using the original
`sounds/powerups/medikit.wav`, local volume 100, priority PLAYER_LOW (4), no loop,
stream or reverb. The typed local sound service validates the live source and
request; a successful voice auto-releases at clip completion, survives source
removal and is reclaimed on unload. Failure is exposed without inventing audio.
Priority is stored intent; arbitration is still unavailable.

The Pickups inspector shows actual presence, charges, attempted/actual gain,
source return, queued actions, playback count and diagnostics, alongside live,
hidden, inactive and pending pickup resources. Snapshot inspection exposes saved
charges and their keep-alive behavior. Pickup sound remains a request only;
**activation audio is decoded and played**. Original localized console text and
PCX inventory HUD are unavailable, with resource/icon names retained as metadata.

## Preservation and bounded adaptations

An owned optional `CInvMedkit` replaces the original intrusive item-list allocation
for this one supported item. Its checked owner token is never serialized. Typed
InventoryState stores absence versus a byte charge count (including zero); values
above 100 reject before mutation. Staged Load rebinds the item to the destination
owner and commits it with weapons/keys. Both Restore and KeepAlive retain the
medkit, while original keep-alive still discards keys. Transient diagnostics and
queued audio restart, rather than being mistaken for saved game state.

This is not full inventory-list/HUD selection, bomb items, flashlight, timed
bonuses, multiplayer scaling/respawn, retail save-file compatibility or safe
NEW_LEVEL. String-created inventory pickups were initially rejected; the later
[pickup contracts](pickup_contracts.md) supersede this with the original
bouncing SpawnItem and admit MedKitPU in destruction spawning. Missing models retain explicit pickup
failure diagnostics. No ECS, new dependency, reference edit or commercial asset
copy is introduced.

Verification: 730/730 headless tests in Debug and inspector-free Release;
62/62 ASan/UBSan tests with leak detection; four Vulkan GPU checks per configuration.
The private authored medkit fixture repeats CityHub loading and validates real
45,312-frame activation audio. No Windows/macOS/mobile/XR or retail executable
parity is claimed. Detailed logs are linked from the implementation evidence.


The [flashlight continuation](flashlight_contracts.md) adds original item2, L toggle,
active-only charge, local audio, checked child lifetime and forced-inactive
Restore/KeepAlive. One radial point light is rendered over the unlit world/models;
GetPointShade, shadows and general dynamic illumination remain unavailable.
Earlier blanket flashlight omissions are superseded only by this bounded subset.
