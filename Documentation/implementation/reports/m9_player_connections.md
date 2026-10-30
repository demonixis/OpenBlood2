# M9 — Server player connections and Bloodbath start points

2026-10-30, following `bd8aa13c`. The internal server registry now supports up to
16 live connections in Bloodbath, Team and CTF. Solo keeps one. A checked player
belongs to one live connection, including the restored-player association window;
stable non-reused tokens, deferred exit and private 256-event queue bounds remain.
Disconnecting one player cannot clear another player's identity or messages.

Bloodbath now uses the original `GoToStartPoint` random ordinal over the complete
named marker list. `Multiplayer=false` entries participate, as in the source.
Exactly one draw is consumed, without spawn retries. Existing marker triggers,
rotation, checked placement/grounding and force-position ordering are retained.
An empty/invalid list fails with cleanup instead of manufacturing an origin.
Solo named selection, fallback and explicit inspection selection are unchanged.
The [source ledger](../sources/m9_player_connections_sources.json) identifies the
original branches and bounded engine adaptations.

## Verification

[Recorded Linux validation](evidence/m9_player_connections_validation_2026-10-30.json):
**30/30 distinct headless cases**, including synthetic entry/ownership/RNG tests
and four private-data solo entry/restore cases; **3/3 ASan/UBSan connection cases**.
Tests cover all three M9 capacities, unchanged other-mode capacity, stale tokens,
exclusive ownership, independent queue saturation, exit/reconnect, 64 marker
selections and exact generator advancement. Existing solo save/restore paths pass.
The test target is available in the server-only build. Initial fixture compile
errors, premature Not Run results and a misplaced target are retained in the logs.
All **6,271 protected files** keep their SHA-256 hashes, including PBR and prior
reference/evidence outputs. No renderer path changed or GPU run was added.

## Remaining work

This is an internal connection prerequisite. The dedicated still refuses peers;
there is no network player admission or claim of multiplayer play. Original
multiplayer profiles/ammo/announcements, Team start filtering, death/respawn,
match rules and protocol delivery remain next. The original client shell still
contains local server services. Windows/macOS/mobile/Quest and human LAN play
are unqualified. M2-GPU-001 and M3 acceptance are unchanged.
