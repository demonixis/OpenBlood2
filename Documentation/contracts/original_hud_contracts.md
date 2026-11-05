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

This replaces the demonstration **statistics HUD**. Inventory scrolling (see the
[inventory cursor](inventory_cursor_contracts.md)), the weapon list, the objectives
page/skull, the power bars and the message lines (below) were connected later.
Communication portraits/text and multiplayer/team displays from the remaining
original UI classes are not connected. The inspector's boss bar remains diagnostic.
Menus, UI scaling preferences, original executable pixel-for-pixel comparison,
Windows/macOS/mobile execution and full-game UI acceptance remain unverified.

## Demo edition resource difference (2026-10-19)

The shipped Build 108 demo lacks `weapons/raidcanister.pcx`. Its explicit HUD variant
keeps the secondary ammunition number without that decorative icon. Other required
art remains strict; retail composition is unchanged. See [demo contracts](demo_contracts.md).

## Weapon tabs (F1, 0.5.1)

Issue #5. `InitWeaponTabs`, the weapon part of `AdjustRes`, `UpdateWeap`/
`UpdateCurrentWeap`, `ToggleWeapons` and `DrawWeaponTabs` are imported. Ten 150x25
tabs (`border_weapon.pcx`, slot numbers 1-10 right-justified in the small numbers)
stack around the vertical centre at the right edge and slide in or out over 0.25 s,
in high detail only, with no display timeout. Each slot shows its weapon's
`m_szStatusIcon`, the current slot its `m_szStatusIconH` when it has one; a reserved
inventory slot (>= 10) keeps the previous highlight. The original shell toggles them
with VK_F1 without Shift; the menu frontend does the same in play, before its
diagnostic F1/F2/F3 guard (F1 stays the inspector key in the `--view-world`
inspection loop). A new world hides them, as `Reset` did. Missing weapon art leaves
a tab empty and is reported in `weapon_error`, never failing the HUD. The tabs
occupy a third atlas row (1024x256).

## CTF inspection icons

The [CTF continuation](ctf_contracts.md) adds selective original team and carried
flag PCX presentation at native top-right positions. Its atlas is now 1024x128;
exact magenta transparency and existing stat/crosshair composition remain.
This supersedes the blanket team-display omission only for these two icons;
network scoreboards and the original console overlay remain unavailable.

## Objectives, power bars and message lines (issue #8)

Tester issue #8 reported the missing air bar, objectives and pickup messages. The
original `CNewStatusBar` objectives and power-bar code, `CMessageMgr` and
`CInventoryMgr::SendPickedUpMessage` are now imported. The HUD atlas is 1024x512:
a fourth band at y = 256 holds the objectives page (x 0), its skull icon (x 512)
and the power bar (x 768), with exact magenta transparency (`m_hTransColor`).
Missing art never fails the HUD; it is reported in `objectives_error` or
`power_error`, as the weapon tabs report `weapon_error`.

### Objectives page (Tab)

`InitObjectivesTab`, `UpdateObjectives`, `ToggleObjectives`, `DrawObjectives` and
`DrawObjectivesIcon` are imported. The retail page is
`interface/objectives/objectives.pcx` (250x150), drawn at (100, 75). The title is
centred at x 135, y 35 and the text wrapped by `DrawFormat` at (35, 50), 200
pixels wide, in `obj_font_1.pcx/.fnt`. An empty page reads "No Current
Objective". The FNT space width is the menus' quarter height (the CoolFont source
is unavailable).

- **Updates.** Each SMSG_OBJECTIVES (single or custom games; resource IDs give
  strings 5000/6000 + id - 1) and each LoadClientData restore counts one
  `UpdateObjectives` call. The client shell plays `Sounds\Events\bellding1.wav`
  locally for a non-null page. Null strings reset the page and hide the icon
  without a sound.
- **Updated icon.** `interface/objectives/skull.pcx` (38x66) shows at
  (width - 38 - 5, 5) when an update arrives while the page is hidden or scaling
  down. Opening the page hides it.
- **Tab.** The original VK_TAB key calls `ToggleObjectives` in single and custom
  games, with `Sounds\Interface\MainMenus\missionobj.wav` on each accepted toggle.
  It is the fixed `objectives` Interface action (Tab). It is not a Controls row:
  CMenuControls ignored Tab, so `controls.cfg` and its version are unchanged.
  There is no gamepad default. The multiplayer frag-bar branch is not
  implemented.
- **Scaling.** The page scales up or down over 0.1 s through the original
  `g_ObjScale*` tables. A toggle during a scale is ignored. There is no display
  timeout.
- **Clamped ratio.** The scale ratio is clamped before indexing the tables: the
  source indexed past them when the clock moved backwards.
- **New world.** `Reset` on a new world hides the page and the icon; the page
  keeps its text.

### Power bar (air, throw, boss)

`SetPowerBarLevel`, `TurnOffPowerBar` and `DrawPowerBar` are imported. The power
bar is a 150x16 magenta surface at ((width - 150) / 2, ammo tab top - 32).

- **Drawing.** The fill covers `int(barW * percent)` pixels from `75 - barW / 2`,
  and the frame is drawn over it with a black key. Throw uses
  `interface/statusbar/bar_throw.pcx` in yellow, air the same frame in blue, and
  boss `bar_health.pcx` in red.
- **Timeout.** The bar hides five seconds after its last request. It expires in
  `DrawPowerBar` but is still drawn that frame.
- **Recomposition.** The surface is recomposed only when the fill width or the bar
  type changes, which limits atlas uploads.

The shell rule is the original one: a percentage at or below 0, at or above 1, or
non-finite turns the bar off. The requests come from three places:

- **Air.** `PlayerStats::air` is the original PLAYERSTATS_AIRLEVEL value,
  `m_fAirLevel / 100`, validated to 0..1. The client reacts only when it changes,
  as the source sent the field only then.
- **Throw.** While a proximity, remote or time bomb is held, the client weapon's
  WS_FIRING call sends `min(held / 0.75, 1)`. Overholding a proximity or time
  bomb past 1.25 s prints "Idiot! Let go next time!".
- **Boss.** SMSG_BOSSHEALTH sends the boss health fraction.

No server sends the unused SMSG_THROWDISTANCE.

### Message lines

`CMessageMgr` (`game/original/MessageMgr.cpp`) is imported with its 20-entry ring,
the shell's `SetMaxMessages(6)`, five-second lines, and the oldest-first expiry in
`Draw`. Lines are drawn at (1, 0), 16 pixels apart, in white with no shadow. They
appear in play before the status bar, independent of the status-bar camera gate,
as PostUpdate drew them.

- **Editor line.** While typing, the editor line reads "SAY:" plus the text, with
  a cursor in the second half of each second.
- **Font.** The replacement font is Dear ImGui's default at the original 16-pixel
  height; the GDI Arial 6x16 request has no portable equivalent.
- **Encoding.** Windows-1252 resource bytes are shown as Latin-1 when a line is
  not valid UTF-8.
- **New world.** A world reset clears the ring, because the new world's clock
  restarts. This is an adaptation; the source kept absolute-time lines.

The lines now replace the earlier chat list. Original sources already present in
the port reach them:

- the typed SMSG_CONSOLEMESSAGE line (80 bytes at most);
- the trigger LOCKED/UNLOCKED strings;
- IDS_GENERAL_IMMOBILE/CANMOVE (265/264), now sent as the source did;
- the Willpower, Stealth and Anger expiry strings 277/276/278, previously only
  counted (`powerup_console_messages` or `powerup_console_unavailable`);
- the local chat echo;
- the pickup messages below.

The port's own control notices (auto-run, view, drop refusals) stay replacement
lines, drawn after the messages for three seconds. Not ported:

- the cheat-message line (`DrawCheatMsg`);
- the `_ALL` message sound;
- network chat.

### Pickup messages

`CInventoryMgr::SendPickedUpMessage` runs for a player with a client, for
PickupObject senders. The port's KeyPickup counts too, because it was a
PickupObject subclass in the original.

- **Display name.** `FormatString(m_nNameID)` when the ID is set, otherwise the
  object name.
- **Strings.** CHWEAP_OK shows IDS_GENERAL_PICKUP_1 (260, "Picked Up %1!s!").
  CHWEAP_NOTAVAIL and CHWEAP_NOAVAILSLOTS show 261 and 262, but only when
  `hObject != m_hLastPickupObject && iRet != m_nLastPickupResult`. That source
  AND quirk is kept.
- **Last pickup.** The last object and result update after every PickupObject
  message, including CHWEAP_NOMESSAGE. They start as NULL/CHWEAP_OK and are not
  saved.
- **Results per caller.** Ammo is OK or NOMESSAGE. Weapons use ObtainWeapon's
  result, with ammunition acceptance overriding it unless the slots are full.
  Items and keys use their add result; an owned bomb's refused ammo is
  NOMESSAGE. Powerups report only an accepted CHWEAP_OK.
- **MID_PICKEDUP.** The reply is unchanged.
- **NPCs.** NPC pickups print nothing, because they have no client.
- **Missing strings.** A missing cres.dll string sends no line. It sets
  `pickup_message_error` and records no session failure, through the new bounded
  `FindClientResourceString` service.
- **Retail text.** The retail IDS_WEAPON_BERETTA (350) ends with a space, so the
  line is "Picked Up Beretta ", as in the original.

### Verification

`hud.panels` (headless) covers:

- the message ring, expiry and positions;
- the objectives state machine and power requests without art;
- the Tab action;
- the shell's air and throw rules;
- the pickup reply without cres.dll.

`hud.private` covers the retail art (page 250x150, skull 38x66, power bar at
(245, 373) in 640x480), the page composition, scaling rectangles, icon rules,
power colours and timeout. In 01_SubwayA it also checks the "Picked Up Beretta"
and "Picked Up RedKey" lines, the ALREADYHAVE suppression, objective 1 ("Kill
Gideon!") with its sound, and the air request.

GPU presentation of the new band was not captured in an automated test. XR shows
the same 640x480 HUD canvas.
