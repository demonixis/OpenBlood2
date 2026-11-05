# M9 — Reliable remote player impulses

2026-10-31, following `47473243`. Original damage and door-push velocity now reach
independent network CMoveMgr through protocol v13. Dead players retain passive
movement reports and checked velocity-only server advancement. This is a movement
boundary delivery; real-process lethal PvP remains the next gate before graphical
held fire, followed by remote audio/FX and the remaining match modes/rules.

## Source and adaptation

Original CDestructable::ApplyDamagePhysics adds normalized damage direction times
`damage * 2500 / mass` to the server body and accumulates a player-only delta.
PlayerObj::UpdateClientPhysics consumes this delta, combines the once-consumed
door push and writes PSTATE_ADDVELOCITY. Original CMoveMgr adds that vector to its
own physics body. It does not run another movement step on receipt. The original
position-message handler has no dead-player exclusion.

This port retains the successful server SetVelocity and enables accumulation only
for players owned by ServerSession. The synchronous solo bridge continues to read
the changed body before its existing substeps; it never receives another delta.
Refused SetVelocity does not enqueue an impulse. Damage ordering, health/armor,
server random draws, CMoveMgr arithmetic, force-position semantics and the solo
save format remain unchanged. Door deltas compose with damage exactly once.

The original unbounded pending vector is validated at publication. Each peer has
at most 128 unacknowledged deltas. An invalid vector, exhausted sequence or full
backlog refuses that peer; it cannot stop the other players or dedicated ticks.
Checked force-position retires old pending deltas, report ACKs and impulse history.
A new life starts a new impulse sequence. Existing 0.5-second stale-report holding
still bounds extrapolation, including for dead players.

The [provenance ledger](../sources/m9_network_impulses_sources.json) identifies
source selections and explicit adaptations. This does not intercept every generic
SetVelocity: reports, collision clipping and respawn also write velocity. The
original Leech/Singularity projectile direct velocity writes have no explicit
PSTATE_ADDVELOCITY publication in those source branches; their multiplayer behavior
and the remaining trapped/creature branches still need their weapon-specific audit.
The existing already-dead aggregate damage refusal stays unchanged.

## Protocol and prediction

MovementImpulse is a 59-byte reliable Control message containing the complete
world/player/object/force identity, monotonic impulse sequence and finite bounded
velocity delta. Report and ACK packets grow to 112 and 114 bytes with an LE u64
highest-applied impulse sequence; setup remains 98 bytes. All four messages retain
bounded platform-independent encoding and atomic malformed/truncated decoding.
Version 13 admission refuses older protocol peers.

A client adds each reliable impulse once to its current predicted velocity,
without changing position, resetting the 0.14-second reporting countdown or
executing CMoveMgr. It retires pre-impulse correction checkpoints so an old ACK
cannot erase or replay the delta. New report checkpoints support ordinary delayed
correction/replay. ACKs naming a not-yet-received reliable impulse are deferred;
once that impulse arrives the earlier report is retired. Setup may itself await
collision membership, so NetworkClient holds a bounded queue until setup applies.
Old-life impulses are ignored; gaps, invalid vectors and impossible combined
velocities are refused.

The server retains unacknowledged deltas. A report sent before an impulse includes
only its earlier velocity, so missing deltas are added to that reported velocity
before the checked move. This restores an overwritten value, rather than adding
the delta again to the current body. A report acknowledging the impulse carries
its already-dragged/collision-adjusted velocity and receives no extra addition.
The ACK records the impulse state included at acceptance. No velocity*time step
runs on a report tick; only intervening ticks perform the existing two swept
velocity-only substeps. Collision rejection never falls back to teleportation.

Movement commands remain neutral while damage-dead. The client still computes
gravity, drag and existing momentum; the server accepts its checked destination
and advances received velocity between reports. Confirmed death/slow death and
weapon cancellation keep their previous boundaries.

## Verification

**46 distinct native tests and 13 ASan/UBSan tests pass.** The
[check inventory](evidence/m9_network_impulses_checks_2026-10-31.json) lists every
case and preserves failed attempts. The [native regression run](evidence/m9_network_impulses_native_tests_2026-10-31.txt)
covers four solo impulse cases, ten movement cases, eight save-session cases,
three solo death cases, protocol/private/weapon codecs, authoritative fire/death,
admission and three independent-process modes. The [GPU/client run](evidence/m9_network_impulses_client_gpu_tests_2026-10-31.txt)
adds original solo weapons and graphical host with a separate client. Its capture
was visually checked for world, Beretta, crosshair and HUD; no reference was replaced.

The new server fixture checks additive damage+door deltas, old/new reports,
monotonic acknowledgement, actual DoDeath followed by passive reports, force
retirement and isolated capacity failure. The existing server-movement fixture
now pushes toward a real retail wall and verifies clipping. Generated client
fixtures check exactly-once delta addition, no position/update/clock mutation,
duplicate/gap/nonfinite/old-owner refusal, ACK-before-impulse ordering, replay,
neutral death commands and force reset. Four wire kinds exercise all truncations
and 40,000 deterministic mutations; the previously stale movement header golden
is now checked against the selected protocol.

The real ENet probe delivers **140 addressed impulses while all State deliveries
are discarded**, then verifies that only the non-acknowledging peer is rejected.
The [real NetworkClient fixture](evidence/m9_network_impulses_client_final_2026-10-31.txt)
checks the accumulated push, exact CMoveMgr update count, movement arc, later
correction, server history retirement, force reset and clean departure. Its
[ASan/UBSan counterpart](evidence/m9_network_impulses_asan_client_final_2026-10-31.txt)
passes, alongside [eight movement/session cases](evidence/m9_network_impulses_asan_tests_2026-10-31.txt)
and [four other protocol cases](evidence/m9_network_impulses_asan_codecs_2026-10-31.txt).

Three fixture assumptions were corrected with the failed logs retained: cleanup
initially drained a departed player; the jump check sampled after bb_crypt's
−2000 authored gravity had returned the player near its initial height; the
sanitized client demanded a best-effort ACK within that short arc. It now verifies
the arc and allows bounded later ACK publications. The server-only sanitizer tree
does not define the solo player_impulse target; its attempted selection is retained
and those four cases pass natively. None of these attempts is reported as a pass.

[Five rebuilt solo traces](evidence/m9_network_impulses_solo_oracles_2026-10-31.txt)
are byte-identical to the death lot: effects 240, audio 360, movement 360,
environment 720 and view 300 frames. All **6,719 protected SHA-256 values** remain
unchanged, including existing PBR files and immutable evidence/captures. The
independent process-client binary has no ServerAdapter, server shell, GameSession,
Objects, CWeapon or inventory-manager symbols. Product builds have no new warnings;
the unchanged external FX oracle helper still emits its known indentation warning.

Commands use the isolated client-enabled native and server-only sanitizer trees:

```sh
cmake --build build/m9-fx-runtime -j6 --target blood2_app blood2_server network_impulse_client_tests network_server_impulse_tests network_server_movement_tests network_client_movement_tests network_movement_codec_tests network_admission_tests network_admission_process_tests network_admission_codec_tests network_player_event_tests network_player_command_tests network_weapon_frame_tests network_server_fire_tests network_server_death_tests network_client_endpoint_tests player_impulse_tests player_death_tests movement_tests save_session_tests
ctest --test-dir build/m9-fx-runtime -V --output-on-failure -R '^network\.(movement_codec|client_movement|server_impulses|server_movement|impulses_udp|impulse_client|server_death|server_fire|admission_codec|player_events|player_commands|weapon_frames|weapon_ack_udp|admission_private|movement_udp|client_endpoint|fire_processes|admission_processes|movement_processes)$|^player_impulse\.|^player_death\.|^movement\.|^save_session\.'
ctest --test-dir build/m9-fx-runtime -V --output-on-failure -R '^network.impulse_client$|^gpu\.(network_host|weapons)$'
cmake --build build/m9-fx-runtime-asan -j4 --target blood2_server network_impulse_client_tests network_server_impulse_tests network_server_movement_tests network_client_movement_tests network_movement_codec_tests network_admission_tests network_admission_process_tests network_server_death_tests network_admission_codec_tests network_player_event_tests network_player_command_tests network_weapon_frame_tests
ctest --test-dir build/m9-fx-runtime-asan -V --output-on-failure -R '^network\.(movement_codec|client_movement|server_impulses|server_movement|impulses_udp|impulse_client|server_death|weapon_ack_udp|fire_processes|admission_codec|player_events|player_commands|weapon_frames)$'
```

The evidence retains the actual separate invocations and final corrected fixture
runs; this combined listing describes the complete selected set.

Tests distinguish generated geometry/codec probes, external retail original-code
fixtures, real ENet loopback delivery and actual independent processes. Dropping
all State deliveries at the test consumer is a channel-independence probe, not a
complete latency/loss/jitter qualification. A human LAN match, Windows/macOS
interop, 22-map two-player combat and 16-player combat are still unqualified.
Original graphics/PBR files and immutable captures remain protected; this lot
does not alter M2-GPU-001 or M3 acceptance.
