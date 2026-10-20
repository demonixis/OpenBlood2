# Original menu frontend

User-authorized UI-M0 through UI-M6 are independent of the global roadmap. M2 remains
closed with reservation M2-GPU-001. Graphics baseline: bb98fd7.

## UI-M0 foundation

Selected original Build order and command/resource identifiers live in
`game/original/MainMenus.cpp`; see the [source ledger](../implementation/sources/original_menu_sources.json).
The controller owns page, selection and one pending application command. Rendering
and mouse events never execute game commands. Source Story and Chosen navigation,
medium difficulty selection and eight save slots are retained. Unsupported actions
are visible and disabled; no game-state persistence is implemented.

PCX/FNT/cres.dll remain external. A 640x480 canvas scales uniformly with centered
black bars; the same transform serves drawing and mouse hit testing. A release
activates only the item pressed in that same page, and focus/page changes cancel
pending clicks. F10 input routing is independent of mouse capture; normal F1/F2/F3
no longer control ownership. Integration into the application follows in UI-M1/M2.

Fonts retain authored glyph widths. The inspected mm_font.fnt is 190 bytes, height
31, total glyph advance 1346, against 1345x31 images. Only the final glyph may extend
one pixel beyond the bitmap; clipped copying handles it. All earlier glyphs remain
bounded. This opt-in does not relax HUD loading. CoolFont's implementation is
unavailable: quarter-height spaces are a documented adaptation, not pixel parity.

The renderer owns one texture/transfer pair, uploads only changed composition, and
uses the existing ImGui SDL_GPU transport behind the inspector. No ScenePresenter,
shader, dependency pin or graphical optimization changes are required.

## Validation and current boundary

[Validation record](../implementation/reports/original_menus.md).
Foundation tests cover navigation, commands, pointer cancellation, viewport mapping,
F10/repeat/input isolation, font bounds and all real menu artwork. Application pause,
new-game replacement, profiles, preferences and menu audio are subsequent steps;
this foundation alone does not claim them operational.

## UI-M1/UI-M2 application integration

Normal launch now enters the main menu, without constructing a playable world.
`--view-world` retains direct startup; headless/smoke/benchmark dispatch is unchanged.
F10 opens the inspector only when built/enabled; menus also run without it.
Esc opens the opaque root and returns through parents. Inspector input has priority.
Focus loss cancels pending input and pauses an active game. Menus skip simulation,
level replacement and scene preparation entirely; the application clock still ticks.
The game SDL stream is paused independently of the menu mixer. No zero-delta game
update is used. AudioMixer also refuses cursor advancement while paused.

Menu ambience and select/enter/back cues use original files and an application-owned
mixer; the ambience has independent randomness and survives submenu changes.
Failures are logged/counted; no synthetic successful playback is substituted.

New-game requests execute after event dispatch. A prepared stable-address session
owns destination objects, original globals and resources before promotion. Failure
restores the source binding and globals; success transfers the SDL output and
retires the old owner under its own globals. UI-M2 intentionally uses Caleb/medium
for both paths. The later UI-M3 replaces that temporary selection policy.

`menus.session` checks real source entry, failed replacement retention, successful
replacement, independent menu cursor advancement and paused game output. NVIDIA
normal-launch capture exercises the application frontend; this is not a human
playthrough or native macOS qualification.

## UI-M3 character request and transfer

Story forces Caleb; Chosen transports its selected default profile and difficulty.
The four original tables are centralized in `CharacterProfiles.h`: character order
is Caleb, Ophelia, Ishmael, Gabriella (menu order remains source order). Entry
attributes, model/skin, sound root, standing eye and gender footsteps/landing follow
the selected profile. Existing animation dimensions and atomic crouch resizing are
unchanged. Initial object construction still starts as source-default Caleb before
the original client profile message; animation selection sets the selected body.

ViewWeapon retains resolved owned model/skin names: source uppercase `C_` substitution
changes all hand skins and the melee model, independently for both hands. Renderer
and startup preflight consume the same identities. Player state/engine validation
accepts all four default profiles; restored attributes and stats are never reset.
Existing NEW_LEVEL carries difficulty and captured character. Character death uses
the selected model/voice and existing original corpse/gib rules. Addon/multiplayer
skins and full Action rules remain outside scope; Chosen uses the existing solo game.

## UI-M4 preferences and audio

Audio sliders retain the original MenuSound 0..50 with step 5; defaults follow the
shipped defaults.cfg (SoundVolume 50, MusicVolume 35). SFX gain is volume/50. Music
gain is volume/100, the original SetMusicVolume scale where 100 is full, so the
menu maximum is half of the full synth level (measured installed-soundtrack peaks
0.33..0.89 there, one Cabal cruising transient 1.15 clipped). Mouse sensitivity
retains 100..1500 with step 20. Default 150 maps exactly to the previous 0.0025 radians per SDL relative
unit; inversion affects mouse pitch only. Mouse slider release maps through the
same drawn thumb travel as hit testing. Unsupported controls remain disabled.

Settings are application-owned and applied to both mixers and every replacement
session. Music and SFX gains apply before final clipping. At default gains the old
mixing path is unchanged. Muting keeps voice, instrument and transport clocks
running; pausing freezes them. Queued PCM with obsolete gains is discarded when
changing a setting. Menu ambience is an original interface sound, so follows SFX.

`preferences.cfg` lives under SDL_GetPrefPath("OpenBlood2", "Blood2"). Seven bounded
versioned keys are parsed transactionally; missing files preserve defaults,
malformed files display an error. Writes use a unique sibling temporary followed
by SDL's replacement rename; failed writes retain the previous file. This is atomic
visibility, not a claim of crash-proof fsync durability. Tests use explicit isolated
paths. `--menu-smoke` never reads or writes normal preferences.

## UI-M5/UI-M6 screens and final traversal

Save contains eight source slots; Load adds source Restart and Quickload rows.
[Save/load lot 6](save_load_contracts.md#implemented-slots-quick-and-restart-lot-6) enables
them over real slot files: the rows show the original slot names, a save or load closes the
menu, a refusal keeps the page under the error modal, and death USE opens Load as the top
level. The demo keeps no rows. Quit uses the original
localized prompt with keyboard/mouse Yes/No controls (No selected initially); Y/N
also confirm/cancel. BloodBath and custom levels remain disabled; Credits is covered
by the [Credits entry](#credits-entry) section.

`--menu-smoke` injects explicit SDL events into the real frontend with its own
1/60 clock. It checks original entry, repeated confirmation, menu/inspector
ownership, more than three seconds of unchanged object transforms, update counts,
animation tracks, voice cursors and music clock, live menu audio, options, resize,
resume without catch-up, live simulation under inspection and confirmed quit.
GPU captures include gameplay, audio/mouse options, resize, resume and confirmation.
This is automated Linux/NVIDIA traversal, not manual retail or macOS acceptance.

Fast navigation can overlap feedback sounds. The application mixer caps voices at
16 and replaces the oldest non-looping feedback at capacity, preserving ambience.
This is a bounded portable policy in place of the unavailable retail voice allocator;
resource/decoder/output errors remain explicit failures. The SDL traversal exposed
this case before delivery; the saturation test now checks it directly.

The automated traversal tags its injected keyboard/mouse/focus events and excludes
physical/compositor input noise only in `--menu-smoke`. Normal focus handling is
unchanged and is separately exercised by input tests. Load labels substitute only
the source `%1!s!` string insertion (the Restart and Quickload names); unknown insertion
syntax fails explicitly.

Audio failures use a persistent nonmodal bitmap-font notice and the application
log; they never capture navigation. Application request/preference errors use a
closable modal, with focus events still delivered to input ownership. Ownership
changes clear command levels and USE/item/jump edge latches without simulation;
already committed weapon animation remains ordinary game state.

## Display options (graphics)

Options > Display is now enabled. Source `CMenuDisplay` offered renderer, resolution and
DETAIL LEVEL. SDL owns the first two, so the page keeps the original cycling row
`DETAIL LEVEL [LOW/MEDIUM/HIGH]` (cres.dll 516-518, setting 6). It also adds one
replacement row styled after it, `LIGHTING [PER VERTEX/PER PIXEL]` (setting 7), which has
no original string. Enter, left/right and the mouse cycle both rows, and Back returns to
Options. Because Options now opens on Display, the automated traversal visits it before
Audio and Mouse.

The frontend applies the original `detail*.cfg` meaning each frame:

| Detail | LightMap | MaxModelShadows |
|---|---|---|
| Low | 0 (vertex light) | 0 |
| Medium | 1 | 0 |
| High | 1 | 1 |

`--no-model-shadows` still forces shadows off. Shipped `defaults.cfg` selects medium, but
the default here is high, the original menu's initial value. The lighting row switches
models between per-vertex and per-pixel light. Detail does not yet drive particles,
bullet holes, mipmap offset or other `detail*.cfg` values.

`preferences.cfg` version 2 adds `detail` (0-2) and `pixel_lighting` (0/1). Version 1
files still load with display defaults, and either version rejects keys of the other.
Writes always produce version 2.

The [enhanced renderer](enhanced_renderer_contracts.md) adds `RENDERER [ORIGINAL/ENHANCED]`
(setting 8) after DETAIL LEVEL and `NORMAL MAPS [GENERATED/OFF]` (setting 9, disabled with
the original renderer) after LIGHTING. With ENHANCED, the LIGHTING row cycles
`STANDARD/HIGH` instead of `PER VERTEX/PER PIXEL`; each renderer keeps its own choice.
`preferences.cfg` version 3 adds `renderer`, `quality` and `normal_maps`. Writes produce
version 3, versions 1 and 2 load with the renderer defaults, and keys of a later version
are rejected. This supersedes the version 2 write statement above.


Normal `--demo` now uses the [demo menu and edition provider](demo_menu_contracts.md).
The fixed-offset experiment remains removed. `--demo --view-world` retains the
[menu-free direct route](demo_contracts.md); retail behavior stays shared and
separately tested.

## Credits entry

Outside a world, the retail main menu's Credits row emits `MenuAction::Credits`.
The frontend performs the original `MenuCredits` request outside SDL dispatch:
`StartNewGame("Credits", GAMETYPE_CUSTOM, LOADTYPE_NEW_GAME, DIFFICULTY_MEDIUM)`.
The original does not call `SetCharacterInfo` here, so the character is the one of
the last entered game (Caleb before any game). `menu_world_start` and
`start_menu_world` in `app/session_playback.h` hold this mapping for the frontend
and the headless test.

- `GAMETYPE_CUSTOM` is the original single-player custom-level type. It is accepted
  only for a fresh NEW_GAME request, through request parsing, world preparation and
  player entry. The existing `!= GAMETYPE_SINGLE` branches of ported original code
  now see it as the original did. NEW_LEVEL, saves and multiplayer types still
  refuse explicitly. Guards that refuse non-single modes (player death, NPC weapon
  drops, player restoration) remain explicit failures in this mode.
- The same transactional `start_new_game` is used: a refused request keeps the
  current session, and the error is shown on the menu.
- `CBloodClientShell::m_bRollCredits` follows the original PreLoadWorld test (END or
  CREDITS, with or without the Worlds prefix, case-insensitive), evaluated at player
  entry from the normalized world path and cleared on exit.
- Return to the menu matches the original: the `credits` world has no exit and
  the roll never ends by itself. Esc opens the in-world main menu; Resume returns to
  the world, and Single Player or Quit leave it.
- In a world the Credits row emits the same action and the frontend shows the
  original text-only GS_CREDITS screen: `m_Credits.Term()`, then
  `Init(CM_CREDITS, bClearScreen = TRUE)`. Play pauses (simulation, game audio, mouse
  capture), the screen is cleared to black and the world is not drawn. Esc runs the
  original GS_CREDITS branch: `Term`, back to GS_PLAYING, unpause. Other keys go to
  `CCredits::HandleInput`. Focus loss does not stack the menu over this screen. A
  missing or malformed resource keeps the menu open with the error.
- The demo edition keeps its Prime Gibs promotion unchanged.

### Credits roll (CCredits/CCredit)

`game/original/Credits.{h,cpp}` ports the original classes with owned strings, an
explicit frame time and the static state (speed, pause, hold defaults, font request)
held by the `CCredits` owner in `CBloodClientShell::m_Credits`.

- **Resource.** `CBloodClientShell::InitCredits` reads `cres.dll` from the mounted
  REZ files and `read_named_resource(..., "TEXT", "CREDITS")` returns the raw bytes
  of that named resource. The PE reader in `engine/string_resources.cpp` executes no
  code. Type and name compare ASCII case-insensitively like `FindResource`; the entry
  must have exactly one language, and non-ASCII names, bad offsets, RVAs outside a
  section and payloads over 1 MiB are rejected. The add-on `CREDITS_AO` and the
  `INTRO` texts are not used by this edition.
- **Parsing (`AddCredits`, `CCredit::Init`, `AddString`).** The text is read up to
  the resource size or the first NUL. `##` ends a block, and the newlines after it
  are skipped. A block that starts with `>END` at its `##` stops parsing. The text
  after the last `##` is never added. Each block is split on CR/LF; runs of line
  breaks collapse, so only a leading line break makes an empty string. `>TIME:`
  followed by text sets the hold time with `atof` prefix rules (a value of 0 or less
  keeps the default); `>TIME:` alone is text. At most `MAX_CREDIT_STRINGS` (12)
  strings are kept, and later lines, including `>TIME:` lines, are dropped and
  counted. At most `MAX_CREDITS` (256) blocks are kept; later ones are dropped and
  counted. Text is handled as single bytes (`_mbsinc` under an SBCS code page).
- **Fixed buffers.** The original copied lines into 256-byte and blocks into
  1024-byte stack buffers without checks. The port refuses longer text explicitly
  and keeps an inited, empty roll, as the original did when the resource was missing.
  The installed text has 87 blocks and fits.
- **States.** CS_START → CS_FADEIN 0.5 s (red 0 → 1) → CS_HOLDIN (the block's
  `>TIME`, else 4 s) → CS_FADEOUT 0.4 s (red 1 → 0) → CS_HOLDOUT 2 s (nothing drawn)
  → CS_DONE, then the next block. Each `Update` call runs one state step, and the
  transition out of CS_START and CS_DONE take a call each. The frame time is
  multiplied by the speed and is zero while paused. In CM_CREDITS the last block
  wraps to the first forever; CM_INTRO is ported but unused (it ends after the last
  block and ignores the speed keys).
- **Roll over the world.** In the END and CREDITS worlds (`m_bRollCredits`), the
  first played frame calls `Init(CM_CREDITS, bClearScreen = FALSE)`. Every frame then
  calls `Update` twice: in the GS_PLAYING branch and after `RenderCamera` (the `[blg]`
  call). Only the second call's drawing survives the screen clear, and the timers
  advance twice per frame. The session step reproduces this. In this mode the
  fade-out stops at red 0.1 and moves straight to CS_HOLDOUT. Opening the menu
  freezes the roll; returning from the text-only screen restarts it from the first
  block. Entry and exit `Term` the roll.
- **Keys** (`HandleInput`, repeats included). Up and Down change the speed by 0.2
  within 0.4..5.0, Home resets it to 1, Return/keypad Enter/Space/PgDn jump to the
  next block in CS_HOLDIN, Pause toggles the pause, and Esc only sets the done flag.
  As in the original `OnKeyDown`, the roll receives keys before the menu and the
  game, so the same keys still move the player or the menu selection. Init resets
  speed and pause.
- **Drawing.** `draw_credits` (app/hud.cpp) draws the last drawn block in the
  640×480 menu viewport, below the screen fade and the status bar. The vertical
  arithmetic is the original `Draw`/`DrawString` integer centering around (320, 240)
  with 22-pixel lines and 4-pixel spacing. The color is `SetupColor1(red, 0, 0)`
  with truncation to 8 bits; a zero red channel draws nothing, because black was the
  transparent key over the world and is invisible on the cleared screen.
- **Font (replacement).** The original created a GDI font from `IDS_CREDITSFONT`
  (cres.dll string 206, `Arial` in the installed data, with an `Arial` fallback) at
  14×22 for screens wider than 520 pixels. The port records that request in
  `CCredits::font_name/font_width/font_height`. It draws with Dear ImGui's embedded
  default font at 22 virtual pixels, without the requested 14-pixel average width
  and without GDI's rasterization. No system font is loaded.

See the [implementation report](../implementation/reports/menu_credits.md) and the
[source ledger](../implementation/sources/menu_credits_sources.json).
