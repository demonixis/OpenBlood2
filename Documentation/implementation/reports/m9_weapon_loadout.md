# M9 — private weapon loadouts and remote view models

2026-10-30, following `3726ce16`. Dedicated and playable-host clients now receive
private weapon state and render original draw/rest animations through the independent
CViewWeapon runtime. This lot does not submit firing or selection commands. See the
[contract](../../contracts/multiplayer_contracts.md) and
[provenance](../sources/m9_weapon_loadout_sources.json).

## State and ownership

Protocol 9 appends private tag 22. Its 76-byte packet contains the existing
world/player/object identity, event sequence and server tick, followed by the full
movement force revision, character/slot, both weapon types, primary/alternate ammo
per hand, liveness, input/fire permissions and binocular state. The codec uses
explicit little-endian values and bounded finite fields; no pointers are transmitted.
Admission refuses previous protocols.

ServerSession reads original inventory/CheckAmmo and player state after refreshing
movement/camera gates. An unchanged loadout emits no event. Camera/Seeing Eye gates
input; death, attributes, movement blocking/imprisonment and binoculars additionally
gate firing. The snapshot does not let a client choose permissions or ammunition.
The local solo service path is unchanged.

NetworkWeapons prepares both changed view models, character skins and held resources
before replacing either hand. It follows the existing SyncWeapons rule: type or
character changes recreate a view; ammo changes and same-type replacements retain
animation state. Owner/world/sequence/full-force checks prevent stale application;
resource failure leaves both previous views intact. Readiness requires the current
movement force, and a new world clears both views.

The original runtime advances once per normal 60 Hz client tick. Its own clock is
anchored at the first accepted loadout and then advances by the fixed step; later
snapshots, render frames and movement replay never advance or rewind it. This lot
passes neutral firing input. Actual remote weapon sound output remains unavailable:
requests are counted and return an explicit failure, not a fabricated played voice.
ClientScene receives borrowed view pointers for the existing renderer. No gameplay
callbacks run in ClientWorld and no second movement integration was introduced.

## Verification

**21 distinct native tests and 9 server-only ASan/UBSan tests pass after updating
the two protocol fixtures.** The initial native run failed the old admission byte
fixture and the private-message catalogue; the sanitizer run failed that catalogue.
They still expected protocol 8 and 21 alternatives. The fixtures now explicitly
assert protocol 9 and include tag 22, retaining all corruption/truncation checks.
The initial failures and passing reruns remain in the evidence.

New tests cover all packet truncations, oversized/trailing/invalid data, incompatible
version, 5,000 byte mutations, dual views, mode-specific ammunition, atomic missing
resources, character/death state, same-type animation retention, stale identities,
force transitions and reset. Original server inventory tests check two isolated
players, dual Berettas, M16 primary/alternate ammo and camera/imprisonment/binocular
gates. The updated private catalogue additionally exercises all 22 alternatives.

Separate ENet client processes receive Beretta state with 50 rounds and run 361/61
weapon animation updates during observer/visitor stays, with zero submitted shots.
Both passive admission and moving-client runs pass; server-derived movement reports,
corrections, camera/HUD ownership and arrival/departure remain checked. The independent
receiver executable has no ServerAdapter, GameSession, server-shell, Objects, CWeapon
or CInventoryMgr method symbols.

Native SDL/GPU process checks pass for a dedicated server with two clients, a playable
host with a remote client, and host shutdown. Dedicated ran 2,400 ticks, admitted and
removed both players, with zero rejects/timeouts/state drops. Host ran 2,706 ticks
without discarded server ticks; shutdown ran 545. Client overload counters remain
visible (one tick in the dedicated visitor, eight in the shutdown visitor). Captures
were visually inspected: the original Beretta appears in the remote first-person
view with the world and original HUD. These captures are observations, not immutable
GPU reference replacements.

The independent weapon-services synthetic/retail cases, existing solo weapon
animation/lifetime/rollback cases and GPU weapon smoke pass. All **6,644** protected
PBR, immutable-capture and earlier-evidence hashes are unchanged. No solo original
weapon/movement/audio implementation or renderer algorithm changed in this lot.
Linux qualification only; no claim of network firing, degraded combat, cross-platform
interop, mobile/XR, human LAN, Team/CTF or whole-M9 completion. M2-GPU-001 and M3 remain
independent.

Commands and full outputs: `evidence/m9_weapon_loadout_*`.

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_admission_process_tests network_frontend_process_tests network_admission_codec_tests network_client_endpoint_tests network_player_event_tests network_server_player_event_tests network_camera_wire_tests network_server_camera_tests network_client_movement_tests network_weapon_loadout_tests network_server_weapon_loadout_tests -j6
ctest --test-dir build/m9-fx-runtime -R '^network\.(weapon_loadout_wire|weapon_loadout_private|server_weapon_loadout|admission_codec|player_events|server_player_events|camera_wire|server_camera|client_movement|client_endpoint|admission_processes|movement_processes)$|^gpu\.network_(dedicated|host|shutdown)$' --output-on-failure -j2
cmake --build build/m9-fx-runtime --target network_player_event_tests network_admission_codec_tests network_weapon_services_tests network_weapon_local_tests weapon_tests -j6
ctest --test-dir build/m9-fx-runtime -R '^network\.(admission_codec|player_events|weapon_services|weapon_local_)|^gpu\.weapons$' --output-on-failure -j2
cmake --build build/m9-fx-runtime-asan --target blood2_server network_admission_process_tests network_client_endpoint_tests network_player_event_tests network_camera_wire_tests network_client_movement_tests network_weapon_loadout_tests network_server_weapon_loadout_tests -j4
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.(weapon_loadout_wire|weapon_loadout_private|server_weapon_loadout|player_events|camera_wire|client_movement|client_endpoint|admission_processes|movement_processes)$' --output-on-failure -j2
cmake --build build/m9-fx-runtime-asan --target network_player_event_tests -j4
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.player_events$' --output-on-failure
```
