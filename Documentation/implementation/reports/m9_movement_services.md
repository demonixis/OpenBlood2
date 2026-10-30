# M9 — independent CMoveMgr and replica prediction

2026-10-30, following `789fee33`. This is a completed client movement prerequisite
inside the active multiplayer goal, not yet a remotely controlled player.
See the [contract](../../contracts/multiplayer_contracts.md) and
[source ledger](../sources/m9_movement_services_sources.json).

## Implementation

`blood2_client_movement` now builds the existing original `CMoveMgr` separately
from `blood2_game`. Its value-based `MovementServices` interface contains no
server shell, connection handle or voice manager. No movement arithmetic changes.
The solo adapter still supplies controls/stance, authoritative state before each
substep, collision prediction, one checked submission, immediate correction,
jump voices and the final server air update in the same order. The client shell
resets CMoveMgr and binds its local connection explicitly during entry.

`ReplicaMovement` supplies the other implementation, using only an admitted
`ClientCollisionWorld` and its own motion body. It checks the world/object
generation and force identity at binding, predicts through the existing swept
solver and queries copied liquid/ladder/conveyor values. Repeated substeps do not
reset the predicted position to an older server snapshot. Neither prediction nor
environment queries mutate colliders or run server contacts, air or damage.

The adapter prepares value reports with world/object identity, full force
revision, monotonically increasing report sequence, move code, accepted predicted
position/velocity/acceleration, surface and controls. Original
`BloodClientShell.cpp:1751–1758` sends on the first frame and resets a float
countdown to 0.14 seconds after each publication, discarding overshoot. The new
report function preserves that behavior and is called separately with real frame
duration; it does not reuse CMoveMgr's capped 0.1-second physics duration or emit
catch-up bursts. Disabled controls become neutral in the report.

This adapter currently accepts initial stance/eye height and a checked force
position. Private setup/stance updates, report encoding/transmission, authoritative
remote velocity advance, acknowledgement/correction history, and remote jump
voice playback remain to connect. The force-position call is an internal service;
the future wire receiver must validate full force revisions before invoking it.
No new network protocol version or wire admission behavior is introduced here.

## Verification

Native Linux GCC RelWithDebInfo builds client and dedicated targets. **56 distinct
tests pass**: 55 headless checks and the real Vulkan `gpu.player_restore` test.
This combines the 39-test movement/replica/admission/restore selection with all
20 existing container tests (three overlap). The ASan/UBSan headless build passes
**19/19** targeted tests. Logs retain the initial missing direct header include
failure and the subsequent successful builds; no gameplay workaround was added.

The new standalone executable links only client movement and engine services.
Symbol inspection finds no ServerAdapter, CBloodServerShell, GameSession or server
object registry. It runs the actual CMoveMgr at 30 and 60 Hz, verifies identical
120-substep movement, wall stopping, jump/landing, liquid/ladder/conveyor behavior,
unchanged collider poses, retained prediction across an older pose publication,
invalid move-code/NaN refusal, vanished-owner invalidation and force-position
reset. The report test observes seven publications over 60 frames, world/force
identities and no burst after a long frame. These are synthetic movement checks,
not remote gameplay or degraded-network collision qualification.

The existing 16-peer admission fixture and actual dedicated/two-process observer
test also pass. Their clients still observe replicas without sending movement.
All five solo traces remain byte-identical: 240 effects, 360 logical/client audio,
360 movement, 720 environment and 300 camera/HUD frames. All **6,403 protected
file hashes** remain unchanged, including PBR files and immutable GPU/evidence
references. The temporary effects trace helper retains its existing indentation
warning; final production builds introduce no warning.

Reproduction uses the existing isolated `build/m9-fx-runtime` and
`build/m9-fx-runtime-asan` configurations:

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_replica_movement_tests movement_tests network_movement_local_tests network_admission_tests network_admission_process_tests network_collision_local_tests network_environment_local_tests network_replica_local_tests player_restore_tests container_tests -j6
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(replica_movement|movement_|collision_|environment_|replica_local|admission_)|^movement\.|^containers\.(liquid|ladder|conveyor)|^player_restore\.|^gpu\.player_restore$' -j3
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^containers\.' -E minefield -j3
cmake --build build/m9-fx-runtime-asan --target network_replica_movement_tests network_movement_local_tests network_environment_local_tests network_collision_local_tests network_admission_tests -j4
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.(replica_movement|movement_|collision_|environment_|admission_codec$|admission_private$)' -j3
```

Raw evidence uses the `evidence/m9_movement_services_*` prefix. Private tests use
external retail data; no commercial content is imported. Windows/macOS, human
LAN, mobile/Quest, combat, death/respawn, Team/CTF and host/join UI remain in the
active M9 continuation. M2-GPU-001 and M3 acceptance remain independent.
