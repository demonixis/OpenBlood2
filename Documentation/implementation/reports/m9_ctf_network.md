# M9 — CTF network admission and independent flag presentation

This lot opens CTF on the dedicated and graphical host with protocol **18**.
It follows [CTF server rules](m9_ctf_rules.md) and connects their publications to
independent client icons, scores, localized notices and local flag sounds.
The [source ledger](../sources/m9_ctf_network_sources.json) records original
BloodClientShell/NewStatusBar/ClientRes behavior and the reconstruction adapters.

## Admission, replication and presentation

Use `--mode ctf --world multi/ctf_cabana` on `blood2-server`, or
`--mode ctf --host multi/ctf_cabana` on `blood2`. Clients connect normally and
select `--team auto|blue|red`. The same checked protocol/edition/content hash,
team assignment/skin, movement, combat, respawn and rotation apply. The four maps
are Cabana, Dead Cities, DMX and Jam. Capture options are documented in the
[operating guide](../../getting_started.md); no new menu or discovery UI is added.

Capture messages carry generation-qualified capturer identity plus absolute
individual and team match scores. The server snapshots those values when the
original capture publishes, before any later departure. The portable codec
bounds points to 0..1,000,000 and scores to ±1,000,000; noncapture messages must
have no capturer or score payload. The independent inbox checks admitted mode,
recipient team, world and ordered private sequence. Duplicate events cannot add
points twice. Departed players are never recreated; late arrivals get current
roster/team totals without replaying historical captures.

Grab displays the opposite flag through the original recipient-team mapping.
Return and need-flag events leave that icon alone; drop clears it. Capture clears
it for every recipient, including another carrier, preserving the original
client quirk. Ordinary ClientWorld attachment/membership/pose replication carries
the real external flag models; presentation executes no flag gameplay callbacks.
The existing Team scoreboard displays combined kill/capture scores.

Original cres.dll strings supply grab, return, need-flag and capture notices.
Capture formats retain the original two-stage `%%s` conversion and name/team
insertion, with bounded output and literal percent characters in player names.
Self-capture uses its own string; an unknown capturer prints nothing. Drop prints
nothing. NeedFlag retains strict `last + 20 < client_time`, independent of the
server's four-second gate. Notices use the existing four lines/five-second expiry.
Missing resources stay explicit rather than receiving invented dialogue.

Grab and return play overlapping local CtfFlagGrab WAV one-shots at source
priority, without streaming, loop or reverb. Each has a retained client-owned
handle in the shared 128-voice budget. Completion, preemption and world teardown
release those handles. Capture and flag respawn sounds stay on the existing
logical-server audio stream. Client playback adds no server voice or RNG draw.

## Corrections exposed by verification

A late third player on Cabana initially overlapped an existing player. The authored
marker also overlapped primary geometry, which won the first nearest-hit query
and masked the occupied body. Multiplayer ground entry now repeats the existing
bounded actor separation after the primary-world nudge and before dropping.
It checks four horizontal candidates and their swept routes, moves only the new
player, consumes no additional marker/RNG selection, and still refuses blocked
entry. Solo and AI ordering, runtime collision and CMoveMgr integration are unchanged.
The retained endpoint trace shows the new player separated by 24.001 units.

The first sanitized CTF process rotation also exposed an exhausted weapon ACK
window during slow synchronous map loading. LevelChanging already cancels firing
on both sides, but neutral client weapon frames continued accumulating. Clients
now stop publishing/updating weapon frames during intermission and start a fresh
sequence on the next world. The 64-frame bound is unchanged. Process tests check
that the frame count stays fixed throughout each intermission. General network
stalls outside intermission retain the existing bounded failure policy.

## Verification and remaining scope

Linux validation on 2026-10-31 finishes with **72 distinct native cases, 46
headless ASan/UBSan cases and 72 inspector-free Release cases passing**. The
[check manifest](evidence/m9_ctf_network_checks_2026-10-31.json) records case names,
source/protection hashes and process metrics. Raw [native](evidence/m9_ctf_network_native_2026-10-31.txt),
[sanitized](evidence/m9_ctf_network_asan_2026-10-31.txt),
[Release](evidence/m9_ctf_network_release_2026-10-31.txt),
[build](evidence/m9_ctf_network_builds_2026-10-31.txt) and
[oracle](evidence/m9_ctf_network_oracles_2026-10-31.txt) logs retain the failed
spawn/rotation attempts, their diagnostic probe and successful corrections.

- Synthetic checks cover every truncated packet prefix, malformed/bounded flag
  fields, score deduplication, carried-icon rules, departed identities and stale
  world/mode/team refusal. Existing admission, command/weapon/audio codecs,
  private events, speech, scoreboard and object contracts pass.
- Private retail checks cover original CTF rule cases on all four maps,
  independent localized capture strings, overlapping local sounds, strict notice
  timing and cleanup. Three ENet endpoints verify actual attached flag replicas,
  capture scores/notices, a late join and carrier departure. Existing Team and
  match rotation cases pass. Native and Release also pass ten local CTF cases,
  eight save-session cases and configuration checks.
- Production dedicated processes retain two actual client processes through
  Cabana → Jam → Cabana, with fresh world/player identities and retained teams.
  Both native and sanitized final runs execute 2,100 ticks/four rotations. Native
  discards zero ticks; instrumentation discards **271 ticks** under bounded
  catch-up. This is functional coverage, not performance qualification.
- Graphical native and Release CTF hosts retain two separate clients and their
  local player through rotation. Final runs execute 2,289 ticks/five rotations,
  with **eight native / seven Release discarded ticks**. Client discarded ticks
  remain zero. The final [native](evidence/m9_ctf_network_native_host_final_2026-10-31.png)
  and [Release](evidence/m9_ctf_network_release_host_final_2026-10-31.png) captures
  were visually checked for world/character presentation, team icon, score table
  and intermission text. First captures remain separate immutable artifacts.
- Sanitized dedicated and Release graphical Team combat regressions pass actual
  shots, damage, frag, impulses and USE respawn. These are scripted local process
  encounters with external retail resources, not human LAN play.
- Five fresh solo traces remain byte-identical: effects 240 frames, audio 360,
  movement 360, environment 720 and camera/HUD 300. Effects read external retail
  data; the other four use isolated synthetic fixtures. All **6,891 protected
  hashes** and all **15 source hashes** match. Independent client-process binaries
  contain no server adapter/game session/player/weapon/inventory symbols.

Builds use `cmake --build build/m9-fx-runtime`, `build/m9-fx-runtime-asan` and
`build/release` with the exercised application/server/test targets, followed by
`ctest --test-dir BUILD -R SELECTOR -V`. Graphical cases use native NVIDIA Vulkan
and CTest's isolated user directory/dummy audio. The initial unrestricted native
build was stopped in favor of targeted builds. Initial test aggregate-initializer
warnings were corrected; final incremental builds and formatting checks pass.
Source/documentation diff checks exclude the intentionally verbatim log whitespace.
The sanitized rotation's original queue failure remains a failed attempt in the
record; its successful rerun checks the actual correction, not a relaxed budget.

The capture/drop endpoint test deliberately invokes original server flag rules
and verifies their delivery through real loopback ENet endpoints in one process.
It is not a movement-driven capture by independently running clients. Separate
production dedicated/host processes exercise admission, retained teams and map
rotation with two client processes. Neither qualifies human LAN, all-map combat,
16 players, network-impaired combat or Windows/macOS interoperability. Mobile,
Quest, physical audio/input and XR hardware remain unqualified here.

Client-routed CTF captures, original Host/Join/LAN screens, chat/measured pings,
communication portraits, music, cosmetic FX and remaining multiplayer branches
stay in M9. Solo saves/behavior, logical audio, single CMoveMgr integration, PBR
files and immutable references remain protected. M2-GPU-001 and M3 acceptance
stay independent; this lot does not complete M9.
