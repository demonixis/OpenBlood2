# Original player status-bar presentation

This user-requested continuation replaces the compact demonstration HUD with the
original `CNewStatusBar` statistics tabs. M2 remains closed with M2-GPU-001; this
work does not reopen its GPU investigation or accept M3. See the
[source ledger](../implementation/sources/original_hud_sources.json) and
[validation record](../implementation/reports/original_hud.md).

## Original code and visible behavior

`game/original/NewStatusBar.cpp` selectively imports `InitStatTabs`, the statistics
part of `AdjustRes`, `SetupTab` and `JustifySurface`. Its update bridge retains the
`DrawStatTabs` inclusion flags/order, and `UpdateCurrentWeap` icon selection for
normal slots. The bottom left health/armor tabs and bottom right current-weapon
ammo tab use the original external PCX borders, icons and ticker. Both numeric
fonts use external PCX/FNT data. No system-font approximation is used.

Coordinates are the original native pixel sizes. A viewport narrower than 512 or
shorter than 384 uses the source low-detail icon/number layout. The alternate tab
uses the source raid-canister icon and is visible only for a nonzero distinct ammo
pool. The source's 135x75 allocated alternate surface and 60x20 placement dimensions
remain distinct. Reserved inventory weapon slots clear the current weapon icon.
The knife displays the delivered zero ammo value, as the source does; the previous
`--`, English labels, threshold colors and bottom numeric strip are removed.

The existing typed `PlayerStats` gains finite nonnegative maximum health and armor
from original `CPlayerObj` damage state. Value changes, maximum changes and forced
entry/restoration publication remain checked/owned. Maxima-only changes invalidate
the composed ticker (the source cached only current values). Presentation still
truncates and clamps health/armor to 0..65535 and ammunition to 0..999. Zero maxima
draw empty tickers; over-maximum values clip to the bitmap. These bounds avoid the
source's division/conversion hazards. A bounded stack number buffer replaces its
unsafe `digits+1` allocation, `sprintf` and scalar `delete`.

An absent player, an external camera or unsupported stats hide the HUD. Rendering
performs no simulation, message consumption or inventory mutation. F1 and collapsed
inspection do not disable it. Physical numeric weapon commands are unchanged.

## Surface services and resources

The new typed `HudCanvas` supplies CPU surfaces, exact magenta color-key blits,
clipping and bitmap numbers. This is replacement engine service code, not a legacy
client ABI. `CoolFont.h` is available in the reference but its implementation is
only in the shipped library. FNT loading is therefore new: the observed 190-byte
layout is a little-endian height followed by 94 widths for `!` through `~`.
Height and summed glyph bounds must match the bitmap. Only numeric drawing is used.

`read_pcx` accepts version 5 RLE images with 8-bit indexed or three RGB planes.
Indexed images require the terminal 256-color palette. It checks dimensions,
stride, row-complete RLE, nonzero runs, truncation and exact payload consumption
before publishing output. Limits are 16 MiB encoded, 4096 per dimension, four
million pixels and 8192 bytes per plane row. Failed output is empty. Palette
colors remain opaque until composition; only exact RGB(255,0,255) is transparent
at final presentation. No near-magenta tolerance or grayscale fallback is used.

Assets use existing normalized exact-path REZ precedence and stay external. Images
are owned/cached by the status bar; failures are latched and reported with the
resource path. Missing required art is a rendering failure, never a successful
fallback to the demo HUD. The prototype PCX reader was reviewed but not imported:
it tolerated truncated rows, assumed palettes and clipped overlong runs.

## GPU ownership and inspection

Four composed surfaces and the [original crosshair](crosshair_contracts.md) share
one 720x75 RGBA SDL_GPU atlas. Only changed snapshots
or layouts upload it; both transfer and texture updates use cycling. The existing
ImGui SDL_GPU backend draws textured quads behind the inspector and above the
screen fade. It supplies transport only, not the original layout or typography.
No dependency pins, shaders, world-renderer optimizations or platform stack change.

`HudRenderer` rebuilds its resource owner when the session's Resources identity
changes, retains no game objects, and waits for GPU idle before releasing its GPU
objects. It is destroyed before the shared UI runtime and SDL device. Both Debug
and inspector-free Release execute this same path. Session inspection exposes raw
stats/maxima, the most recently composed layout/revision and loaded bitmap paths
and dimensions, including while the HUD is camera-hidden.

## Explicit limits

This replaces the demonstration **statistics HUD**. Inventory scrolling, weapon-list
toggle, objective page/skull, power/air/throw/boss bars, communication portraits/text
and multiplayer/team displays from the remaining original UI classes are not
connected by this continuation. The inspector's boss bar remains diagnostic.
Menus, UI scaling preferences, original executable pixel-for-pixel comparison,
Windows/macOS/mobile execution and full-game UI acceptance remain unverified.

## Demo edition resource difference (2026-10-19)

The shipped Build 108 demo lacks `weapons/raidcanister.pcx`. Its explicit HUD variant
keeps the secondary ammunition number without that decorative icon. Other required
art remains strict; retail composition is unchanged. See [demo contracts](demo_contracts.md).

## CTF inspection icons

The [CTF continuation](ctf_contracts.md) adds selective original team and carried
flag PCX presentation at native top-right positions. Its atlas is now 1024x128;
exact magenta transparency and existing stat/crosshair composition remain.
This supersedes the blanket team-display omission only for these two icons;
network scoreboards and the original console overlay remain unavailable.
