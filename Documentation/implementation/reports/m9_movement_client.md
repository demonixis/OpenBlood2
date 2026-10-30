# M9 — independent client movement correction

2026-10-30, following `edc30d84`. The independent client now consumes private
movement setup/ACKs and runs original CMoveMgr through ENet. The dedicated plus
two separate client processes can move and correct their predicted bodies. This
is a headless movement delivery; the interactive client, weapons and remaining
private presentation events still belong to the active M9 work.
See the [contract](../../contracts/multiplayer_contracts.md) and
[source ledger](../sources/m9_movement_client_sources.json).

## Behavior

`NetworkMovement` owns CMoveMgr and ReplicaMovement, with no server game class.
Reliable setup applies numeric physics, stance/eye height, movement gates and
full force identity. Force changes snap immediately, reset velocity/acceleration
through the existing force service and discard old correction history. Original
CMoveMgr counters survive an in-session force; its jump/container force resets
stay unchanged. The original byte code can wrap without reviving old ACKs.

Stance changes keep the client's feet anchored and check the new box against
client-owned collision. If an older standing setup would grow into a ceiling the
client has already reached, keep the checked smaller body until an ACK supplies
a corresponding authoritative position and dimensions. Setup never copies an
ordinary stale server pose into local prediction. Invalid setup is refused;
unavailable owner/collision defers it without replacing active state.

Every frame runs CMoveMgr once and records its inputs and value state. Reports
retain the first-frame/0.14-second countdown and monotonically increasing
sequence. An ACK restores the checkpoint of its report, installs the server's
receipt-time motion, then replays only later client frames against the current
collision replica. Subsequent report checkpoints are updated by that replay.
This is an explicit reconciliation adaptation, not a claim about undocumented
retail network code. No server callbacks, air/damage, audio or report timer run
during replay. Logical movement counters count predicted frames once; separate
replay counters expose the extra client computation.

History is bounded to 512 input frames and 32 report checkpoints; evicted reports
are retired. Duplicate/old ACKs, old force generations and other owners cannot
rewind the body. Unsent sequences/wrong current codes are refused. A future-setup
ACK has one bounded pending slot. Changed physics/stance retires history rather
than replaying old frames with new constraints; an identical newer setup retains
useful history. A correction embedded in newer collision is deferred, including
when there are no later frames. Failed prediction/replay restores the previous
client state; the ACK remains retryable. Collision queries remain callback-free.

CMoveMgr's arithmetic, local services, server advancement, protocol version 6,
solo clocks and presentation paths are unchanged. A value checkpoint groups its
existing fields and private readiness/input state; it copies no body reference,
service or original server object. Client jump voice output is still pending.

## Verification

**55 native Linux tests pass**, including Vulkan player restore; **20 ASan/UBSan
headless tests pass**. Client and dedicated targets build without new production
warnings. The temporary effects oracle retains its pre-existing indentation warning.

All five solo traces are byte-identical: 240 effects, 360 audio, 360 movement,
720 environment and 300 camera/HUD frames. All **6,460 protected hashes** are
unchanged, including PBR files, immutable GPU references and previous evidence.
The first native three-process movement run recorded 40 reports/39 corrections
for the observer and 7 reports/6 corrections for the visitor. The dedicated
completed 900 ticks with two admissions/two departures, no rejected or timed-out
peers, no discarded ticks and no State drops.

Commands use the existing isolated native and sanitizer configurations:

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_client_movement_tests network_admission_process_tests network_movement_codec_tests network_server_movement_tests network_admission_tests network_world_snapshot_tests network_world_snapshot_private_tests network_world_state_tests network_world_receiver_tests network_wire_collision_tests network_admission_codec_tests network_movement_local_tests network_replica_movement_tests movement_tests player_restore_tests container_tests -j6
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(client_movement$|movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_)|^movement\.|^player_restore\.|^gpu\.player_restore$|^containers\.' -E minefield -j3
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.(client_movement$|movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_)' -j3
```
Raw evidence uses `evidence/m9_movement_client_*`. Commercial data stays external.

The independent fixture compares immediate and delayed corrections exactly in
air/ground, water, ladders and conveyors, including a second ACK after the first
replay. It checks one integration, cumulative ACK counters, force/code wrap,
foot-anchored stance, blocked standing, obstructed corrections, failure rollback,
future setup ordering, bounded history and stale owner/world refusal.

The process fixture launches the actual dedicated executable on retail Crypt,
then two separately hashed/loaded clients with independent DAT/collision/model
state. Each drives CMoveMgr at 60 Hz, sends its reports and applies addressed
ACKs while observing player arrival/departure. This is actual ENet loopback, not
an impaired-network or human LAN qualification. Neither client executable links
ServerAdapter, CBloodServerShell, GameSession or the original server object set.

Complete private HUD/camera/audio/effect delivery, external gameplay commands,
remote firing/death/respawn, Team/CTF and the interactive host/join frontend remain
pending. Windows/macOS interoperability, human LAN and mobile/Quest remain
unqualified. M2-GPU-001 and M3 acceptance stay independent.
