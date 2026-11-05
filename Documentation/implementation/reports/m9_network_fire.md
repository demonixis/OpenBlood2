# M9 — animation-validated network fire

2026-10-31, following `0651140d`. Protocol v11 now transports weapon input through
the independent client and dedicated server. Actual fire runs through original
CPlayerObj/CInventoryMgr/CWeapon, with checked identity, selection, ammunition,
permissions and animation cadence. The graphical frontend still submits neutral
weapon input until multiplayer death/slow death/respawn is ported; this is a
verified combat prerequisite, not an accepted playable match.

## Original behavior and adaptation

Original CViewWeapon owns draw/rest/start/fire/end, reload thresholds, semiautomatic
edges, alternate loops, shotgun barrels, bomb charge and FIRE_KEY markers. Original
CWeapon::Fire calculates a reload interval but does not enforce it. Reusing only
that server entrypoint would therefore accept arbitrary rapid shots. A second
approximate cooldown would mishandle real multi-key/dual/alternate animations.

The new server sequencer runs the same CViewWeapon in AnimationOnly resource mode:
ABC clips and keys are retained; skins, flashes and sprite textures are not loaded.
Its SOUND_KEY does not create a logical server voice. Actual accepted CWeapon fire
still uses the existing logical sound service. Default presentation creation and
the synchronous solo bridge are unchanged.

A client sends one 64-byte B2WF frame on reliable Control per normal 60 Hz weapon
tick: full world/player/object/force identity, stream sequence, the sequence of a
server-issued private loadout, held primary/alternate bits and bounded aim. It
never sends ammunition, damage, reload time, seed, target or origin. Rendering and
movement correction replay send no weapon frames. Menus/focus loss keep sending
neutral ticks. CMoveMgr receives the firing flag without another integration.

The server retains at most 128 issued loadouts. The referenced sequence must exist
and never go backwards. Two nonzero per-hand view revisions expand loadouts from
76 to 92 bytes; published type/character changes increment only the affected hand.
Ammo and force changes retain animation. An observed A → B → A transition still
recreates A; adding a left Beretta does not reset the right hand.

Both animation timelines consume the referenced server-issued ammo/permissions,
the same fixed deltas and the same clock origin. This preserves prediction when
ammo or permission delivery is delayed. Original CheckAmmo reads HUD state; its
Fire's local temporary ammo subtraction does not update that HUD. The predicted
Fired result therefore advances presentation only. Each actual delivery separately
rechecks the current player, force/pose revision, relevant view revision, character,
weapon, life, movement/camera/Eye/binocular permissions and real ammunition. The
origin is the current checked body plus eye height. Current server simulation time
owns damage/projectiles/audio; there is no hit rollback or lag compensation.

Commands and weapon frames share an ordered queue. Selection cannot overtake a
frame waiting for budget. Bounds are 32 queued commands plus undrained command ACKs,
64 weapon frames, at most six weapon frames per simulation tick, and one frame of
credit per server tick capped at 60. This one-second delayed-input allowance is a
bounded burst recovery policy, not competitive lag compensation. No unlimited idle
credit is banked. Malformed/future/unknown references and capacity violations reject
the peer; duplicate frames never execute twice. Old force/view frames can advance
the proof timeline but cannot fire in the current life/view.

A 71-byte latest State ACK reports processed sequence, actual shot totals per hand
and authoritative refusals. Client counters reject impossible future sequences and
regressions; an ACK never grants health/ammo or moves the body. State may arrive
before its reliable force setup. Cosmetic shots and actual shots remain separate.
Future replicated FX still need their own shot identity/deduplication.

## Verification

The wire fixture covers explicit LE bytes, every truncation, invalid identities,
flags, angles and protocol versions, atomic output and 20,000 mutations. Loadout
checks include zero revisions, changed types without revisions, retained right-hand
animation and same-type recreation after an observed intermediate change.

The retail server test executes real Beretta animation-driven damage against a
fixture owning the original damage aggregate, checks ammo and server eye origin,
and verifies duplicate suppression, current blocking, stale forces, dry weapons,
A → B → A history, dual alternate-to-primary behavior, the 64-frame queue and
six-frame processing budget. A selection behind that queue stays ordered; weapon
frames alone perform zero server movement steps. The fixture ends with 33 actual
right-hand shots and 36 authoritative refusals; it does not qualify lethal PvP.

Two real ENet client processes each execute three actual Beretta shots against the
world, observe the matching 50 → 47 ammo and shot ACKs, then disconnect normally.
The dedicated runs 900 ticks with two admissions/departures and no rejection,
timeout or discarded tick in this native run. Existing moving/static process,
command, private state, admission and endpoint checks remain separate coverage.

All 39 retail weapon modes compare presentation versus AnimationOnly for 720 ticks
each: states, animation timing/revision, barrel/bomb/Voodoo state, FIRE_KEY counts,
595 predicted fire requests and their seeds/hand/mode/special values agree. This is
animation/request parity, not authoritative multiplayer qualification of every
weapon. Four characters and both Beretta hands retain their existing checks.

**30 distinct native tests and 17 ASan/UBSan tests pass.** The native set
includes all eight save-session cases, independent camera/movement cases, the
solo GPU weapon smoke and a real graphical host/remote run. The host capture was
visually checked for its world, Beretta, HUD and crosshair; its hash is recorded
without replacing any baseline. The sanitizer configuration builds the server
without client/renderer/inspector targets.

Five rebuilt solo runtime oracles remain byte-identical: effects (240 frames),
audio (360), movement (360), environment (720) and view (300). All 6,682 protected
hashes remain unchanged, including existing PBR files and immutable references.
The independent process client contains no ServerAdapter, CBloodServerShell,
GameSession, Objects, CWeapon or CInventoryMgr symbols. Final production/test builds
have no new warning; the external effects-oracle helper retains its known
misleading-indentation warning.

[Check inventory/capture hash](evidence/m9_network_fire_checks_2026-10-31.json),
[native process/animation details](evidence/m9_network_fire_process_verbose_2026-10-31.txt),
[sanitizers](evidence/m9_network_fire_asan_tests_2026-10-31.txt),
[GPU](evidence/m9_network_fire_gpu_tests_2026-10-31.txt),
[save and replica regressions](evidence/m9_network_fire_regression_tests_2026-10-31.txt),
[solo oracles](evidence/m9_network_fire_solo_oracles_2026-10-31.txt).

Commands used (isolated native and sanitizer trees):

```sh
cmake --build build/m9-fx-runtime -j6 --target blood2_app blood2_server network_server_fire_tests network_weapon_frame_tests network_admission_process_tests network_weapon_loadout_tests network_server_weapon_loadout_tests network_weapon_services_tests network_weapon_local_tests network_player_command_tests network_server_player_command_tests network_admission_codec_tests network_player_event_tests network_client_endpoint_tests save_session_tests network_camera_wire_tests network_client_movement_tests
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(weapon_frames|server_fire|fire_processes|admission_processes|movement_processes|weapon_loadout_wire|weapon_loadout_private|server_weapon_loadout|weapon_services|weapon_services_private|weapon_local_animation|weapon_local_lifetime|weapon_local_rollback|player_commands|server_player_commands|admission_codec|player_events|client_endpoint|camera_wire|client_movement)$'
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^save_session\.|^gpu\.(network_host|weapons)$'
cmake --build build/m9-fx-runtime-asan -j4 --target blood2_server network_server_fire_tests network_weapon_frame_tests network_admission_process_tests network_weapon_loadout_tests network_server_weapon_loadout_tests network_weapon_services_tests network_weapon_local_tests network_player_command_tests network_server_player_command_tests network_admission_codec_tests network_player_event_tests
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.(weapon_frames|server_fire|fire_processes|admission_processes|movement_processes|weapon_loadout_wire|weapon_loadout_private|server_weapon_loadout|weapon_services|weapon_services_private|weapon_local_animation|weapon_local_lifetime|weapon_local_rollback|player_commands|server_player_commands|admission_codec|player_events)$'
```

The tests above ran in several bounded groups; the linked inventory counts unique
tests, not repeated executions. No Windows/macOS interoperability, human LAN, impaired combat matrix,
22-map interactive sweep or 16-player combat qualification is claimed here.

## Remaining work

Finish multiplayer death, slow death, respawn and frags before enabling frontend
held fire. Then connect remote audio/FX, finish weapon-specific multiplayer branches,
match limits/rotation, Team/CTF and LAN/menu flows. Existing PBR files, immutable
references, solo services, logical audio and M2-GPU-001/M3 boundaries stay separate.

[Source/adaptation ledger](../sources/m9_network_fire_sources.json).
