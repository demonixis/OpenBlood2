# Menu Credits entry

Date: 2026-10-20. Roadmap task 3.2 (M3). The retail main menu's Credits row now
opens the original `credits` world. M2 closure and its GPU reservation are
unchanged; this does not accept M3.

## What the original does

`MainMenus.cpp` sends `MENU_CMD_CREDITS` to `CBloodClientShell::MenuCredits`.

- Outside a world it calls `StartNewGame("Credits", STARTGAME_NORMAL,
  GAMETYPE_CUSTOM, LOADTYPE_NEW_GAME, DIFFICULTY_MEDIUM)`. It does not call
  `SetCharacterInfo`, so the configured character (the last one chosen, Caleb by
  default) enters the world.
- Inside a world it pauses the game and shows the text-only GS_CREDITS screen.
- `PreLoadWorld` sets `m_bRollCredits` for the END and CREDITS worlds. While
  playing, `CCredits` then rolls text from the cres.dll TEXT resource `CREDITS`
  over the rendered world. It fades each block in and out and loops forever.
- The `credits` world has no exit. Esc in GS_PLAYING opens the in-world main
  menu, as in any other world. `CCredits::ExitToMainMenu` only sets a flag that
  nothing reads in this mode.

## Delivered behavior

- Outside a world, Credits emits `MenuAction::Credits`. The frontend maps it with
  `menu_world_start` to `Credits`, medium difficulty, the current character and
  `GAMETYPE_CUSTOM`, then starts it through the existing transactional
  `start_new_game`.
- `GAMETYPE_CUSTOM` is accepted for a fresh NEW_GAME request in request parsing,
  world preparation and player entry. Ported original branches that test
  `!= GAMETYPE_SINGLE` now behave as the original did for custom levels.
- `CBloodClientShell::m_bRollCredits` reproduces the original END/CREDITS test. It
  is evaluated at player entry from the normalized path and cleared on exit.
- Esc opens the in-world main menu. Resume returns to the credits world. Single
  Player starts a new game, and Quit exits.
- A refused request keeps the current session and shows the error on the menu.
- The demo edition keeps its Prime Gibs promotion.

## Remaining differences

| Area | Status |
| --- | --- |
| CCredits text overlay | Not ported. It needs the cres.dll TEXT resource reader, a font and the CCredit fade/hold timers. The frontend logs a warning when a roll-credits world starts. |
| In-world Credits row | Disabled. The text-only GS_CREDITS screen depends on the same overlay. |
| Custom-mode guards | Player death, NPC weapon drops and player restoration still refuse non-single modes explicitly. The credits world does not reach them in the tested run. |
| Custom NEW_LEVEL | Refused. Custom levels that chain to another world remain unsupported. |

## Validation

[Evidence](evidence/menu-credits-tests.log), macOS on Apple M5, Dev and
inspector-free Release builds.

- `menus.state` checks that Credits is enabled outside a world and produces
  `MenuAction::Credits`, and that it is disabled in a world.
- `menus.credits` (private data, headless) checks the menu command, the request
  mapping, entry into `worlds/credits.dat` with custom type and medium difficulty,
  a clean startup report, the roll flag and five seconds of simulation with a
  living player. It also checks the in-world menu and Resume, that the current
  character is kept, and the explicit refusals: a multiplayer type, a missing world
  and custom NEW_LEVEL syntax.
- The full headless suites have 17 failures in both Dev and Release. The same 17
  fail at the unmodified base commit `2936c87`, so this change adds none.
- `--menu-smoke` passes on Metal. The in-world traversal is unchanged because
  Credits stays disabled in a world.

The interactive Credits selection was not driven through the SDL frontend. The
headless test covers the same mapping and session call.
