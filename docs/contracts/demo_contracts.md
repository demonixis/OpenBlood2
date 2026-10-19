# Original demo edition (`--demo`)

This separately authorized continuation makes the two shipped demo worlds usable
through the shared source menu or the preserved menu-free direct frontend. It does
not reopen M2, resolve M2-GPU-001,
accept M3 or certify a complete campaign. See the [implementation and evidence](../../../Documentation/Implementation/demo_playable.md),
[Voodoo contracts](voodoo_contracts.md), and the implemented [demo menu](demo_menu_contracts.md).

## Edition and startup

`--demo --data-dir DIR` explicitly selects the demo and mounts only `B2Demo.rez`
(case-insensitive filename). Normal launch opens the [demo menu](demo_menu_contracts.md);
Caleb and the selected difficulty start Demo_01. `--demo --view-world` starts
Demo_01 directly with Caleb/medium, and `--world Demo_02` overrides that direct
world. Diagnostics keep their own dispatch:
`--check-data`, `--inspect-world`, `--inspect-player`, `--start-game`, `--view-world`
and `--bench` still work. Retail-content scripted smokes reject the demo; `--demo-smoke` is the separate
menu-free diagnostic and `--demo --menu-smoke` exercises the edition menu.

`Resources::demo()` survives new-game and NEW_LEVEL preparation and exposes
`ServerDE::IsDemoBuild()`. Mounting never combines demo and retail REZs. Loose music
continues to use the selected external directory and its checked Music subtree.
No commercial resources or reference source files are changed or bundled.

The archive contains `worlds/demo_01.dat` and `worlds/demo_02.dat`, variants of
retail `05_CDM` and `05_CDMB`, not byte-identical retail levels. Validate them
separately. The existing missing-BSP policy and source-row diagnostics remain;
there is no new demo-specific object omission or placeholder pickup.

## Removal of the old experiment

Phase 1 removed the fixed-offset `demo_menu_strings` reader, its build entry,
experimental menu branches and three experiment-only tests. The new provider and
limited source menu implementation are documented separately; they do not restore
that reader or its binary-layout dependency. The historical source ledger and
Git history remain evidence of that
experiment, not acceptance of its behavior. Retail menus and shared PCX/FNT,
preferences, input, GPU transport and audio services remain.

Menu-free direct startup constructs neither `MenuCanvas` nor `MenuAudio`: it does
not open a
menu, read menu strings/fonts/artwork, or create its independent mixer. A plain
settings holder supplies the shared Audio/Mouse preferences, loaded from the
existing SDL user directory. Invalid preferences are reported in the log. Demo
direct play has no settings editor; normal menu launch exposes shared settings.
The generic 640x480 surface drawing entrypoint is
shared with the retail renderer, without initializing a menu canvas.

The shipped Build 108 archive lacks the source Build 109 HUD's
`interface/statusbar/weapons/raidcanister.pcx`. The explicit demo HUD layout keeps
the secondary ammunition number without that decorative icon. All other selected
HUD artwork remains mandatory; no retail lookup or invented bitmap is used.
This is an edition adaptation, not a claim of pixel-identical Build 108 HUD behavior.

## Original demo game rules

The existing `_DEMO` branches still restrict player weapons to knife, Beretta,
MAC-10, M16, Tesla and Voodoo. Inventory bombs cannot be constructed; adding an
inventory bomb returns `NotAvailable`; bomb pickups remove themselves using the
source rule; detonating is a successful source no-op. NPC ordinary weapons are
not restricted by the player's list.

Fanatic initialization now accepts **only** that normal `NotAvailable` bomb result
when `IsDemoBuild()` is true. Invalid/other failures and retail failures remain
failures. Demo_01 has 31 authored Fanatics and Demo_02 has four; no Fanatic is
removed to make startup pass. VoodooDollPU is a real source pickup/weapon, also
available in retail where authored. Focus regeneration is shared source behavior.

## Application ownership, pause and end

In the preserved direct route, `DemoFlow` owns Playing, Paused and Finished,
plus pending quit/restart requests. Normal menu launch instead uses shared menu
ownership and the [promotion/end contract](demo_menu_contracts.md#pause-promotions-and-lifetime).
Esc toggles Playing/Paused outside inspection. Pausing skips simulation updates
and pauses the game mixer; no zero-delta update or catch-up update is used. Held
movement/fire and action edges are cleared, and mouse capture follows ownership.
Focus loss pauses and clears input; focus gain never resumes automatically. F10
keeps its development inspection role. Window close quits normally.

The authored first exit requests Demo_02 and entry `05_egress`. Existing transactional
NEW_LEVEL preparation carries player fields, inventory, Focus and difficulty;
failed preparation retains the source session. The arrival elevator uses its
original Door rules and the existing atomic rider solver.

After the original dead-player exit guard, case-insensitive `EndGame` is a terminal
intent **only in demo**. The client records it, and the message pump cancels queued
level changes without inspecting `endgame.dat`. The application consumes the intent
outside callbacks, unloads the world, pauses audio and releases gameplay input.
Normal menu launch uses its prevalidated Info promotion and returns to Main on
dismissal. Direct entry decodes `screens/info.pcx` with the bounded PCX reader.
Missing/corrupt artwork fails
explicitly. Finished draws the 640x480 image with aspect-preserving black bars;
Esc, Enter/keypad Enter or Space quits in this direct route. This is the retained
phase-1 adaptation; normal menu launch now follows the source return to Main.

## Temporary death retry and future saves

User-confirmed phase-1 binding: **Enter after death restarts the current world**
with Caleb's initial equipment and the current difficulty. This resets inventory,
Focus and world progress; it is neither a save restore nor a dead-player revival.
The actual source death, corpse, camera and command guards remain active until
replacement. Esc still pauses/resumes a dead session. No save UI is introduced.

`MenuInput::demo.restart` (`DemoFlow::restart`) is an owned application request
shared by menu and direct demo play. `frontend.cpp` consumes it
outside SDL/object callbacks and calls transactional `GameSession::start_new_game`.
Only successful preparation replaces the owner; a failed retry logs its error and
retains the dead session. This is the deliberate future save-system integration
point: replace the retry handler with checkpoint selection and transactional
restoration, not writes to `m_bDead`, inventory, or aggregate fields. Capture a
checkpoint only after successful world entry/transfer; prepare all restored
objects, original globals, client state, clocks and audio before promotion. Keep
source ownership on failure, reset input once on success, and retain a distinct
fresh-world fallback when no checkpoint exists. Persistence/schema/versioning and
restoration of a dead world's independent objects require their own implementation
and tests; the current Enter binding provides none of those services.

## Tests and limits

`demo.flow` covers pause, held keys, repeats, foreign windows, focus, inspection,
retry and terminal input. `demo.rules` covers regeneration/rates/cap/death,
retail-vs-demo EndGame, pending cancellation and PCX failures. `demo.weapon` uses
real demo assets, source pickup, all five variants, alternate/miss/self damage,
visibility obstruction, six effects, real animation FIRE_KEY delivery and a live
original Fanatic. `demo.worlds` checks authored pickup and exit deliveries, failure
retention, inventory/Focus transfer, the complete 9,920-unit elevator ride, end
art and direct second-world entry. Contact/exit delivery and target placement are
explicit diagnostic setup, not a claim of navigating the entire first world.

`gpu.demo` drives the real frontend through pause/focus, source death, Enter retry,
transfer and end. It uses a fixed 1/60 clock and tagged SDL events, isolates normal
preferences, and asserts that no menu canvas/mixer exists. Normal input is not
filtered. Release runs this without inspection. A separate external test archive
with menu assets removed proves resource independence. Evidence distinguishes
these checks from a full human walkthrough and original-client visual parity.
