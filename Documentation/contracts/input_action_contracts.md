# Input actions (input lot: keyboard, mouse and gamepad)

Play and menu input go through a table of logical actions
(`src/app/input_actions.h`). `MovementControls` keeps device state (keys, mouse
buttons, relative motion, gamepads). It derives the existing game-facing flags from
`InputBindings`: `MovementInput`, `use_down`, item flags, weapon slots and rotation.
Game rules, the original shell, MoveMgr and weapon state machines are unchanged.
This is a replacement application layer: the action list, the gamepad layout and the
gamepad device policy do not come from the retail configuration. The keyboard defaults
for the commands added with the gamepad step come from the original `defaults.cfg`.
The [player control contracts](player_control_contracts.md) still describe the mouse
device policy (capture, sensitivity, focus). **M2/M3 scope is unchanged.**

- Step 1 added the action layer with the previous keyboard/mouse layout.
- Step 2 adds gamepads, weapon cycling and the inventory commands.

## Actions and kinds

Each action has a stable lowercase name (`move_forward`, `fire_primary`, ...) reserved
for a future preference file, and one kind:

- **Hold**: movement, run, jump, crouch and the look keys. Level state; a key held across
  focus loss resumes on its next repeat event, as before.
- **Latch** (`run_latch`): a down edge that keeps Run on until every movement action
  is released. Pressing it with no movement held does nothing.
- **Analog** (`look_horizontal`, `look_vertical`): a signed axis value; keys and
  buttons never drive it.
- **Press**: primary/alternate fire, USE and the item actions (flashlight, medkit,
  binoculars, night goggles, the Eye, proximity/time/remote bombs, detonate). Level state
  that must start with a fresh press while play accepts input:
  - A repeat-only down after focus loss is ignored for any key bound to a Press action.
  - Held input during disabled play, inspector or text editing is refused until it is
    released and then sampled.
  - Releasing the mouse re-arms only the two fire actions from their remaining held
    sources, which preserves the earlier F/G rule.
  - A player change (menu closed, level entry) re-arms every Press action from its held
    gamepad sources. A trigger or button held while the menu closes does not act.
- **Command**: weapon slots 1–10, next/previous weapon, next/previous item and use item.
  Each is queued on a non-repeated down edge while play is enabled and not editing text,
  then consumed once by the frame.
- **Interface**: Menu, MenuUp/Down/Left/Right, MenuAccept, MenuBack, QuickSave and
  QuickLoad. Play never consumes these from the inspector.

## Bindings

A binding is a source and an action. The sources are:

- `Key`: an SDL scancode.
- `MouseButton`: buttons 1–5.
- `MouseWheel`: 0 (away from the user, up) or 1 (toward, down). Each wheel event with
  whole ticks (SDL's accumulated `integer_y`, corrected for natural scrolling) gives one
  down edge. Several ticks in one frame queue one command. The wheel has no held state.
- `GamepadButton`: an SDL gamepad button.
- `GamepadAxisPositive` / `GamepadAxisNegative`: an SDL gamepad axis, used in two ways:
  - Digital actions treat it as held beyond ±0.5.
  - Analog actions read its value (negated for the negative source).

The mapping is many-to-many:

- The held state of an action is the OR of all its sources, across every open gamepad.
- One source may serve both a play action and a menu action: the arrows are look keys in
  play and menu navigation; south (A) is USE in play and accept in menus. The active
  context decides which one applies.
- Mouse buttons and the wheel count only while the mouse is captured: uncaptured clicks
  and scrolling still belong to the UI.

`add` keeps a binding table bounded and checked:

- It refuses unknown or out-of-range codes and out-of-range actions.
- It refuses F1, F2, F3 and F10, which stay inspector/diagnostic shortcuts handled
  before bindings.
- It refuses entries beyond 256.
- A duplicate is accepted and kept once.

`clear` removes every source of an action.

`InputBindings::defaults()` is the keyboard/mouse table plus the gamepad layout below.

### Keyboard and mouse

| Action | Default |
| --- | --- |
| Move / strafe | W S A D (physical; ZQSD on AZERTY) |
| Run / jump / crouch | L/R Shift, Space, L/R Ctrl |
| Turn / look | Left/Right, Up/Down arrows |
| Primary / alternate fire | F or left mouse button / G or right mouse button |
| USE | E |
| Flashlight, medkit, binoculars | L, H, Z |
| Night goggles, the Eye | N, I |
| Proximity, time, remote bomb, detonate | P, T, R, B |
| Weapon slots 1–10 | 1–9, 0 and keypad 1–9, 0 |
| Next / previous weapon | `'` / `;` (original defaults.cfg), mouse wheel down / up |
| Previous / next item, use item | `[` / `]`, Enter (original InventoryLeft/Right, UseInventory) |
| Menu (open/back, release mouse) | Esc |
| Menu navigation / accept | arrows / Enter, keypad Enter |
| Quick save / load | F5 / F6 |

Enter keeps its menu meaning (accept, demo death retry). In play it is now also the
original UseInventory key, so it is consumed by play like any other play key.

### Gamepad (console layout)

| Action | Default |
| --- | --- |
| Move / strafe | left stick |
| Look | right stick (analog) |
| Run | left stick press: latches run until the stick returns to rest |
| Crouch | right stick press (not in the requested list; needed for crawl spaces) |
| Primary / alternate fire | right trigger / left trigger |
| Next / previous weapon | right bumper / left bumper |
| Jump | north (Y) |
| USE and menu accept | south (A) |
| Menu back | east (B) |
| Use item | west (X) |
| Next item | Back (Select) |
| Detonate remote bombs | D-pad up (in play; it still navigates menus) |
| Menu (open/resume) | Start |
| Menu navigation | D-pad or left stick |

The gamepad device policy is a set of replacement constants in
`movement_controls.h`:

- Digital axis threshold: 0.5.
- Analog look:
  - It reads zero within a 0.2 deadzone. Past it, the value is rescaled to [0,1] and
    squared.
  - At full deflection it turns 3.5 rad/s in yaw and 2.5 rad/s in pitch.
  - It shares the mouse's vertical inversion, and is quartered while zoomed, like mouse
    offsets.
- At most eight gamepads are tracked.

The Eye, flashlight and the other item hotkeys have no dedicated gamepad button. Items
are reached through Next item and Use item. The wheel bindings are replacement defaults:
the original configuration binds no wheel.

## Gamepad lifetime and state

`GamepadDevices` (`src/app/gamepad_devices.h`) opens every gamepad SDL reports,
including those present at startup, and closes them on removal and destruction. It is
used by both the menu frontend and the `--view-world` diagnostic loop.
`MovementControls::track` updates the state from button and axis events and returns
the digital edges of the event: an axis crossing zero leaves one direction and enters
the other. The state follows these rules:

- **Focus loss** drops all gamepad state, and events are ignored while unfocused,
  because background events may be lost. When focus returns, `GamepadDevices::resync`
  polls the real state and re-arms Press actions.
- **Removal** releases that gamepad's sources, even while unfocused.
- **While a menu or the inspector is open**, gamepad state keeps updating. A stick still
  held when the menu closes moves the player at once; a held Press source needs a fresh
  press.

## Menu, demo and frontend routing

`MenuInput` keeps its original ordering and early returns. It reads input edges, so
keys, mouse buttons and gamepad buttons/axes share the same action checks:

- Promotions are dismissed by Menu, MenuAccept, MenuBack or the original Space key.
- The credit roll keeps its original VK translation of Up/Down/Home/Enter/Space/
  PageDown/Pause/Esc, because `CCredits::HandleInput` consumes those codes. Gamepads
  reach it through the menu actions:
  - MenuAccept sends Enter;
  - Menu or MenuBack sends Esc, or leaves the in-world credits screen;
  - MenuUp/MenuDown send Up/Down.
- The Quit page keeps its original Y/N keys.
- Menu opens the menu or goes back one page (Start and Esc). MenuBack (B) only goes back
  while the menu is open; on the top in-world page that resumes play, like Esc.
- Navigation actions honor key repeats. A gamepad moves one step per button press or
  per stick threshold crossing; there is no gamepad auto-repeat. MenuAccept needs a
  fresh press.
- The demo death retry uses MenuAccept.
- QuickSave and QuickLoad replace F5 and F6. Their key-ups are still swallowed.

`DemoFlow` uses the same edges for Menu and MenuAccept. The normal frontend's error
screen is dismissed by Menu or MenuBack. The `--view-world` diagnostic loop reads the
Menu binding for its "Esc releases the mouse first" check; its Esc quit stays a literal
inspection shortcut. The menu frontend samples the time-bomb action as well (step 1
fixed T, previously wired only in `--view-world`).

## Weapon cycling

Next/previous weapon call `GameSession::cycle_weapon`. It applies the `command_weapon`
guard (the original OnCommandOn dead and movement-blocked return), then
`CInventoryMgr::CycleWeaponSlot`:

- The search follows the original SelectNextWeapon/SelectPrevWeapon: from the current
  slot over slots 0–9, wrapping, skipping empty slots.
- No other carried weapon is the original CHWEAP_CURRENT: nothing happens.
- When an inventory bomb (slots 10–12) is current, the source resets the slot to 0
  before searching. If the search lands back on slot 0, the source silently keeps slot 0
  without a weapon change. Here that case performs a real change to slot 0, so the hand
  models and the client follow.

The change itself goes through the existing `select_weapon`: ChangeWeaponSlot and
client SyncWeapons, with rollback on failure.

## Verification and remaining work

The headless `input_actions.*` tests cover:

- the table and binding validation;
- the default keyboard, mouse and gamepad layouts, and the consumed keys;
- remapping of Hold, Press, Command and look actions, and of mouse buttons;
- mouse wheel ticks: capture-only, direction and natural scrolling, several ticks in one
  event, other windows and text editing;
- the Press repeat guard and fire re-arming;
- gamepad digital thresholds, diagonals, the run latch, triggers with text-editing
  suppression, command edges, analog look (deadzone, curve, zoom, inversion), several
  gamepads, removal, the eight-gamepad bound, focus loss and resync;
- the re-arming of held buttons on player change;
- gamepad menu navigation, accept and back, resume, the credits screen, and menu-time
  state.

`hud.session` checks weapon cycling and its guard on a fixture player. All existing
control, menu, demo and HUD tests keep passing.

The inventory commands (next/previous item, use item) drive the original item cursor and
its status-bar tabs: see the [inventory cursor contracts](inventory_cursor_contracts.md).

Not included yet:

- Touch camera drag.
- VR controllers (they will reuse the gamepad layout).
- Gamepad auto-repeat in menus.
- A rebinding screen and persistence in preferences.
