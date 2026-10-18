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

[Validation record](../../../Documentation/Implementation/original_menus.md).
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

Audio sliders retain 0..50 with step 5; mouse sensitivity retains 100..1500 with
step 20. Default 150 maps exactly to the previous 0.0025 radians per SDL relative
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
Every row is disabled and the screen explicitly says saving/loading is unavailable.
No file snapshot or invented successful save is produced. Quit uses the original
localized prompt with keyboard/mouse Yes/No controls (No selected initially); Y/N
also confirm/cancel. Credits, BloodBath and custom levels remain disabled.

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
the source `%1!s!` string insertion; unknown insertion syntax fails explicitly.

Audio failures use a persistent nonmodal bitmap-font notice and the application
log; they never capture navigation. Application request/preference errors use a
closable modal, with focus events still delivered to input ownership. Ownership
changes clear command levels and USE/item/jump edge latches without simulation;
already committed weapon animation remains ordinary game state.
