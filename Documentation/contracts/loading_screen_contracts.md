# Loading screen and title splash

The original client draws a loading screen around every world load and a title
splash at startup. `game/original/LoadingScreen.{h,cpp}` ports `GetWorldTitle`,
`DrawLoadingScreen` and `LoadScreenData.cpp` from `ClientShellDLL/BloodClientShell.cpp`;
`app/loading_flow.h` and `app/frontend.cpp` port the `StartNewWorld`, `PreLoadWorld`,
`PostUpdate`, `InitSinglePlayer` and `OnKeyDown` (`GS_WAITING`) branches around it.

## World title and load screen

`world_load_info` parses the DAT world-info string with the engine's `Parse` rules
(`world_info_commands`, shared with saved-game titles): the last `worldname` wins,
`loadscreen` selects `Interface\LoadScreens\<name>` and `loadscreenID` is `atoi`'d.
Without a title the name given to `StartNewWorld` is used. Under `_DEMO` every
`loadscreen` becomes `\screens\loading2.pcx` when the uppercased `Worlds\<name>.dat`
contains `DEMO_02`, else `\screens\loading1.pcx`. The header is read without
loading the world (`read_world_info` over a bounded prefix, the original
`GetWorldInfoString`); a missing world keeps the fallback title and the load
itself reports the failure. In the [Nightmares edition](nightmares_edition_contracts.md#lot-n2-frontend-and-world-paths)
a `WORLDS_AO` name is read as given and always shows `Screens_ao\Bumper.pcx`.

## Drawing

The screen is the 640x480 reference canvas (the menus' letterboxed presentation),
so the table's 640x480 coordinates apply unscaled. The background is the named
screen, or `LoadScreen.pcx` with `LoadingText\Loading_01.pcx` at (215, 321),
stretched to the screen. Fonts are `stat_font_1` (title, bottom line) and
`obj_font_1` (bumper text) with the menus' quarter-height space adapter.

- Load screen ID 0 (retail) and the demo: the world title centred at y = 2h.
- Retail IDs 1-34 (`g_LoadScreenData`, identical for every level): the title and
  `IDS_BUMPER_TITLE_n` (2000 + n - 1) centred at x = 350, y = 2h and 3h; unless the
  game type is `GAMETYPE_ACTION`, `IDS_BUMPER_TEXT_n` (3000 + n - 1) left-justified at
  x = 200, y = 5h, wrapped at 300 pixels. The bottom line, centred at
  y = 480 - h, is a random `IDS_LOADING_MESSAGE_1..20` while loading and the pulsing
  `Press a key to continue...` while waiting (the source's one-second red/tan
  alternation, `DrawSolid`). `esc01.wav` plays once per wait (through the menu mixer).
- Original strings come from `cres.dll` as data. IDs above 34 or missing original strings fail.

The language catalogs translate the 34 retail briefings by original resource ID.
Translated titles and bodies reuse the mounted bitmap glyphs where available and
fill gaps with the bundled Unicode font. The original artwork, text positions,
line breaks, indents and 300-pixel body width are preserved. Missing translations
or unavailable glyphs use the original English text. Replacement `cres.dll`
resources from a mod retain precedence.

The selected language also applies to the demo level and chapter lettering, the
loading plate and the stock Nightmares titles. The English version uses the
original PCX pixels. Translated labels are confined to the original text areas.

CoolFont is unavailable, so `HudFont::draw_format` and `draw_solid` are adapters:
words wrap before the width, `|` breaks a line and `~` indents by four spaces.
This source revision's `_DEMO` branch never blits its surface; the demo instead
shows its own `loading1`/`loading2` screens with the title, as retail ID 0 does.
That is an explicit adapter, not a demo-executable trace.

## Frontend flow

Only normal menu launches use it; `--view-world`, the menu-free demo route
(`--demo --demo-smoke`, `--demo --view-world`), the demo's Enter retry and
inspector levels keep their immediate loads.

1. A New Game or Credits command, a Load row (slots, Restart), F6 or a pending
   `NEW_LEVEL` begins the screen (`GS_LOADINGLEVEL`). Play pauses, the menu and its
   ambience are hidden, and the screen is presented for one frame (`PreLoadWorld`).
2. The next frame performs the unchanged synchronous load at the frame boundary.
   Saved games use the slot's recorded world; unreadable or empty slots load (and
   refuse) without a screen. A failure returns to the previous state with the usual
   modal; a rejected `NEW_LEVEL` keeps the world.
3. A world with a load-screen ID enters `GS_WAITING` (`PauseGame(TRUE)`): the world
   stays paused (game audio included) while the presenter prepares it behind the
   screen. Any fresh key continues; mouse buttons, gamepad buttons, taps and the XR
   controllers' virtual pad also continue (non-keyboard adapter). Focus loss does
   not open the menu over it. Without an ID play resumes at once.

The demo's two worlds carry IDs 7 and 8, so the demo waits too (without a prompt,
as its branch draws none). A failed screen (missing art or strings) is logged and
the load proceeds without it.

## Startup movies and title splash

On a normal retail launch, `StartMovies` plays the loose `Monolith.smk`, `Gti.smk`
and `Intro.smk` in that order. Look in the selected external data directory first,
then its `Movies` subdirectory, matching those names without regard to case.
Missing or unreadable clips are skipped with a diagnostic, so installations with
only the introduction can play it. The demo retains the original movie skip.
Nightmares plays the logos and proceeds to its own title.

Smacker playback preserves the source frames, palette, frame intervals and
soundtrack. The frame fits the display with black margins where needed. Only
`Intro.smk` has timed captions. The selected language uses the `[intro]` catalog,
with the English cue as fallback. Options > Audio > Subtitles also controls
these captions. Esc advances the current movie and consumes
the event. Direct world entry, networking and diagnostic menu traversal skip
the movies. Switching installations through Setup in-process does not replay
them.

On normal menu launches the original `Splash_Display("\screens\title.pcx")` (Nightmares:
`\screens_ao\title.pcx`) is shown,
stretched to the screen, for six seconds or until Esc, Return or Space (mouse,
gamepad and tap adapters as above), then the main menu opens. Direct entry
(`runworld`, here `--view-world`) and diagnostic menu traversal skip the splash.
A missing title image is logged and skipped.

The `loading_screen.*` tests cover parsing, formatting and retail/demo
composition. The menu traversal smokes exercise the loading screen, wait and
continue. The private `gpu.startup_retail` and `gpu.startup_demo` tests exercise
startup and the handoff to the main menu using `--startup-smoke`.
