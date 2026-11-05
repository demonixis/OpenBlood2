# M9 — reliable player commands and remote zoom

2026-10-30, following `04c7bdf8`. Remote clients now select/cycle weapons, issue USE,
operate/cycle inventory, toggle alternate sniper zoom and request remote-bomb
detonation through checked server dispatch. Existing bindings supply the graphical
frontend. Network weapon firing remains pending. See the
[contract](../../contracts/multiplayer_contracts.md) and
[provenance](../sources/m9_player_commands_sources.json).

## Boundary and rules

Protocol 10 adds B2PC on the reliable ordered Control channel. Requests are 57 bytes;
acknowledgements are 48 bytes. Both carry full world/player/object/force identity and
a command sequence. Ten explicitly encoded actions admit only their bounded arguments;
USE alone carries finite aim angles. It carries no origin, hit object or arbitrary
server command. Invalid decoding leaves the prior output intact.

The server queues requests without executing gameplay in the network pump. It applies
them after authoritative movement in the existing simulation tick, refreshing camera
and force state before each command. A command whose force expired while queued gets
one negative ACK. Duplicates and old world/object identities never dispatch; sequence
gaps, malformed directions/channels and excessive queues are rejected. At most 32
requests plus undrained ACKs exist for one server player. The independent client tracks
at most 32 outstanding requests and requires each ACK to match the oldest pending
identity/sequence. Normal gameplay refusal does not terminate the connection.

Original handlers own the result: slot selection and cycling use CInventoryMgr;
USE casts from the authoritative body/eye and applies the original door exclusion;
items retain run, movement, camera and active-Eye exceptions; detonation marks the
original owner's remote bombs. Empty/current in-range slot requests still cancel
zoom. Dead-player commands are refused here; multiplayer dead USE/respawn is subsequent
work. The item and weapon messages already feed private HUD/loadout/camera state.
No CMoveMgr, solo gameplay handler, movement integration or server RNG algorithm changed.

NetworkControls consumes the existing command/Press bindings in the original order.
A held USE/item/alternate button creates one rising command. Menu, focus and loading
suppression prevents held presses from reappearing on resume; queued slot commands
are consumed while blocked. This applies to the existing keyboard/mouse/gamepad
binding layer, without adding a device-specific protocol.

ZoomView now also drives the client's FOV at the original eight radians/second toward
90 or 12.5 degrees, with external-camera/Orb/letterbox rules. The renderer receives
that FOV, the HUD uses zoom crosshair 98/99, and mouse look uses existing quarter
sensitivity. Remote zoom sounds and item light/audio effects still need consumers.
Firing, drop/grab/taunt, creature escape, death/respawn, scores and match rotation remain
separate deliveries; this is not completed Bloodbath gameplay.

## Verification

**19 distinct native tests and 12 server-only ASan/UBSan tests pass.** Native includes
the seven later GPU/solo checks; the sanitizer set covers the twelve network cases.
Client/server and server-only sanitizer builds succeed. Independent client symbols
contain no GameSession, ServerAdapter, server shell, Objects, CWeapon or CInventoryMgr
methods. All **6,658** protected PBR, immutable-capture and earlier-evidence hashes
remain unchanged.

The command codec exercises ten actions, both ACK results, explicit byte layout,
all truncations, incompatible protocol, malformed arguments and 18,000 mutations.
Input tests hold presses for 240 frames and cross menu/focus boundaries. Server
checks cover deferred tick application, per-player isolation, duplicate USE, weapon
cycling, empty-slot zoom cancellation, queued teleport/force refusal, movement blocks,
binocular ownership/run restrictions, wrong recipients, departure and request/ACK
capacity including undrained acknowledgements. A pre-admission ACK is refused by the
actual endpoint. Existing movement, camera, loadout and private-message checks pass.

Separate client processes each complete nine commands with eight positive ACKs and
the intended empty-slot refusal: knife, Beretta, sniper, alternate zoom, empty-slot
unzoom, Beretta, USE, next item and previous item. Both FOV limits and crosshair 99
are observed through real ENet. The sequence passes in stationary and moving runs,
natively and under sanitizers. Each dedicated run admits/removes two players with
zero rejects/timeouts/state drops and no discarded server ticks. No shots are sent.

The native graphical host/remote smoke and the solo GPU weapon smoke pass. Existing
independent weapon-services synthetic/retail tests and solo animation/lifetime/rollback
also pass. The graphical smoke checks normal presentation; zoom behavior is checked
through the real headless clients, not a new zoom pixel reference or human session.

Two corrections arose during verification. The initial empty-slot fixture used the
original multiplayer fallback (weapons 1–9), so slot 9 was occupied; it now requests
an explicit sparse test profile, without changing production defaults. Then both
real-process runs exposed valid authored spawn yaw above pi. The new USE codec had
incorrectly required normalized angles. It now matches the existing typed original
finite bound of 1e6 and tests an authored yaw above pi plus excessive/NaN values.
The failed runs and passing reruns remain in the evidence.

Linux only. No new Windows/macOS interoperability, mobile/XR, degraded combat,
16-player load, human LAN or complete M9 qualification. M2-GPU-001 and M3 remain
independent. Commands and outputs: `evidence/m9_player_commands_*`.

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_player_command_tests network_server_player_command_tests network_admission_process_tests network_admission_codec_tests network_player_event_tests network_client_endpoint_tests network_camera_wire_tests network_client_movement_tests network_weapon_loadout_tests network_server_weapon_loadout_tests -j6
ctest --test-dir build/m9-fx-runtime -R '^network\.(player_commands|server_player_commands|admission_codec|player_events|client_endpoint|camera_wire|client_movement|weapon_loadout_wire|weapon_loadout_private|server_weapon_loadout|admission_processes|movement_processes)$' --output-on-failure -j2
ctest --test-dir build/m9-fx-runtime -R '^network\.(player_commands|server_player_commands|admission_processes|movement_processes)$' --output-on-failure -j2
ctest --test-dir build/m9-fx-runtime -R '^network\.weapon_(services|local_)|^gpu\.(network_host|weapons)$' --output-on-failure -j2
cmake --build build/m9-fx-runtime-asan --target blood2_server network_player_command_tests network_server_player_command_tests network_admission_process_tests network_admission_codec_tests network_player_event_tests network_client_endpoint_tests network_camera_wire_tests network_client_movement_tests network_weapon_loadout_tests network_server_weapon_loadout_tests -j4
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.(player_commands|server_player_commands|admission_codec|player_events|client_endpoint|camera_wire|client_movement|weapon_loadout_wire|weapon_loadout_private|server_weapon_loadout|admission_processes|movement_processes)$' --output-on-failure -j2
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.(player_commands|server_player_commands|admission_processes|movement_processes)$' --output-on-failure -j2
```
