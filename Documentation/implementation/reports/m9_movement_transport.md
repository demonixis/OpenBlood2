# M9 — movement transport and server advancement

2026-10-30, following `d5bda522`. The server now accepts addressed movement over
ENet. The graphical/independent CMoveMgr client still needs its setup and correction
receiver; this is not yet an interactive multiplayer delivery.
See the [contract](../../contracts/multiplayer_contracts.md) and
[source ledger](../sources/m9_movement_transport_sources.json).

## Behavior

Protocol **6** adds explicit little-endian movement messages: reliable private
setup (**98 bytes**), unreliable client report (**104 bytes**) and unreliable
server acknowledgement (**106 bytes**), each bounded by a 256-byte schema limit.
The setup contains numeric physics, dimensions, stance/eye height, movement gate,
position and force identity. Reports include controls/aim and the position,
velocity and acceleration already computed by the client. No pointer, C++ layout,
asset or server object is serialized.

Only a joined peer on State can submit a report. The endpoint binds it to that
peer's admitted player and checks world, object generation, force generation,
move code and increasing report sequence. Wrong recipients/malformed messages
disconnect the sender. Old identities, duplicates and reordered reports are
ignored without refreshing liveness or consuming a newer sequence. Each player
keeps one pending report; a newer report replaces it before simulation. This
bounds movement work independently of receive frequency.

The network force generation advances when either the original player
`movement_revision` or the engine's `pose_revision` changes. It is independent of
the original eight-bit code and observes engine teleports that do not emit an
original force message. Refresh clears pending reports and old acknowledgements,
and resets velocity/acceleration once, like the local client force response.
Multiple force changes before publication coalesce to the latest destination.
No original solo code, gameplay teleport path or RNG draw is changed.

The 60 Hz ServerSession adds its player stage after the existing model/physics
updates and before container damage/deferred cleanup. Generic physics continues
to skip client-associated players. A report tick invokes the existing checked
position handler and does **not** integrate another delta. Between reports, the
server moves received velocity through two existing swept substeps per tick,
without applying gravity, acceleration or CMoveMgr commands a second time.
Original animation/clearance retains controls each tick; door-push consumption,
air and held-model updates stay server-owned. Movement gates neutralize controls;
reported gravity/impulse motion still follows the selected client-authored model.

Extrapolation is explicitly bounded to **30 ticks / half a second** without a new
report. Afterwards the server holds velocity and uses neutral controls until a
fresh report. This is an adaptation for bounded stale movement, not a claim to
reproduce undocumented retail extrapolation. It does not replace heartbeat timeout.

ACKs capture the actual accepted position/velocity/dimensions at report receipt,
before later velocity advancement. They repeat with 20 Hz publication; using the
later extrapolated pose as the correction would add another velocity/time offset.
Setup changes are reliable. A teleport during report contacts invalidates that
report's ACK rather than assigning it the new force generation. World snapshot
publication and the immediate solo movement path retain their previous clocks.

## Verification

**53 distinct native Linux tests pass**, including the real Vulkan player-restore
check; **18 ASan/UBSan headless tests pass**. The final animation/force-generation
changes also pass the four affected server/UDP/process cases in each build.
Client and dedicated targets build without new production warnings.

The codec checks exact message sizes/header bytes, atomic refusal, every truncation
and **30,000 seeded mutations**, alongside existing admission/world codec tests.
The native broad run initially had 52/53 passes because the admission golden still
expected protocol 5; updating its explicit version byte to 6 makes both focused
codec tests pass. No image reference or immutable capture was changed.

The retail Crypt fixture verifies no extra displacement on a report tick, exactly
two velocity substeps on the next tick, no repeated acceleration/gravity, stable
receipt-time ACKs, latest-report coalescing, stale extrapolation hold, generation
checks and the original 256-code wrap. A report aimed beyond a primary-world wall
receives a collision-limited position, never the historical teleport fallback.
A pending report is invalidated by a subsequent force; a direct engine teleport
also invalidates reports without changing the original player movement revision.
The initial fixture had sent before original first-update initialization's extra
force message; its failed log remains and the fixture now settles that update
before testing accepted motion.

Two actual ENet peers in one process verify private setup, movement acceptance,
addressed ACKs, duplicate refusal and rejection of another player's identity.
The existing 16-peer observer fixture still passes, as does the actual dedicated
executable with two independent observer processes. Those separate processes do
not yet drive CMoveMgr; the new movement UDP test uses explicit typed reports.
No degraded-network combat or collision traversal qualification is claimed.

All five solo traces are byte-identical: 240 effects, 360 audio, 360 movement,
720 environment and 300 camera/HUD frames. All **6,425 protected hashes** remain
unchanged, including PBR files, immutable GPU references and previous evidence.
The temporary effects trace helper retains its existing indentation warning.

Commands use the existing isolated native and sanitizer build configurations:

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_movement_codec_tests network_server_movement_tests network_admission_tests network_admission_process_tests network_world_snapshot_tests network_world_snapshot_private_tests network_world_state_tests network_world_receiver_tests network_wire_collision_tests network_admission_codec_tests network_movement_local_tests network_replica_movement_tests movement_tests player_restore_tests container_tests -j6
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_)|^movement\.|^player_restore\.|^gpu\.player_restore$|^containers\.' -E minefield -j3
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.(movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_)' -j3
```

Raw logs and traces use `evidence/m9_movement_transport_*`. Retail resources remain
external. Setup/ACK application to CMoveMgr, client correction history, complete
private HUD/camera/audio/effect event delivery, remote firing/death/respawn,
Team/CTF and host/join UI remain in the active M9 goal. The existing legacy event
queues retain their bounds; this movement channel does not drain or discard them.
Windows/macOS interoperability, human LAN and mobile/Quest remain unqualified.
M2-GPU-001 and M3 acceptance remain independent.
