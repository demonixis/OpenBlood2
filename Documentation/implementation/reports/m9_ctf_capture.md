# M9 — Client-routed CTF capture and bounded weapon waiting

This lot adds an actual movement-driven flag capture on retail **Jam**, followed
by the original score-limit intermission and rotation to Cabana. The production
dedicated/graphical host and each remote client run in separate processes. It
extends the [CTF network lot](m9_ctf_network.md), whose capture test deliberately
stimulated the original rules on the server. Protocol **18** is unchanged.

## Client-routed capture

The independent test client reads flag models and poses from ClientWorld. A
bounded test-only ground route search probes copies of its body against the
client collision replica. It uses 32-unit nodes, eight-unit collision probes,
existing slope/stair support and bounded drops; no server objects, callbacks,
teleports, game commands or commercial asset changes drive the capture. Search
work is split between client updates. The planner is not production navigation
and has no jump, crouch or ladder strategy.

Only ordinary look, forward and run inputs move the real player through
CMoveMgr. The test requires an unchanged force revision, no death or shots, and
observed stair steps. Original server contacts grab the enemy flag, attach its
real model, and capture it on reaching the home stand. A second independent
client joins while the flag is carried and verifies its attachment and owner.
Both clients receive exactly one capture with five individual/team points. The
carrier alone receives the grab sound/icon; capture clears the icon and reaches
the configured five-point limit. The original intermission rotates the map;
both peers retain teams, receive fresh identities and zero scores, then leave
cleanly. The graphical host has its own third, idle local player.

This proves one scripted route on Jam. Early standalone planner probes could
not find a ground-only route across DMX; that is a planner limitation, not evidence
of broken retail geometry or game collision. No production collision was relaxed
and other CTF routes remain unqualified. The host screenshot is the final Cabana
view after rotation, not a capture-time flag HUD or a human-operated encounter.

## Slow-server input window

The first ASan process attempt failed before reaching the flag: the client had
sent 838 weapon frames while the server had acknowledged 774, exhausting the
64-frame window during normal play. This was a bounded-queue failure, not an
ASan memory finding. The prior intermission-only suppression does not cover a
server whose normal simulation advances slower than the client's fixed clock.

When the existing window is full, NetworkClient now waits for credit rather than
disconnecting. Only weapon publication and its matching animation timeline pause.
Movement/reporting, camera, audio, transport and normal timeouts continue. There
is no deferred queue of skipped input: the next available frame samples current
held buttons and aim. Already admitted frames retain their checked order and
original animation-derived fire behavior. Sequence exhaustion and malformed
ACKs remain errors. Server cadence, six-frame catch-up, 64-frame bounds, loadout
history and all gameplay validation are unchanged. A cumulative waiting-tick
counter is exposed in the endpoint and graphical session log.

A real ENet endpoint regression stops server simulation for four simulated
seconds while pumping transport, holds fire/movement, releases during the stall,
then runs the server at one-third speed for four seconds. It checks the bound
every update, frozen weapon time/update counts while full, continuing movement
reports, recovery and no later shots from discarded held input. Existing queued
frames produce four legitimate shots. All three builds record 338 waiting ticks.
This is deterministic clock-controlled overload coverage, not an RTT/loss matrix.

## Verification

Linux checks on 2026-10-31 finish with **52 distinct native, 33 headless
ASan/UBSan and 52 inspector-free Release cases passing**. The
[manifest](evidence/m9_ctf_capture_checks_2026-10-31.json) lists exact cases and
hashes. Raw [native](evidence/m9_ctf_capture_native_2026-10-31.txt),
[sanitized](evidence/m9_ctf_capture_asan_2026-10-31.txt) and
[Release](evidence/m9_ctf_capture_release_2026-10-31.txt) logs retain initial and
final attempts, including the failed ASan window exhaustion.

| Final CTF process case | Server ticks | Discarded server ticks | Runner reports / stairs | Runner weapon waiting ticks |
| --- | ---: | ---: | ---: | ---: |
| Native dedicated | 6,000 | 0 | 384 / 52 | 0 |
| ASan/UBSan dedicated | 6,000 | 1,688 | 598 / 47 | 1,115 |
| Release graphical host | 6,029 | 11 | 393 / 50 | 0 |

Every row completes one capture-driven rotation and clean departures, with zero
rejected/timed-out peers where dedicated counters are exposed. Instrumented
simulation is substantially slower; its successful result is functional coverage,
not performance qualification. The ASan observer also waits 889 weapon ticks.
Release's local host discards zero client ticks and waits zero weapon ticks.
The first and final Release [captures](evidence/m9_ctf_capture_release_host_final_2026-10-31.png)
were visually checked for Cabana world, view weapon, statistics and team icon.
The [first image](evidence/m9_ctf_capture_release_host_first_2026-10-31.png) stays
immutable and separate. Rendering uses native NVIDIA Vulkan, dummy audio and
isolated CTest user directories; physical audio/input is not tested.

Regression coverage includes reliable weapon ACKs, checked fire/cadence/loadouts,
weapon sounds, client/server movement and impulses, commands, endpoint timeout/
malformed handling, CTF rules/presentation on all four maps, Team admission and
rotation. Native and Release also pass ten local CTF cases and eight save-session
cases. Actual native dedicated Bloodbath combat and Release graphical Team
combat with two remote processes pass lethal damage, one frag and USE respawn.
The client-routed CTF binary links no ServerAdapter, GameSession, CPlayerObj,
CWeapon or CInventoryMgr symbols in any of the three builds.

Five fresh [solo traces](evidence/m9_ctf_capture_oracles_2026-10-31.txt) are
byte-identical to the previous lot: effects 240 frames, audio 360, movement 360,
environment 720 and camera/HUD 300. Effects read external retail data; the other
four use new isolated synthetic fixtures. All **6,902 protected hashes** match,
including existing PBR files and immutable evidence. The unchanged external
retail content fingerprint is recorded in the manifest.

[Build logs](evidence/m9_ctf_capture_builds_2026-10-31.txt) cover targeted
`cmake --build build/{m9-fx-runtime,m9-fx-runtime-asan,release} --parallel 3
--target ...`, then `ctest --test-dir BUILD -R SELECTOR -V` (two parallel jobs
for the short regression set, serial process/GPU cases). The final project
builds and formatting checks pass. The external effects oracle retains its
existing misleading-indentation warning; it is not a production diagnostic.
The Release test helper initially encountered an unbuilt executable while
reading CTest metadata, then resolved its target from the generated test file
and built it before executing tests. That tooling error is not a passing test.
Documentation/code diff checks exclude verbatim log whitespace.

[Standalone planner diagnostics](evidence/m9_ctf_capture_probes_2026-10-31.txt)
retain DMX no-route outcomes and the preliminary Jam success, omitting verbose
NODE search dumps. They neither certify those failed routes nor change the
production movement contract. Commercial geometry/data remain external.

## Remaining M9 scope

The selected client-routed capture does not qualify all four CTF routes, all 22
multiplayer maps, 16-player load, impaired combat, human LAN or Windows/macOS
interoperability. Original Host/Join screens, LAN discovery, chat/measured pings,
communication portraits, music and cosmetic network FX remain in M9. Mobile,
Quest, physical audio/input and XR hardware remain separately unqualified.
Solo, logical server audio/RNG, single CMoveMgr integration, PBR files and immutable
GPU references remain protected. M2-GPU-001 and M3 acceptance stay independent.
