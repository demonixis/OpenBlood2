# M9 — Fresh Bloodbath player profiles

2026-10-30, following `d2a64d52`. Bloodbath profile messages now follow the
original multiplayer entry branch: one addressed startup announcement, shared
attribute validation and weapon acquisition, then the original entry reset.
The four retail characters support all seven source skin paths; the authoritative
model and PlayerPhysics descriptor agree. Duplicate/malformed requests and
non-retail-player weapon types fail before allocation or the announcement draw.

Starting ammo now follows the same match level as multiplayer pickups: None,
Half, Normal, Double and Insane retain the source quantities and infinite/full
ammo behavior, including inventory bombs. The default PlayerInit remains solo's
single Beretta; the reference client's absent-B2C fallback of weapons 1..9 is
verified separately. No B2C frontend or new network admission is implied.

The client-owned announcer resolves the server's index modulo eight using the
original Voice248..255 files, cres.dll strings 4001..4008 and portrait 4. Original
English fallback lines remain available with visible resource diagnostics.
Selection has no server object dependency or client random draw. The server does
not play the announcement. The existing CommLink service delivers it locally;
CommLink/VoiceMgr still need audio/service extraction before remote gameplay.
The [source ledger](../sources/m9_player_profiles_sources.json) records provenance.

## Verification

[Linux validation](evidence/m9_player_profiles_validation_2026-10-30.json):
**47/47 distinct headless cases**, **5/5 ASan/UBSan cases**, and **2/2 GPU cases
after the conversation fixture correction**. Tests cover ownership/private event
routing, one exact RNG draw, duplicate and malformed init, attribute limits,
dual Berettas, all five ammo levels, all 28 retail character/skin combinations,
the nine fallback weapons and real announcer resources. Solo entry, death,
conversations and save/restore regressions pass. The standalone announcer links
no ServerAdapter, original server shell, object registry or GameSession.

The conversation GPU fixture manually updated CommLink and then read the client
mixer before publishing the resulting logical voice replacement. Its initial
failure and probe are retained: the old logical voice was gone while its client
replica still existed. The fixture now publishes at the normal boundary before
checking both lifetimes, without advancing either clock or changing production
audio behavior. The rerun passes on native NVIDIA Vulkan.

All five solo preservation traces remain byte-identical: 240 FX, 360 audio,
360 movement, 720 environment and 300 camera/HUD frames. All **6,285 protected
files** retain their hashes, including PBR and earlier reference/evidence outputs.
Sanitized dedicated Crypt also completes 60 zero-client ticks with unavailable
audio/video drivers and zero discarded ticks. No emulator or simulator was used.

## Remaining work

This is fresh internal Bloodbath initialization. Dead respawn/slow death,
delayed spawn voice, configurable regeneration, Team/CTF profiles, match rules,
remaining client-shell services and wire admission are subsequent work. Team/CTF
profile requests remain refused explicitly. No multiplayer game is qualified by
these entry checks. Windows/macOS/mobile/Quest and human LAN play remain untested;
M2-GPU-001 and M3 acceptance stay independent.
