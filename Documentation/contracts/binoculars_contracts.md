# Original binoculars and camera zoom

This extends [inventory items](medkit_contracts.md), [the flashlight](flashlight_contracts.md),
[inventory state](inventory_state_contracts.md), [the crosshair](crosshair_contracts.md) and
[player controls](player_control_contracts.md). Sources: `ItemPickups.cpp` BinocularsPU,
`GameInvItems.cpp` CInvBinoculars, `InvItem.h/.cpp`, `InventoryMgr.cpp`, `PlayerObj.cpp/.h`
zoom handling, and the client `CBloodClientShell` SMSG_ZOOMVIEW/INVITEMACTION, UpdateCamera,
PostUpdate and mouse-look branches with `CViewWeapon::Fire`. Retail parity is not claimed.

## Item and server zoom

`BinocularsPU` inherits `ItemPickupCharged`: item ID 5, quantity 1, resource 304,
`Binoculars_pu` model and the shared inventory pickup path. Containers may spawn it
(03_Museum), and string creation bounces like other item leaves. The base
`CInvItem::AddItem` refuses a second pair, so a later pickup stays in the world.

`CInvBinoculars::Activate` ignores the request while the player is already zoomed by
something else; otherwise it sets active, sends the action (type 5, active bit) and calls
`SetZoomMode(TRUE, 98)`. `Deactivate` clears active, sends the action and ends zoom.
`ActivateItem` toggles and reports success after a deactivation, as the source does.
Z is the application binding for the direct item command; the original run-key and
dead/movement/camera gates of `OnCommandItem` apply.

`SetZoomMode` stores the flag and cursor type and sends `ZoomView`. The source sends
every call; a dead player's per-update `ChangeWeaponSlot(1)` would repeat "off" each
frame. Repeated "off" only restores an already restored client crosshair, so consecutive
offs after an off (or before any message) are **coalesced** and counted instead of
filling the bounded client queue. "On" is always sent; send failures are counted and
never revert the server state.

Original weapon interactions are kept, including their quirks:

- `ChangeWeaponSlot` ends zoom for any in-range request from a client inventory, before
  testing the slot. An empty or current slot therefore leaves the binoculars flagged
  active without zoom; the next Z press deactivates them.
- `Update` tracks the current gun type (`m_CurGun`); an actual change toggles active
  binoculars off through `SetActiveItem`.
- Death removes the item (an active pair deactivates first) and resets zoom after SMSG_DEAD.
- Inventory capture keeps an owned `binoculars` presence flag. `InvItem::Load` always
  clears the active flag and player `Load` clears zoom without a message.

## Client

`ZoomView` saves the current crosshair and visibility on "on" and shows cursor type 98
(the source's override); "off" restores them. The type-5 action plays the local
`sounds/powerups/binoculars.wav` and sets `m_bBinocularsActive`; the client weapon then
refuses to fire before its seed draw (`CViewWeapon::Fire`). Server-side firing is not
gated: the source's `HandleFiring` binocular check is commented out.

`UpdateZoom` follows the first-person `UpdateCamera` branch: 8 rad/s toward FOV_ZOOMED
(12.5 degrees) or back to the 90-degree default, clamped. External cameras keep the current
value and letterbox cameras present the default, as in the source. Mouse-look offsets are
quartered while zoomed; keyboard turning is unchanged. World entry resets zoom, crosshair
and FOV.

Presentation multiplies the world, model, sprite, PolyGrid and sky projections by
`1 / tan(fovX / 2)`. The port keeps its aspect-derived vertical FOV instead of the source's
0.8 Y scale, and view models keep their unzoomed projection (replacement policy).
Frustum culling stays at 90 degrees, which contains every narrower view. The HUD atlas grows
to 768x75 so crosshair98 (97x41 installed) fits; PostUpdate reloads the image whenever the
crosshair number changes. The inspector reports item, zoom, FOV and message counters.
The binocular HUD icon, console item-name text and save games remain unavailable.
