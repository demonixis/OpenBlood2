# M9 — reliable private player events

2026-10-30, following `bff2046c`. Protocol 7 delivers the original private player
messages to independently linked clients. HUD statistics and inventory values now
apply on the receiving client. Other presentation events reach an owned inbox;
connecting all original audio, camera/effect and shell consumers remains separate.
See the [contract](../../contracts/multiplayer_contracts.md) and
[source ledger](../sources/m9_private_events_sources.json).

## Behavior

The B2PE codec has 20 explicit tags, a 47-byte identity/sequence/tick header and a
2,048-byte packet limit. Integers/floats use checked little-endian fields; text,
optional items, booleans, enums, numeric values and object generations are bounded.
No C++ layout, pointer or commercial resource is transmitted. Receivers reject
truncated, oversized, unknown, invalid and trailing data atomically. Admission's
protocol version is 7; an older peer fails the existing protocol gate.

ServerSession captures events synchronously through an optional ServerAdapter
sink. Pointer-bearing Eye, Orb, creature-attachment and CTF messages become checked
ObjectIds while the original objects still exist, including deferred destruction.
Queued identity values survive retirement of the server handles. An unresolved
required identity is refused; no address is cast into a network ID. The sink is
absent from ordinary local games and is cleared before ServerSession teardown.

Admission already processes PlayerInitRequest, and reliable movement setup handles
PlayerPhysics/PlayerPosition with full force generations. These three legacy
messages are counted as subsumed, never replayed as a second initialization or
force. Other original event order is retained without coalescing item-show,
announcer, death, zoom or sound events. Each player has an independent sequence and
256-event bound. Endpoint publication waits for baseline admission, then sends the
owned events on reliable Control. ENet's existing per-peer byte budget still
applies; a failed send removes that peer. Queue overflow remains an explicit
service refusal, not unbounded storage or silent success.

The client inbox validates recipient/world/object generation, exact reliable
sequence and monotonic source tick. Duplicate and other-owner events do not apply.
It has the same 256-event bound and a take operation for presentation consumers.
ClientHud receives statistics/items exactly once, including each ITEMF_SHOWITEMS
request. The inbox retains the other typed events; receiving a voice/camera event
alone is not a claim that its final remote presentation is connected.

Original null-client broadcasts for conversations, objectives and boss health now
reach every associated player. A saturated recipient does not suppress delivery
to the others; the aggregate service result retains the first failure. Original
objective game-mode filtering and dialogue playback rules are unchanged. This
corrects the previous one-client adapter restriction; it does not add network chat
input or change the private Bloodbath startup announcement.

## Verification

**66 distinct native Linux tests pass**, including Vulkan player restore;
**31 ASan/UBSan headless tests pass**. The broad runs initially had one failure:
the movement codec's explicit golden header still expected protocol 6. Updating
that version byte to 7 gives 4/4 focused codec/event passes in each build. Both
failed broad logs remain in evidence; no GPU reference was changed.

Client and dedicated builds have no new production warnings. All five solo traces
are byte-identical: 240 effects, 360 audio, 360 movement, 720 environment and 300
camera/HUD frames. All **6,488 protected hashes** are unchanged, including PBR,
immutable captures and previous evidence. The temporary effects oracle retains
its existing indentation warning.

Commands use the existing isolated build configurations:

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_player_event_tests network_server_player_event_tests network_admission_tests network_admission_process_tests network_admission_codec_tests network_movement_codec_tests network_server_movement_tests network_client_movement_tests network_movement_local_tests network_replica_movement_tests network_world_snapshot_tests network_world_snapshot_private_tests network_world_state_tests network_world_receiver_tests network_wire_collision_tests movement_tests player_restore_tests container_tests player_connections_tests network_player_profile_tests network_announcer_tests -j6
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(client_movement$|movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_|player_|server_player_events$|announcer)|^movement\.|^player_restore\.|^gpu\.player_restore$|^containers\.' -E minefield -j3
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.(client_movement$|movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_|player_|server_player_events$|announcer)' -j3
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(movement_codec|admission_codec|player_events|server_player_events)$'
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.(movement_codec|admission_codec|player_events|server_player_events)$'
``` Raw evidence uses
`evidence/m9_private_events_*`; retail files remain external.

The standalone client fixture covers all 20 tags, golden header bytes, round-trip
values, every truncation, trailing data, invalid identities/flags/text and 30,000
seeded mutations. Inbox checks cover addressed HUD, duplicate/item-show behavior,
sequence gaps, stale clocks/owners/worlds, bounded queues and fresh admission.
The retail fixture verifies one original startup announcement per player,
identity retention after referenced-player destruction, private HUD/items,
all-player broadcasts, delivery past a saturated recipient and cleanup.

Actual ENet fixtures and the dedicated/two-process clients now consume private
messages while continuing movement and world replication. The child clients check
positive private health and exactly one startup announcement each. Audio playback,
remote camera selection, effect delivery, firing/death/respawn, Team/CTF and
interactive hosting/joining still belong to the active M9 work. These results do
not qualify degraded-network combat, Windows/macOS interoperability, human LAN or
mobile/Quest. M2-GPU-001 and M3 acceptance remain independent.
