# Inventory cursor and item tabs

This continuation of the [input action lot](input_action_contracts.md) ports the
original inventory item cursor and its status-bar display, so the next/previous item and
use-item commands work from the keyboard (`[`, `]`, Enter) and the gamepad (Back and X).
The sources are:

- `InventoryMgr.cpp`: SelectNextItem, SelectPrevItem, SelectItem, GetNextItem,
  GetPrevItem, GetItemCharges, UpdateClient, RemoveItem and the Save/Load current-item
  DWord.
- `PlayerObj.cpp`: OnCommandOn COMMAND_NEXTINVENTORY, COMMAND_PREVINVENTORY and
  COMMAND_USEINVENTORY.
- `BloodClientShell.cpp`: SMSG_ITEMCHANGED.
- `NewStatusBar.cpp`: InitInvTabs, AdjustRes, UpdateInv, DrawInvTabs and the
  current-item part of DrawStatTabs.

**M2/M3 scope is unchanged.**

## Cursor

`CInventoryMgr::current_item` is the original `m_pCurItem`. The original walks an
intrusive list in pickup order. The port keeps typed items, so the cursor walks them in
the order the [inventory save](save_load_contracts.md) already uses:

1. medkit;
2. flashlight;
3. night goggles;
4. the Eye;
5. binoculars;
6. keys, in pickup order;
7. proximity, remote and time bombs.

The rules are:

- **Carried items.** An item is carried only with a nonzero `GetItemCharges`: the item
  count, 1 for the Eye and binoculars, the key's uses, or the bomb's ammunition clamped
  to 255. This matches the original Update, which removes zero-charge items. A key is
  identified by its case-insensitive name.
- **Next/previous.** SelectNextItem takes the following item, or the first one when
  there is none or no cursor. SelectPrevItem takes the preceding item, or the last one.
  Both wrap, and both set `show_items` (`m_bShowItems`) when they select an item.
- **Displayed neighbours** (GetNextItem/GetPrevItem) never wrap past the list head. On
  the first item there is no previous, and on the last there is no next. Without a cursor
  they are the first item (next) and the last (previous).
- **Picking up items.** The original AddItem/AddKey leave the cursor on the list head
  (no item) in every reachable case, so picking up an item selects nothing until the
  player cycles.
- **Removal.** In the original, RemoveItem on the current item selects the next item,
  wrapping to the first, or nothing if it was the only one. Removals here are typed
  resets, consumed keys and zero charges, so `ReconcileItemCursor` applies that rule
  against the list it last checked: the next surviving item, in the old order. When
  several items disappear at once (death), it approximates the sequential source
  removals.
- **Save/Load.** Both write and read the original current-item DWord, the item type.
  Load clears the cursor, then SelectItem selects the first carried item of that type;
  unknown types (above 9) select nothing. Saves that predate this continuation wrote
  `INV_NONE` and load with no cursor. Keep-alive transfers drop keys, so a saved key
  cursor selects nothing. The typed `InventoryState::current_item` is validated (0..9)
  like the other fields.

## Commands

The client queues one request per press (`next_item_requested`, `prev_item_requested`,
`use_item_requested`). `UpdateUse` sends them after the item hotkeys:

- **`PlayerItemCursor`** runs CPlayerObj::OnCommandItemCursor. It applies the
  OnCommandOn guard (dead, movement or camera blocked), then selects the next or previous
  item.
- **`PlayerUseInventory`** runs CPlayerObj::OnCommandUseInventory: SetActiveItem on the
  current item's type.
  - Unlike the item hotkeys (COMMAND_INVITEM), it has no run-key exclusion.
  - An active Eye lets the command through a movement block, which then uses the current
    item, whatever it is.
  - Keys have no activation, as SetActiveItem(INV_KEY) does nothing.

## SMSG_ITEMCHANGED

`CPlayerObj::UpdateInventoryClient` runs every player update after UpdateStats. It is
forced on client initialization and invalidated on restore. It sends a typed
`InventoryItems` value when it differs from the last one sent, or when a show request is
pending. The value holds the current, previous and next items: type, original picture
names, charges, and either the cres.dll `IDS_ITEM_*` name resource (301–308) or a key's
authored display name. The show flag is the original `ITEMF_SHOWITEMS`.
`SendToClient` validates:

- types 1–9;
- relative picture paths (no root, drive or `..`);
- bounded strings;
- name resources only for non-key items.

The client keeps the values, counts messages and show requests, and never stores the
show flag itself.

The original sends the item name from the server's FormatString. When that is NULL, the
current icons are not sent. Here the HUD resolves the name from cres.dll (read once). A
missing string leaves the name empty but keeps the icons.

## Status bar

`CNewStatusBar` gains the original inventory tabs, laid out as in AdjustRes:

- Three 75×50 tabs: `border_inv.pcx`, picture at (50,18) centered, charges at (50,29),
  using `stat_font_1`. The current tab sits at the left edge, vertically centered; the
  previous and next tabs sit 15 pixels further left, above and below it.
- The 50×45 current-item tab: `border_current.pcx` above the health tab at x = 20.
- The 100×14 item-name surface.

Pictures load from `interface/statusbar/items/`; current-item tabs use the highlighted
picture. Missing art does not fail the HUD:

- A missing picture leaves that tab empty, as the original's NULL surface did, and
  records `item_error`.
- A missing font or border disables only the item tabs.
- The health, armor and ammo tabs keep their strict resource policy.

`UpdateInv` runs once per SMSG_ITEMCHANGED. Its state machine follows the original:

- a show request makes the hidden tabs scroll in;
- any message while stopped restarts the 5-second display;
- any message while scrolling out scrolls the tabs back in.

`DrawInvTabs` advances it at draw time:

- The tabs slide in or out over 0.15 s, by `(loc + size) * ratio - size`.
- They then display for 5 s.
- Time is the session clock, so the slide pauses with the game.

What is drawn depends on the detail level:

- **High detail** (512×384 or more) draws the sliding tabs and the item name beside the
  current tab.
- **Low detail** hides the sliding tabs and draws the name beside the current-item tab.
- The current-item tab is drawn with the stat tabs whenever an item is selected.

The renderer's atlas grows to 1024×192. Its second row holds the five item surfaces.
Text spaces in `stat_font_1` use the menus' quarter-height adapter policy, because
CoolFont's implementation is unavailable. Number fonts keep no space advance.

## Verification

- `hud.inventory` (headless fixture player) covers:
  - the order, charges and zero-charge absence;
  - cycling and wrapping, display neighbours and show requests;
  - key identity, and removal handing the cursor on (including the wrap);
  - the typed save type;
  - client queueing, message delivery and change suppression;
  - USEINVENTORY on binoculars, and the guards.
- `hud.private` (retail data) covers:
  - the real art and font;
  - the tab positions, the slide ratio, the display restart and scroll-back;
  - the low-detail name placement;
  - the drawn name pixels and missing-picture tolerance.
- Input routing is covered by the [input action tests](input_action_contracts.md).

Not verified: GPU pixels of the new atlas row (the existing `gpu.hud` capture does not
select an item), and retail pickup-order cycling.
