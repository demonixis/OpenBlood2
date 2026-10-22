# Original demo menu with edition resources

The separately authorized menu implementation follows the [audit recommendation](../implementation/reports/demo_menu_audit.md):
limited original menu branches and a finite edition resource provider. It replaces
the normal menu-free `--demo` launch, while preserving that diagnostic path.
See [validation](../implementation/reports/demo_menu.md),
[source ledger](../implementation/sources/demo_menu_sources.json),
[shared menu contracts](menu_contracts.md) and [demo gameplay/retry](demo_contracts.md).
M2 closure, M2-GPU-001 and the graphics stack are unchanged.

## Startup and resource boundary

`--demo --data-dir DIR` mounts only the external `B2Demo.rez` and opens the main
menu without loading a world. Single Player > Caleb > difficulty starts Demo_01.
`--demo --view-world [--world Demo_02]` preserves direct entry, simple Esc pause,
and the phase-1 end image followed by exit. Other inspection/start/benchmark
diagnostics keep their existing dispatch. A world argument alone does not select
the menu's New Game destination; use `--view-world` for direct selection.

`MenuEdition` is fixed for each controller. `MenuResources` supplies selected title
paths, fonts, background, sliders, title/list vertical placement, ambience/cues
and promotion images. `MenuCanvas` rejects mismatched controller/resource editions.
Shared readers, texture transport, hit testing, preferences and audio services
remain in use; there is no new frontend, renderer, DLL ABI or global string fallback.

Retail reads its external `cres.dll` through the existing bounded PE reader.
Demo uses 27 reviewed English semantic labels in `menu_resources.cpp`, based on
`ClientRes.rc` with Build 108 wording/casing overrides. This is source text packaged
with the source adaptation, not an offset-based commercial DLL extractor. Demo
menu initialization reads neither `cshell.dll` nor `cres.dll`. The manifest is
menu-only: objective/gameplay localization still uses its own required resources.
Other languages/build-specific translations need an explicitly reviewed provider.

Manifest construction is transactional. Unknown/duplicate IDs and missing, empty,
non-printable/non-ASCII or over-256-byte required labels fail. The common required
set is checked for both editions; retail-only IDs are not requested from the demo.
All selected art/font files, all three 640x480 promotion images and all selected
demo ambience/cue WAVs are validated before exposing the menu. Corrupt or missing
assets fail startup explicitly, without retail or invented fallback. Audio
preflight uses the real decoder/cache, kills temporary voices without advancing
or output, and leaves no retained voice. Device-output failures retain the shared
visible audio notice and log behavior; they are not asset-validation success.

The Build 108 provider selects `mainmenu_title.pcx`, `singlepla_title.pcx` and
`options_title.pcx`; retail selects its existing names. Both use the existing
source 640x480 composition and bitmap-font rules. The current aspect-preserving
display policy is retained, rather than the original splash's full-window stretch.

The two editions intentionally retain their different external font metrics:
the supplied demo `mm_font.fnt`/PCXs are 41 pixels high, retail's are 31. Original
`CMainMenus::InitFonts` chooses that path and `CMenuBase::AddLargeTextItemOption`
uses its fading fonts; no edition-specific size normalization exists there.
The shared renderer advances rows and sizes pointer rectangles from those metrics.
`demo_menu.assets` and `menus.artwork` check the real edition heights; demo large
rows also check their hit rectangles. Equal 640x480 composition does not imply
equal glyph dimensions. See the [retake evidence](../implementation/reports/demo_retakes.md).

## Source menu behavior and capabilities

| Page/action | Implemented behavior | Boundary |
| --- | --- | --- |
| Main | Single Player, Bloodbath, Options, Prime Gibs, Quit, in source order | All five actions enabled |
| Bloodbath | `screens/bloodbath.pcx` | Original promotional action; no networking |
| Prime Gibs | `screens/info.pcx` | Original promotional action |
| Single Player | Caleb and Chosen only | Source `_DEMO` omits load/save/custom rows entirely |
| Caleb | Genocide/Homicide/Suicide; normal initially selected | Existing transactional Caleb new-game request |
| Chosen | `screens/modes.pcx` | Promotion, not playable character selection |
| Options | Display, Audio, Controls, Mouse, Joystick, Keyboard, then the replacement VR (XR sessions), SETUP and CHEATS rows | Controls/Joystick/Keyboard unavailable in OpenBlood2, visibly grey; with more than six rows the 41-pixel rows share the list, down to two thirds of a row (nine rows in XR) |
| Display | Existing detail and lighting controls | SDL renderer/resolution pages unimplemented; lighting is a reconstruction option |
| Audio | Sound, volume, music, volume | Historical 16-bit output choice grey/unavailable |
| Mouse | Sensitivity, Input [Normal], Invert Y-Axis | Three Build 108 rows; input-rate control grey/unavailable |
| Quit | Info promotion, then owned quit request | No retail Yes/No dialog |

Unsupported controls are explicitly labelled as OpenBlood2 limitations. Their
disabled state is not attributed to the original demo. Display/audio/mouse
settings use the existing shared preferences file and services. The mouse
sensitivity range remains the shared later-source range (100..1500, step 20),
not the shipped Build 108 range (15..400, step 15). The manifest's disabled
Input [Normal] label does not imply a new input-smoothing implementation.
No save slots, controller mapping, key rebinding, loading artwork or movies are
introduced by this menu work.

## Pause, promotions and lifetime

Esc opens/closes the shared source menu in game. Opening clears held commands
and action edges, releases capture and pauses simulation/game audio; independent
menu audio advances. Focus loss opens the pause menu and never auto-resumes on
gain. Development F10 inspection and inspector-free Release keep their existing
roles. Returning from pause uses no catch-up update. Normal window close quits.

Promotions are typed menu state, not world load requests. Only a fresh Esc,
Space, Return or keypad Enter dismisses; repeats, mouse actions and inspection
shortcuts cannot activate the underlying page. Non-quit dismissal always selects
the root main menu, as `OnEnterMenuState(GS_SPLASH)` does in `GameStates.cpp`,
including Chosen opened from Single Player. During a promotion, menu ambience is
stopped; return selects a fresh ambient. Demo ambience maps source choices
1/3 to 2/4. Cue playback still uses the independent menu mixer.

The existing application-owned menu pause remains active during promotions,
including a promotion opened while a world is paused. Original source menu exit
resumes game sounds globally before its splash; keeping gameplay/audio paused
under the opaque promotion is an explicit adaptation of the shared application
ownership model.

The existing case-insensitive demo EndGame request is consumed outside object
callbacks. The application unloads the world, clears input/inspection, pauses
game audio and calls `Menu::end_demo`: root menu plus enter cue plus Info. Dismissal
returns to Main; a fresh game can then be started. No `endgame.dat` is requested.
Quit uses the same Info artwork with a distinct exit flag; dismissal produces a
single owned quit request. Direct `--view-world` retains its phase-1 terminal exit.

Enter after death still requests a fresh current-world restart, preserving
difficulty but resetting equipment/progress. Both frontend paths feed the same
owned `MenuInput::demo.restart` request and transactional application handler.
The [future checkpoint/save integration contract](demo_contracts.md#temporary-death-retry-and-future-saves)
remains the replacement point; this menu adds no persistence or restore semantics.

## Validation scope

`demo_menu.manifest` checks the finite required set and malformed entries.
`demo_menu.flow` checks page/action rules, absent retail pages, promotion input,
pause clearing, death retry, end return and quit. `demo_menu.assets` composes every
supported page with the real demo archive, compares promotion pixels to decoded
PCXs, exercises sound mapping/cues, and injects synthetic corrupt title/font/
promotion/audio overrides. No commercial data is committed in fixtures.

`gpu.demo_menu` / `--demo --menu-smoke` traverses pages, promotions, settings,
pause/focus/audio clocks, death/Enter retry, the authored first exit, EndGame,
a new game after completion and quit. Its tagged SDL input, fixed 1/60 simulation
clock and isolated preferences are diagnostic only. Authored exit callbacks are
invoked deliberately; this is not a full human walkthrough. `gpu.demo` separately
proves the preserved menu-free path. Retail tests and `gpu.menu_traversal` guard
the shared controller/resources/preferences. Source/static-binary comparisons
and our rendered captures do not establish original-client visual parity: the
isolated Wine observation attempt failed to set the original video mode.
