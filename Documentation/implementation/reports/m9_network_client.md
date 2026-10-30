# M9 — reusable independent network client

2026-10-30, following `36be746d`. NetworkClient now owns the working admission,
replication, movement, private-event and camera path previously exercised inside
the separate-process fixture. It creates no GameSession or original server object.
The graphical host/join frontend and remaining remote gameplay/presentation
consumers remain subsequent M9 work. See the [contract](../../contracts/multiplayer_contracts.md)
and [provenance](../sources/m9_network_client_sources.json).

## Behavior

One endpoint lifetime is one admission. It mounts/fingerprints its own retail
resources, decodes its own DAT/models/sprites and owns ENet, world/collision
reception, CMoveMgr prediction, HUD inbox and camera. A reconnect constructs a new
endpoint and receives a new identity. Initial baseline and Joined may arrive in
either channel order. The existing codecs and bounds remain protocol 8.

The state machine bounds each pump to 64 transport events. It checks phase,
channel, owner and decoding before applying messages, has 5-second connect,
30-second offer/baseline and 15-second active inactivity deadlines, and allows
two seconds for graceful departure. Rejections retain their server reason;
malformed/order failures close explicitly. The bounded private inbox must be
drained by its presentation caller. Camera/PlayerDead, authoritative rotation,
zoom and HUD are handled exactly once; remaining events stay typed for consumers.

Client prediction uses 60 Hz steps, at most six catchup steps, counted discarded
ticks and the unchanged original first-frame/0.14-second report timer. A setup
waiting for collision references is retained and retried. Existing force/ACK
reconciliation remains the same. Neutral commands do not stop prediction,
networking or camera clocks. Look uses the existing desktop ±1.48-radian limit
and camera restrictions. Weapon fire is still absent from this endpoint.

NetworkClock anchors only newer server stamps, accounts for half the measured
ENet RTT and never returns backwards presentation time. Repeated/older packets
cannot restart the anchor. Silence caps advancement at 250 ms beyond the latest
server stamp; distant poses use a 100 ms delay and the existing bounded sampler.
The sampled ClientWorld now carries its requested presentation time; the
receiver's authoritative timestamp is unchanged. Local body motion stays
predicted and camera selection uses generation-qualified replicas.

## Verification

**8/8 tests pass natively and 8/8 under ASan/UBSan.** They cover the new clock,
real ENet rejection/order/timeout handling, existing world reception/pose
impairment matrix, movement reconciliation, camera consumer, and two actual
client processes against the dedicated. The process fixture now uses NetworkClient
instead of its duplicated endpoint implementation and still checks arrival,
departure, HUD/startup events, predicted eye, reports/corrections and clean server
counters. It includes both neutral and driven client variants.

The clock fixture checks duplicates, old stamps, invalid/backwards local clocks,
bounded silence, no backwards reanchor and 1,000 alternating RTT/jitter samples.
This tests clock arithmetic, not combat under impaired networking. The negative
endpoint fixture uses a real ENet listener with private local retail data; it
checks bad packet/order refusal, timeout and retained content-rejection text.

`nm -C` finds no GameSession, ServerAdapter, CBloodServerShell or Objects methods
in either process-client or negative-endpoint executable. All **6,586** protected
PBR files, captures and historical evidence match their pre-edit hashes. Solo
runtime code is unchanged; this lot does not rerun the preceding lot's five solo
oracles or GPU comparisons. No reference output was regenerated. Linux only;
no Windows/macOS, hardware XR/mobile, human LAN or complete gameplay acceptance
is claimed. M2-GPU-001 and M3 remain independent.

Commands (complete outputs under `evidence/m9_network_client_*`):

```sh
cmake --build build/m9-fx-runtime --target blood2_server network_client_endpoint_tests network_client_clock_tests network_admission_process_tests network_world_state_tests network_world_receiver_tests network_client_movement_tests network_camera_wire_tests -j6
ctest --test-dir build/m9-fx-runtime -R '^network\.(client_endpoint|client_clock|admission_processes|movement_processes|world_state|world_receiver|client_movement|camera_wire)$' --output-on-failure -j3
cmake --build build/m9-fx-runtime-asan --target blood2_server network_client_endpoint_tests network_client_clock_tests network_admission_process_tests network_world_state_tests network_world_receiver_tests network_client_movement_tests network_camera_wire_tests -j4
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.(client_endpoint|client_clock|admission_processes|movement_processes|world_state|world_receiver|client_movement|camera_wire)$' --output-on-failure -j3
```
