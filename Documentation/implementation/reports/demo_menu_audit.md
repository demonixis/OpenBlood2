# Blood II demo menu audit and recommendation

Date: 2026-10-19. This is the authorized phase-2 study, not a menu implementation.
The old incomplete experiment has been removed by [phase 1](demo_playable.md).
M2 remains closed with M2-GPU-001; the graphics stack and reference sources are
unchanged. Recommended effort for implementing the future menu: X-high for the
edition/resource boundary, High for bounded pages and validation.

## Evidence and version boundary

The primary reference is `sources/blood2/ClientShellDLL`, `Shared/ClientRes.h` and
`ClientRes/ClientRes.rc`. Contrary to the initial hypothesis, these sources contain
real `_DEMO` branches. Their version is **DEMO v1.0a (Build 109)**. The installed
`B2Demo.rez` contains a **DEMO v1.0 (Build 108)** client, 719,872-byte cshell.dll,
and a **Ver 1.0 (Build 178)** cres.dll with 43 strings, IDs 0..48. That table does
not contain the source menu IDs 500+. Adding one missing string would not reconcile
these different builds.

The supplied REZ has 1,364 entries and SHA-256
`e410ce2944210c776c7407a79e01cfa031c782132f68407cd0871379c5e7218c`.
Its source menus, strings/PE sections, resource index and supplied Readme.txt were
examined without executing or modifying the reference DLLs. Binary addresses below
are analysis evidence for this particular image, not a proposed runtime API.
The original Windows client was **not interactively traversed in this run**;
visible timing, exact layout and runtime device-dependent states remain unverified.
The table deliberately separates source behavior, static shipped-build evidence
and current implementation support.

## Pages, resources and behavior matrix

| Area | Original source / shipped evidence | Current support and required adaptation |
| --- | --- | --- |
| Main | Source order: SINGLE PLAYER, BLOODBATH, OPTIONS, PRIME GIBS, QUIT. Build 108 literals at file offsets 0x94408..0x94438 and construction references agree. | No demo menu in phase 1. A future source controller can preserve order and actual actions. |
| Bloodbath | `_DEMO` opens `screens/bloodbath.pcx`, not multiplayer. Path is also present in Build 108. | Real shipped promotion bitmap; should be an enabled promotional action, not a grey multiplayer item. |
| Prime Gibs / Quit | Info PCX; Quit passes the splash exit flag and skips the retail Yes/No dialog. | Reuse bounded PCX display and owned return/quit intents. Do not reuse retail confirmation blindly. |
| Solo | Caleb opens difficulty; Chosen opens `screens/modes.pcx`. `_DEMO` omits Load/Save/Custom entirely. Build 108 has just Caleb/Chosen literals in this construction block. | Do not render disabled retail save/custom rows as historical demo behavior. Phase-1 Enter retry is a temporary application action. |
| Difficulty | GENOCIDE, HOMICIDE, SUICIDE exist at 0x94104..0x9411c; source maps the three difficulty commands. | Existing typed new-game request supports these difficulties; Caleb/normal remains the phase-1 default. |
| Options | Source and Build 108 construct Display, Audio, Controls, Mouse, Joystick, Keyboard. Disassembly 0x10052331..0x10052398 has all six Add calls and the command dispatcher routes all six. | Blanket greying is unsupported by this evidence. Any unavailable page must be identified as an OpenBlood2 limitation. |
| Display | Build 108 contains renderer, resolution and three detail labels. Source delegates renderer/device changes to the old engine. | Current SDL frontend provides detail/lighting only. Native display/renderer behavior requires a separate design; the added lighting row is a reconstruction control. |
| Audio | Build 108 labels: Sound, Sound Volume, 16 Bit Sounds, Music, Music Volume (0x9457c..0x945ac). | Shared SFX/music settings work; obsolete 16-bit output choice has no equivalent. Preserve the distinction between available settings and historical controls. |
| Mouse | Build 108 **does contain** Sensitivity (0x99730), Invert Y-Axis (0x99748) and Input [Smooth] (0x997d8), outside the old reader's window. Source has sensitivity/input rate/invert/mouselook/lookspring and later wheel support. | Existing sensitivity/invert work. Audit the complete Build 108 mouse constructor before selecting further rows. Old reader incompleteness is not a reason to disable this page. |
| Controls / joystick / keyboard | Binary includes control names, Use Joystick and Turn Speed. Supplied Readme section 11 explicitly describes Controls rebinding and enabling a joystick through its page. | Rebinding/device support is not provided by current menus. Keep it explicitly unavailable until implemented; no claim that it was originally disabled. |
| Title artwork | Build 108 literals reference `mainmenu_title.pcx`, `singlepla_title.pcx`, `options_title.pcx`; source Build 109 uses `mainmenu.pcx`, `singleplayer.pcx`, `options.pcx`. Both sets are shipped. | Edition resource selection must include images and layout metrics, not only missing text. Visual comparison is still needed. |
| Fonts / background | `interface/mainmenus` ships mm_font.fnt and images 01..06/dis, background, slider/tab and dialog art; `interface/fonts` also ships MenuFont1/2. | Existing bounded PCX/FNT decoder is reusable. Preserve measured font bounds rather than patching arbitrary glyph widths. |
| Sound / pause | `_DEMO` remaps ambience 1→2 and 3→4; only ambient02/04 are shipped, plus enter/select/esc. Source pauses game sound while menus have focus and resumes on loss. | Reuse independent menu mixer and input ownership only in the future menu. Phase 1 never constructs it. Menu sound failures must remain observable. |
| Promotion input | Splash source accepts Esc/Space/Return and returns to menu, or exits when requested. It stretches to the display. | Current end surface preserves aspect ratio; a future menu needs an explicit return-page intent and a documented scaling policy. |
| EndGame | Source compares case-insensitively after the dead-player guard, clears in-world, pauses, selects main menu, plays enter01 and displays info.pcx. | Phase 1 unloads and shows the same image, then quits. A future menu should replace that terminal destination with source splash→main-menu ownership, without loading endgame.dat. |
| Loading/start | `_DEMO` skips opening movies and selects loading1/2.pcx according to Demo_02; title text is overlaid. | Both loading PCXs and title.pcx exist. Loading presentation remains separate from the implemented direct startup. |
| Cheats / other edition rules | Demo has its own first level, cheat restrictions and bomb/weapon rules. | Retain gameplay edition rules independently of menu UI. Cheats, networking and a save system are not implied by adding pages. |

The historical 24-label reader covered only a portion of this data. Its size,
build-string and NUL/ASCII checks made that small known mapping fail closed, but
its offsets coupled the entire menu to one DLL layout and did not prove complete
navigation. Its source ledger is retained unchanged as historical evidence.

## Architecture comparison

| Approach | Fidelity | Maintenance / robustness | Binary-version dependency |
| --- | --- | --- | --- |
| Adapt source menu branches directly | Good for rules/order/promotion behavior; Build 109 still differs from 108 assets/text. | Small controller changes, but scattered missing-resource substitutions would be fragile. | Low for code; unresolved text/art mismatch if used alone. |
| Explicit edition resource provider with shared source controller | Good when the edition manifest records text, title paths, font/layout choices and promotion assets separately from capabilities. | Central bounded validation; independently test every required entry and error; retail provider stays unchanged. | None required at runtime if the reviewed text manifest comes from source resources plus documented edition overrides. |
| Separate demo interface | Can reproduce one image, but risks drifting from source commands, pause and preferences. | Duplicates controller/input/audio behavior and future save integration. Appropriate only if a substantially different original interaction model is demonstrated. | Optional, but recreating labels through offsets would retain the same weakness. |

**Recommendation: limited source-menu adaptation plus an explicit edition resource
provider.** These are complementary choices. No global FormatString fallback, fake
cres.dll, patched original DLL or ABI polyfill is needed for this source-integrated
engine. Missing objective/gameplay strings must continue to fail independently;
a menu resource adapter must never fabricate success for unrelated callers.

Use a finite menu-specific lookup keyed by existing semantic resource IDs. Retail
continues to read its external cres.dll. For demo, a reviewed declarative text
manifest can originate from the available `ClientRes.rc` strings, with explicitly
recorded Build 108 casing/wording overrides. Keep it separate from controller logic
and game localization. Commercial artwork/fonts/audio remain in the user's REZ.
This avoids a runtime dependency on a precise cshell.dll layout. If distribution
policy instead requires all text to be supplied externally, use the same manifest
format as an external file and validate it at initialization; do not silently fall
back to arbitrary DLL strings. The final text packaging choice and any translated
edition need review before implementation.

A validated binary extractor could be an optional research/import tool, keyed by
whole-file digest and an explicit known-build recipe. It should produce that
manifest, never sit on the normal startup path. PE string-table parsing alone
cannot recover compiled C++ menu literals. A generic string scan cannot reliably
associate controls with actions or determine enabled state. The old offset reader
therefore remains historical evidence, not the default solution.

## Proposed implementation sequence

1. Finish observation of the original Build 108 client: traverse all six Options
   pages, pause/resume, quit and three promotions; record constructor-derived
   labels separately from visible behavior, font/layout and device restrictions.
   The static inventory above narrows this work; it does not replace it.
2. Introduce a small menu-only edition resource contract and complete manifest
   validation. Check every required label/image/font/sound before exposing a page;
   include missing/ambiguous/corrupt input tests. Keep retail lookup unchanged.
3. Port only the needed `_DEMO` controller branches into Blood2: main, solo,
   difficulty and promotions. Shared original source files remain untouched.
   Separate original item presence/action from current engine capability.
4. Connect supported Audio/Mouse/display controls and label unsupported functions
   honestly. Do not activate fake saves or network screens, and do not claim that
   unavailable OpenBlood2 controls were disabled in the demo.
5. Extend application ownership so pause and EndGame promotions can return to the
   menu. Preserve transactional sessions, input clearing, preferences and separate
   mixer clocks. Retain/document Enter retry until the checkpoint restoration
   handler described in the demo contract replaces it.
6. Validate every page/action in Debug and inspector-free Release with demo-only
   resources; repeat retail menu traversal. Compare against the captured original
   observations before claiming menu fidelity. No implementation is included in
   this audit, and no graphics-stack change is required.
