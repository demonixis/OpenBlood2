# Input actions (input lot: keyboard, mouse, gamepad and touch)

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
- Step 3 adds the touch camera drag.
- Step 4 adds the original Controls page for rebinding, `controls.cfg` and the on-screen
  touch controls.

## Actions and kinds

Each action has a stable lowercase name (`move_forward`, `fire_primary`, ...), used by
`controls.cfg`, and one kind:

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

## Touch camera

Touch screens drag the camera (`MovementControls`, replacement policy):

- The first finger that lands on the right half of the window (x ≥ 0.5) owns the drag,
  while play is focused, enabled and not editing text. Other fingers are ignored until
  it lifts or is cancelled; its travel before release is still applied.
- Travel accumulates in window units and is consumed once by `look()`:
  - 3 rad of yaw per window width and 1.5 rad of pitch per window height;
  - shared vertical inversion, quartered while zoomed;
  - no mouse capture needed.
- Only direct touch screens count. A macOS trackpad's indirect touches and SDL's
  mouse-emulated touches are ignored.
- SDL's touch-emulated mouse events never look or fire in play; menus still receive them
  as pointer input.
- Focus loss, player changes and disabled or text-editing frames drop the drag.

## On-screen touch controls

`TouchControls` (`src/app/touch_controls.h`) is a replacement virtual gamepad for direct touch
screens; the original has no counterpart. Its fingers become ordinary gamepad events with the
fixed joystick ID `touch_gamepad_id`, which then take the normal gamepad path. The console layout,
its rebinding, the Press guard and the run latch therefore apply unchanged.

| Control | Gamepad source |
| --- | --- |
| Floating stick (a finger landing in the left half, away from a button) | left stick, 0.12 window heights for full deflection |
| FIRE / ALT | right / left trigger |
| JUMP, DUCK, USE | north (Y), right stick press, south (A) |
| WEAPON, ITEM, NEXT | right bumper, west (X), Back |
| RUN | left stick press (the run latch) |
| MENU | Start |

- Positions and radii are in window-height units, anchored to the left or right edge and the
  bottom; a finger within 1.25 radii presses a control.
- A finger that lands on a control holds it until it lifts or is cancelled, even if it slides
  off. The stick finger moves the stick from where it landed and returns it to rest on release.
  A second left-half finger is swallowed. Fingers on the right half away from the controls keep
  the camera drag above.
- Fingers start controls only while play accepts input: a player in the world, focused, enabled,
  no menu, credits screen or inspector. In menus, touches stay SDL's emulated mouse pointer.
- Ownership changes (menu, inspector, focus loss, player change) forget every finger and drop the
  virtual gamepad state without edges, so held sources need a fresh press.
- The controls are drawn (outlined circles, filled while held, the stick at its finger) from the
  first direct touch until a key, a real mouse click or a real gamepad button is used. Trackpads,
  other windows and mouse-emulated touches never press them.

## Rebinding: Options > Controls

The original `CMenuControls` page is ported over the action layer; see the
[menu contracts](menu_contracts.md#controls-cmenucontrols) for the page itself.
`src/app/control_bindings.h` holds the row table and the binding rules.

- **Rows.** The 42 original actions, in source order, with their cres.dll names: Weapon_0 is
  slot 1 (key 1 in the installed defaults.cfg), Weapon_9 slot 10. Strafe, RunLock (the
  always-run toggle), TurnAround, DropWeapon, Grab, Message, Taunt, MouseAimToggle and
  Crosshair have no port and are disabled. Replacement rows follow for actions the original
  menu did not list: Run Latch, Flashlight, Medkit, Binoculars, Night Goggles, The Eye, then
  Restore Defaults.
- **Rebindable actions** are the row actions. Menu, menu navigation, quick save/load and the
  analog look axes keep their bindings.
- **Assigning** (original `HandleDeviceTrack`). Enter on a row shows "=" and waits for the next
  fresh down edge: a key, a mouse button (uncaptured too), a wheel tick, a gamepad button or a
  gamepad axis crossing ±0.5. The control first leaves every other rebindable action, then the
  row's action loses its control of the same device (keyboard; mouse buttons and wheel; gamepad),
  then the control is bound. So each device keeps one control per action, as in the original.
  A control may still serve a menu action: Enter bound to Jump stays the menu accept key.
- **Not bound.** Any Menu source (Esc, Start) cancels the tracking. Tab (original), F5/F6
  (quick save/load) and the reserved F1/F2/F3 are ignored and tracking continues; F10 toggles the
  inspector, which ends tracking like every ownership change. Key repeats and touch-emulated
  clicks never bind.
- **Unassigning.** Delete (original) or Backspace (the Mac delete key) removes every control of
  the selected row.
- **Restore Defaults** returns every rebindable action to `InputBindings::defaults()`.
- **Names.** Keys use SDL's name for the current keyboard layout (the original
  ConvertKeyToCurrentKeyboard), uppercased; a non-ASCII name falls back to the scancode name.
  Mouse buttons keep the original one-based DirectInput numbering (`MBUTTON 1` left, `2` right,
  `3` middle, `4`/`5` side); the wheel is `MWHEEL UP/DOWN`. Gamepad controls are `PAD` plus the
  console-layout name (`PAD A`, `PAD RB`, `PAD LS UP`, `PAD RT`) instead of the original `J` plus
  a DirectInput name. Devices are joined with " or ", keyboard first.

### controls.cfg

The bindings persist in `controls.cfg`, beside `preferences.cfg` under
`SDL_GetPrefPath("OpenBlood2", "Blood2")`, and replace the original console `rangebind` lines:

```
version=1
bind=jump key 44
bind=fire_primary mouse 1
```

- One `bind=<action> <source> <code>` line per binding of a rebindable action. Sources are
  `key` (SDL scancode), `mouse`, `button`, `wheel`, `axis+` and `axis-`; codes are the SDL values
  of the binding table. `#` lines and CR line ends are accepted.
- Loading starts from the defaults and replaces every rebindable action with the file's lines,
  so an action without a line is unassigned. Unknown versions, actions, sources or codes,
  non-rebindable actions, refused (reserved or invalid) bindings, a missing version line and
  files over 16 KiB are refused: the bindings stay unchanged and the menu shows the error.
- The frontend writes the file atomically whenever the bindings change. Smoke runs never read
  or write it. A missing file means the defaults.
- The `--view-world` frontend and the demo use the same bindings; the headless diagnostic loop in
  `main.cpp` keeps the defaults.

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

## Fire and dual weapons

Fire from the keyboard, mouse or gamepad reaches the view weapons with `all_hands`. This
is the original HandleFiring rule: both carried hands fire together, and with two weapons
the alternate trigger fires primary. See the [weapon contracts](weapon_contracts.md).

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
- the touch drag: right half only, one owning finger, consumption, zoom, release,
  indirect/emulated/foreign-window touches, touch-emulated mouse events and the drop rules;
- rebinding through the real menu path (`rebind`): tracking, same-device replacement, a
  control leaving its other action, Enter keeping menu accept, Esc/Start cancel, ignored Tab,
  F5/F6 and reserved keys, mouse and gamepad sources, touch clicks, Delete/Backspace, focus loss,
  Restore Defaults and play after a rebind;
- `controls.cfg` (`controls_file`): round trip, unassigned actions, every refusal leaving the
  bindings unchanged, and comments/CR;
- the on-screen controls (`touch_controls`): trigger and button sources, sliding off, the
  floating stick and its threshold, the run latch, the swallowed second stick finger, the camera
  drag beside them, rebinding applying to them, MENU releasing everything, menus, other devices
  hiding them, trackpads, other windows, disabled play and focus loss;
- gamepad menu navigation, accept and back, resume, the credits screen, and menu-time
  state.

`hud.session` checks weapon cycling and its guard on a fixture player. `menus.state` checks the
Controls page model, `menus.artwork` every original action string and the scrolled list with the
real artwork, and `gpu.touch_controls` the drawn overlay. All existing control, menu, demo and HUD
tests keep passing. No touch device was available: the on-screen controls are verified with
synthetic SDL finger events and a GPU capture, not on a phone or tablet.

The inventory commands (next/previous item, use item) drive the original item cursor and
its status-bar tabs: see the [inventory cursor contracts](inventory_cursor_contracts.md).

Not included yet:

- VR controllers (they will reuse the gamepad layout).
- Gamepad auto-repeat in menus.
- The original Keyboard (turn speed) and Joystick (axis configuration) pages, which stay
  disabled.
- Rebinding the menu and system keys, and a customizable on-screen layout.
