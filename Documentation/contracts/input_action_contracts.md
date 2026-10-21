# Input actions (input lot, step 1: keyboard and mouse)

Play and menu input now go through a table of logical actions
(`src/app/input_actions.h`). `MovementControls` keeps device state (keys, mouse
buttons, relative motion). It derives the existing game-facing flags from
`InputBindings`: `MovementInput`, `use_down`, item flags, weapon slots and rotation.
Game rules, the original shell, MoveMgr and weapon state machines are unchanged.
This step is a replacement application layer: neither the bindings nor the action list
come from the retail configuration. The [player control contracts](player_control_contracts.md)
still describe the device policy (capture, sensitivity, focus). **M2/M3 scope is unchanged.**

## Actions and kinds

Each action has a stable lowercase name (`move_forward`, `fire_primary`, ...) reserved
for a future preference file, and one kind:

- **Hold**: movement, run, jump, crouch and the look keys. Level state; a key held across
  focus loss resumes on its next repeat event, as before.
- **Press**: primary/alternate fire, USE and the item actions (flashlight, medkit,
  binoculars, night goggles, the Eye, proximity/time/remote bombs, detonate). Level state
  that must start with a fresh press while play accepts input:
  - A repeat-only down after focus loss is ignored for any key bound to a Press action.
  - Held input during disabled play, inspector or text editing is refused until it is
    released and then sampled.
  - Releasing the mouse re-arms only the two fire actions from their remaining held
    sources, which preserves the earlier F/G rule.
- **Slot**: weapon slots 1–10, queued on a non-repeated down edge while enabled and not
  editing text.
- **Interface**: Menu, MenuUp/Down/Left/Right, MenuAccept, MenuBack, QuickSave and
  QuickLoad. Play never consumes these from the inspector.

## Bindings

A binding is a source (`Key` by SDL scancode, or `MouseButton` 1–5) and an action.
The mapping is many-to-many:

- The held state of an action is the OR of all its sources.
- One key may serve both a play action and a menu action (the arrows are look keys in
  play and navigation in the menu); the active context decides which one applies.
- Mouse buttons count only while the mouse is captured: uncaptured clicks still belong
  to the UI.

`add` keeps a binding table bounded and checked:

- It refuses unknown or out-of-range scancodes and buttons, and out-of-range actions.
- It refuses F1, F2, F3 and F10, which stay inspector/diagnostic shortcuts handled
  before bindings.
- It refuses entries beyond 256.
- A duplicate is accepted and kept once.

`clear` removes every source of an action.

The default `keyboard_mouse()` table reproduces the earlier fixed layout exactly:

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
| Menu (open/back, release mouse) | Esc |
| Menu navigation / accept | arrows / Enter, keypad Enter |
| Menu back | none (Esc already returns; reserved for the gamepad step) |
| Quick save / load | F5 / F6 |

## Menu, demo and frontend routing

`MenuInput` keeps its original ordering and early returns:

- Promotions are dismissed by Menu, MenuAccept, MenuBack or the original Space key.
- The credit roll keeps its original VK translation of Up/Down/Home/Enter/Space/
  PageDown/Pause/Esc, because `CCredits::HandleInput` consumes those codes.
- The Quit page keeps its original Y/N keys.
- Menu opens the menu or goes back one page; MenuBack only goes back while the menu is
  open.
- Navigation actions honor key repeats; MenuAccept needs a fresh press.
- The demo death retry uses MenuAccept.
- QuickSave and QuickLoad replace F5 and F6. Their key-ups are still swallowed.

`DemoFlow` uses Menu and MenuAccept the same way. The normal frontend's error screen is
dismissed by Menu or MenuBack. The `--view-world` diagnostic loop reads the Menu binding
for its "Esc releases the mouse first" check; its Esc quit stays a literal inspection
shortcut.

The normal frontend now also samples the time-bomb action. Before this step, T worked
only in the `--view-world` loop, an omission in the menu frontend.

## Verification and remaining work

The headless `input_actions.*` tests cover:

- the table and binding validation;
- the default layout (bindings, consumed keys, slots);
- remapping of Hold, Press, Slot and look actions, and of mouse buttons;
- the Press repeat guard and fire re-arming on mouse release;
- the Menu binding releasing the mouse;
- action-driven menu navigation, back and quick save/load.

All existing control, menu, demo and HUD tests keep passing unchanged.

Not included yet:

- Gamepad: the console-style default layout, analog axes, hot-plug, menu navigation and
  inventory cycling/use, which needs the original item selection.
- Touch camera drag.
- VR controllers.
- A rebinding screen and persistence in preferences.
