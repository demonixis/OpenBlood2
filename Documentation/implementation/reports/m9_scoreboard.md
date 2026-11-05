# M9 — Original Bloodbath scoreboard and player notices

2026-10-31, following `1a4e82be`. The independent network frontend now displays
Bloodbath scores with the original PCX/FNT resources. Tab or gamepad Back toggles
the table, death opens it and validated revival closes it. Arrival/departure text
uses the original localized strings. Dedicated and playable-host clients share
these consumers; no server gameplay, transport schema or solo path changes.

See the [contract](../../contracts/multiplayer_contracts.md#original-bloodbath-scoreboard-and-player-notices)
and [original/baseline source ledger](../sources/m9_scoreboard_sources.json).

## Behavior and adaptations

The wire roster remains admission-ordered. A separate owned score list reproduces
CFragInfo's insertion/movement rules: move only across strictly lower scores on
gain, or strictly higher scores on loss. Equal scores retain their previous display
order. Initial late-admission scores sort through the same rule. Departed identities
are removed, fresh reconnects are new rows and invalid/duplicate events are inert.

ClientFragTable composes the four Chosen portraits, fragbar.pcx and
frag_font_1.pcx/.fnt from the client's mounted retail data. Native pixel placement,
centering, source row limits, signed scores, 145-pixel names, magenta keying and
compact layout below 350 pixels are retained. The original redundant DrawClip
short-prefix draw is preserved. GPU upload occurs only when content/resolution
changes; storage is bounded to 16 authored rows. Tiny viewports safely show no
rows rather than underflowing unsigned dimensions. GPU resources remain frontend-
owned and wait for completion at destruction.

The existing portable HudFont adapter replaces unavailable CoolFont implementation.
Its ASCII glyph repertoire and quarter-height space width are unchanged; unsupported
bytes do not produce glyphs. Names remain intact in the roster and notice text.
Unknown ping is explicitly `(-)`, matching the original pending measurement;
measured ping delivery and Unicode scoreboard glyphs are not implemented here.
Native/compact screenshots are visual checks of this port, not retail pixel-parity
references. No commercial artwork is copied into Git.

The score overlay hides the crosshair while leaving movement/fire and host ticks
active. Tab repeats and input while unfocused/in menus are ignored. Gamepad Back
is a network-only addition. PlayerDead reveals scores; a later validated living
camera state closes them, avoiding the source shell's nonnegative-health revival
heuristic during slow death. Match-end display and team columns remain separate.

IDS_ADDPLAYER/IDS_REMOVEPLAYER are read from cres.dll as data. Bounded `%1!s!` and
`%%` expansion treats player names literally. Missing/malformed strings use the
exact original English with a diagnostic. Initial admission reports all known
players including self; departure uses the cached name and excludes self. Notices
share four lines and independent five-second expiry with frag text, create no
audio and ignore duplicate rosters. Notice rendering retains the existing portable
ImGui font substitution for Windows Arial.

## Verification

**15 native tests, 12 ASan/UBSan tests and 12 inspector-free Release tests pass.**
Native includes two GPU cases; Release includes the scoreboard GPU capture.
Commands and complete outputs are retained in `evidence/m9_scoreboard_*_2026-10-31.*`.

- Synthetic tests cover stable ties, gains/suicides, late scores, removal/reconnect,
  duplicate/refused atomicity and reset. SDL event tests cover Tab, repeats,
  window/focus/menu gates, gamepad Back, death/revival and manual visibility.
- Generated resources exercise localized names containing `%`, `%%` expansion,
  malformed-template fallback, arrival/departure, shared queue replacement/expiry
  and absence of sound. Retail checks load the actual PCX/FNT and both localized
  strings, test revision caching, signed scores, compact/tiny viewports and missing
  resources. Retail notice lookup uses no fallback.
- Native GPU captures at 640×480 and 320×240 show all four actual portraits,
  score columns, clipped long names and compact rows. Captures are retained in
  each build's `tests/network/scoreboard-captures/`; their hashes enter the checks
  evidence. The port's images were visually inspected. GPU validation remains
  distinct from the headless sanitizer run.
- Actual dedicated and graphical-host process tests check the killer reaches the
  top of the scoreboard, named arrival/departure counters, death reveal and
  respawn hide, in addition to existing combat/audio assertions. These are
  scripted retail encounters, not human LAN or a 16-player qualification. Dedicated
  native/sanitized Blood Arena completes 2,400 ticks without discarded ticks,
  13 shots, one frag, 13 impulses and health-100 respawn. Fields hosting completes
  1,200 frames, 3,608 ticks with eight discarded server ticks and zero local
  discards, three admissions/departures, 33 shots and one frag. Both remote clients
  receive all named arrivals; the survivor receives one departure.
- Five fresh solo traces are byte-identical to the preceding lot: effects 240,
  audio 360, movement 360, environment 720 and camera/HUD 300 frames. Independent
  scoreboard executables in all three builds contain no server gameplay symbols.
  All 6,851 protected hashes, including PBR, immutable captures and prior evidence,
  remain unchanged. CMoveMgr and logical server audio have no new consumers or ticks.

The first native build failed on the new frag table's missing transport header;
adding its direct include fixed it. The failed log is retained. Final project
builds have no warnings; the external effects-oracle helper retains its known
indentation warning.

This lot does not qualify physical gamepads, high-DPI/XR hardware, Windows/macOS
interoperability, impaired combat, all maps/weapons, mobile/Quest or human LAN.
Measured pings/chat, communication portraits, remote music/FX, match limits/rotation,
Team/CTF and Host/Join/LAN screens remain M9 work. M2-GPU-001 and M3 acceptance
remain independent.
