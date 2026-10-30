# M9 — Initial ClientWorld snapshot over ENet

2026-10-30, following `2613231d`. An admitted Bloodbath client now receives
its initial observable object world on the reliable Baseline channel. Two
independent client processes decode their own DAT, models and sprites and feed
the existing CPU presentation inputs from ClientWorld. This is an initial frame,
not continuous network gameplay or a graphical join mode.

The shared read-only capture preserves the local publisher's object/creation
order, poses, flags, color/alpha, animation and sprite tracks, hidden nodes,
attachments, world-model bindings, sky and PolyGrid/stream owner identities.
It advances no simulation, callback, sound or random state. Network records
contain explicit values and resource names, never original handles, decoded
assets or GPU objects. See the [source ledger](../sources/m9_world_baseline_sources.json).

Protocol **version 2** supersedes admission-only version 1. A snapshot is bounded
to **1 MiB**, split into reliable packets of at most **32,768 bytes** with a
30-byte world/sequence/length/offset header. The receiver keeps one bounded
assembly, requires exact offsets and matching metadata, and commits only a
complete validated snapshot. Control and Baseline arrival order is not assumed.
The client waits for both Joined and the baseline and checks its player identity.

The separate `blood2_client_network` module resolves assets from client-owned
stores, validates model animation/node and sprite-frame bounds, and checks
world-model names/indices against its own DAT. Resource failure leaves the
previous ClientWorld intact. Retained frames keep their decoded assets after
server/world teardown. An attached child whose parent is pending removal can
retain its last resolved pose while its missing parent hides it, as in solo.

## Verification

[Linux validation](evidence/m9_world_baseline_validation_2026-10-30.json):
**10/10 GCC headless cases**, **6/6 Clang ASan/UBSan cases** and the native
NVIDIA Vulkan **gpu.player_restore** regression pass. Client and server compile.
The codec tests cover exact round trips, every truncation, invalid fields and
identities, count/byte budgets, reordered/duplicate/stale chunks, atomic refusal
and 25,000 deterministic mutated or malformed inputs.

All **22 mounted multiplayer maps** encode and decode their complete initial
object frame with independent client asset resolution. Re-encoding yields exactly
the original bytes; client model/sprite pointers differ from the server's, and
model presentation remains usable after server unload. The largest snapshot is
**45,425 bytes** (CTF Jam). These are data/replica tests, not CTF network admission.

The actual console server and two independent client processes reconstruct
Crypt's initial ClientWorld and model scene, exchange heartbeats and leave
cleanly under both GCC and sanitizers. Each client verifies data independently.
The sixteen-peer UDP test also consumes each baseline and receives a fresh one
on reconnect after 180 ticks. That load test shares one test process. Neither
the client probe nor standalone asset receiver links ServerAdapter, the original
server shell, GameSession or the object registry.

All five solo traces remain byte-identical: 240 FX, 360 audio, 360 movement,
720 environment and 300 camera/HUD frames. The **6,316 protected files**, including
PBR and prior immutable evidence/GPU references, retain their hashes.
The initial process test failure is retained: the parent still searched for the
old `Joined player` diagnostic after its children successfully printed `Replicated`.
The assertion now checks the new diagnostic alongside successful child/server
exit and zero-rejection/departure accounting. No suppression was introduced.

## Remaining work

Only the joining client receives this initial object frame. Existing clients
do not yet receive later joins, object mutations, poses or destructions.
Continuous reliable membership, 20 Hz state publication, interpolation and
cross-frame retired-identity tracking remain next. Collision/environment,
private player/HUD events, logical audio and effect recipes also need wire
delivery. The regular graphical client, remote movement/combat, death/respawn,
Team/CTF rules, playable host and LAN UI remain in progress.

This lot does not qualify Windows/macOS interoperability, mobile/Quest builds,
human LAN play or degraded-network gameplay. No simulator/emulator was used.
M2-GPU-001 and M3 acceptance remain independent.
