# Original menu frontend

User-authorized UI-M0 through UI-M6 are independent of the global roadmap. M2 remains
closed with reservation M2-GPU-001. Graphics baseline: bb98fd7.

## UI-M0 foundation

Selected original Build order and command/resource identifiers live in
`game/original/MainMenus.cpp`; see the [source ledger](../implementation/sources/original_menu_sources.json).
The controller owns page, selection and one pending application command. Rendering
and mouse events never execute game commands. Source Story and Chosen navigation,
medium difficulty selection and eight save slots are retained. Unsupported actions
are visible and disabled; no persistence is implemented.

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
