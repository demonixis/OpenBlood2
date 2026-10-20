# Menu Credits entry

Date: 2026-10-20. Roadmap task 3.2 (M3). The retail main menu's Credits row now
opens the original `credits` world, and the original CCredits text rolls over it.
In a world the same row shows the original text-only credits screen. M2 closure
and its GPU reservation are unchanged; this does not accept M3.

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

## Text roll (follow-up, same day)

- `game/original/Credits.{h,cpp}` ports CCredit/CCredits: `##` blocks, `>END`,
  `>TIME:` hold override, 12 strings per block, 256 blocks, the fade/hold state
  machine, the red ramp, the speed/pause/advance keys and the endless wrap. The
  [contracts](../../contracts/menu_contracts.md#credits-roll-ccreditsccredit) list
  every rule.
- `read_named_resource` extends the bounded PE reader with named resource types
  and names. The client shell reads `TEXT/CREDITS` from the mounted `cres.dll`
  without executing code. The installed text has 87 blocks; `IDS_CREDITSFONT` is
  `Arial`.
- In the END and CREDITS worlds the roll is inited on the first frame and updated
  twice per frame, as the original GS_PLAYING branch and its post-`RenderCamera`
  call do. Over the world the fade-out stops at 10 % red.
- In a world, the Credits row opens GS_CREDITS: play pauses, the screen is black,
  the roll restarts with screen clearing, and Esc returns to play. Back in the
  credits world the roll starts again from the first block.
- The frontend draws the current block in the 640×480 menu viewport, below the
  screen fade and the HUD. The font is a replacement (Dear ImGui's default font at
  22 virtual pixels for the original GDI Arial 14×22); the warning that the overlay
  was missing is removed.

## Remaining differences

| Area | Status |
| --- | --- |
| Credits font | Replacement: ImGui's embedded font, not GDI Arial. The 14-pixel average width and GDI rasterization are not reproduced. |
| Oversized credit text | The original overflowed its 256/1024-byte buffers; the port refuses such text explicitly. The installed text fits. |
| Add-on and intro texts | `CREDITS_AO`/`INTRO`/`INTRO_AO` are not read; CM_INTRO is ported but unused. |
| Custom-mode guards | Player death, NPC weapon drops and player restoration still refuse non-single modes explicitly. The credits world does not reach them in the tested run. |
| Custom NEW_LEVEL | Refused. Custom levels that chain to another world remain unsupported. |

## Validation

[Evidence](evidence/menu-credits-tests.log), macOS on Apple M5, Dev and
inspector-free Release builds. The first section is the world entry; the second is
the text roll follow-up.

- `menus.state` checks that Credits is enabled and produces `MenuAction::Credits`
  both outside and inside a world.
- `menus.credits_text` (synthetic, headless) checks parsing (`##`, `>END`, NUL,
  `>TIME:` with atof rules, blank lines, the 12-string and 256-block limits, the
  explicit buffer refusals, screen-width font sizes), every state and its timers
  and colors, the 0.1 floor over the world, speed limits, Pause, Return/Space/PgDn
  advance and wrap, CM_INTRO keys, the vertical layout arithmetic, the named PE
  resource reader (case, missing type/name, two languages, non-ASCII names,
  truncation, a bad RVA) and the key routing for the roll and GS_CREDITS.
- `menus.credits` (private data, headless) adds the installed `cres.dll` text (87
  blocks, first/second/last block contents and hold times, `IDS_CREDITSFONT` =
  `Arial`). In the credits world it checks Init on the first frame, two updates per
  frame for 300 frames, the advance to the second block, the clear-screen re-init of
  the in-world screen and the roll restart after Esc. The earlier world, menu and
  refusal checks are unchanged.
- The full headless suites still have the same 17 failures in Dev and Release as
  the base commit; this change adds none.
- `--menu-smoke` passes on Metal in Dev and Release. Its in-world step now presses
  Down three times to pass the enabled Credits row before Quit. The demo traversal
  passes unchanged.

The rendered overlay and the GS_CREDITS screen were not checked visually or driven
through the SDL frontend. The headless tests cover the state, the key routing and
the layout arithmetic; the ImGui drawing itself is untested.
