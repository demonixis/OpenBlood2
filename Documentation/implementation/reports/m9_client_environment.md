# M9 — local client environment replica

Implemented 2026-10-30 on `804e76ee`, following the authorized
[multiplayer contract](../../contracts/multiplayer_contracts.md#client-environment-replica).
This lot separates liquid, ladder and conveyor membership for local solo movement.
M9 remains in progress; M2-GPU-001 and M3 acceptance are independent.

## Delivered boundary

`ClientCollisionFrame` publishes optional code/current/gravity/viscosity/Hidden
values with the existing owned geometry and generation-qualified collider IDs.
The authoritative metadata extractor is shared by server queries and publication.
Client frame admission validates environment values before replacing any accepted
state. Built-in Container code zero and unsigned 16-bit unknown codes retain their
existing inert semantics; unrecognized container classes have no descriptor.

`ClientCollisionWorld::environment` is an engine-core-only consumer. It computes
body and head membership without server instances, handles, queries or callbacks.
It preserves slot order, hidden/inactive membership, four returned body entries
plus total count, hidden/zero-code first-head masking, exact concave geometry and
already-posed DAT coordinates. It continues to work after server teardown.

CMoveMgr changes only its container value types. Its liquid/ladder/conveyor math,
surface swimming, previous-step braking speeds, category order, currents, gravity,
frame cap and substeps are unchanged. The immediate bridge samples final client
membership before the authoritative air update; air/suffocation still runs once
per capped frame, and landing refreshes server membership itself. Server volume
physics/damage and the original client/server conveyor differences are unchanged.
The player still integrates once; prediction has no gameplay callbacks, and the
server validates the already-integrated destination without adding velocity again.
No save format changes, new dependencies or commercial assets.

The Containers inspector now labels client membership alongside authoritative
membership/air/health and volume diagnostics. Eye offset, character, stance,
camera/fog/weapon point queries and other FX/audio paths still use local services.
The local bridge publishes complete frames before environment and prediction
queries, reusing copied shapes; this adds snapshot CPU work, not integration.
Network transport of environment/geometry, delayed reports, multiple client
processes, LAN and other M9 milestones are not qualified by this lot.

## Verification

The [source ledger](../sources/m9_client_environment_sources.json) records the
original and reconstruction baselines. Linux is the only platform exercised here.

- An engine-core-only receiver checks four-of-six body overlap, separate head,
  hidden and zero-code masking, full built-in codes, concave shells, double-offset
  refusal, invalid metadata, stale identities and world reset. Its symbol table
  contains no ServerAdapter, Objects, CBloodServerShell or original dispatch.
- Local tests compare membership against the authoritative service at 300 positions;
  poison the server's cached list; change ON/Hidden/current/gravity/viscosity without
  moving shapes; remove volumes; detach geometry and destroy the server. Repeated
  client queries cause no motion, air, damage or volume callbacks. One frame still
  produces one authoritative air dose and matched prediction/acknowledgement counts.
- The new private-data test opens SubwayA and CityHub, simulates 120 ticks each,
  compares body/head membership each tick and queries every authored volume's
  bounds centre. Those probes are query-only placements, not traversals of all
  liquid/ladder/escalator regions. The existing 900-tick collision test remains a
  separate scene/movement regression.
- Before implementation, a synthetic harness recorded 180 frames each of Water,
  Ladder, Conveyor and FreeFall (720 total), including currents, run/jump/crouch
  and OFF/ON. Post-change output is byte-identical for pose, velocity, dimensions,
  air, health, container state and movement counts. SHA-256:
  `59e0b11a9e0f563fa8109e444bfdf81dfbe0a117ecd9f487cea3d8907fb2af73`.
  The [trace](evidence/m9_environment_trajectory_2026-10-30.txt) and
  [harness](evidence/m9_environment_oracle_2026-10-30.cpp) are retained. Compile the
  harness with the `movement_tests` compile/link flags and `tests/` include directory;
  its one argument is a temporary fixture directory, with isolated `BLOOD2_USER_DIR`.
- The existing 360-frame dry movement trace also remains byte-identical:
  `25c2932ddcc886dc77262d628c70993c33c186ce65850b26b01f945717f9f144`.
- Clang Debug ASan/UBSan, client disabled: **34/34** targeted synthetic tests pass;
  the private environment test also passes on both retail maps, without diagnostics.

- GCC development build: **183/183** selected headless regressions pass (151
  synthetic, 32 using external retail data), covering movement, containers,
  locomotion, movers/pushes, impulses/death, camera, imprisonment and solo save/load.
  A separate all-containers invocation passes **22/22**, including the two index
  tests outside the selected executable list.
- Linux NVIDIA/Vulkan GPU: **5/5** (`world`, `movement`, `player_restore`,
  `containers`, `conveyor`) pass. Inspector-free Release builds and the 240-frame
  rendered movement smoke pass, exit 0. Both binaries contain the new independent
  environment query; no reference capture is regenerated.
- All **6,185** protected files (the 39 pre-existing untracked PBR files plus existing
  evidence/captures) match their pre-lot SHA-256. Changed C++ formatting, diff
  whitespace, source-ledger JSON and 1,081 local documentation links pass.

The first development test run had 18 aborted container cases. Its `container_tests`
executable predated its newly compiled object (13:44:24 versus 13:44:40 local time).
The build tree also emitted Ninja's existing “premature end of file; recovering”
warning. Both game/core archives had zero empty members. The old executable was
retained under `build/dev/tests/container_tests.m9-environment-stale`, then Ninja's
recorded final link was replayed against the current objects/libraries. All 22
container tests and a complete 183-test rerun then passed without production-code
changes. The underlying incremental-build issue is not diagnosed or claimed fixed.
The initial test-fixture type errors and unsplit concave caps were also corrected;
geometry validation was not relaxed. Initial failures and final logs are retained
in the [evidence](evidence/m9_environment_validation_2026-10-30.txt).

Commands (external data directory and temporary user roots are installation-specific):

```sh
cmake --build build/dev --target blood2_app container_tests \
  network_environment_receiver_tests network_environment_local_tests --parallel 6
# Also rebuild the regression executables listed in the evidence before running the selection.
ctest --test-dir build/dev --tests-from-file /tmp/blood2-m9-environment-test-names.txt \
  --output-on-failure -j6
ctest --test-dir build/dev -R '^gpu\.(world|movement|player_restore|containers|conveyor)$' \
  --output-on-failure -j1
cmake --build build/server-clang-sanitizers --target blood2_server \
  network_environment_receiver_tests network_environment_local_tests --parallel 4
ctest --test-dir build/server-clang-sanitizers \
  -R '^(objects\.|network\.(environment_|collision_|movement_|replica_|pickups|server_clock_audio|server_help))' \
  --output-on-failure -j4
BLOOD2_USER_DIR=/tmp/blood2-m9-env-asan-private \
  build/server-clang-sanitizers/tests/network/network_environment_local_tests private /path/to/Blood2
cmake --build build/release --target blood2_app --parallel 4
BLOOD2_USER_DIR=/tmp/blood2-m9-env-release-user build/release/blood2 \
  --config build/release/tests/empty.ini --data-dir /path/to/Blood2 \
  --movement-smoke --capture /tmp/m9-environment-release.png
```

CTest uses isolated user directories. The sanitizer build disables the client and
inspector; Release disables the inspector. The evidence lists all selected tests
and executables so the temporary test-name file can be reproduced.

## Limits and preservation

These synthetic and private-data Linux checks are not retail executable equivalence,
complete campaign traversal, human LAN play or Windows/macOS interoperability.
Mobile/Quest, XR hardware and impaired-network qualification were not run. No
simulator/emulator was launched. Dedicated sanitizer compilation retains the
headless boundary, but this lot adds no multiplayer gameplay or admission.

Existing PBR files and immutable evidence/captures are checked separately by
SHA-256 and excluded from these commits. No renderer/shader or reference file under `sources/blood2/`
is changed. The root PBR guidance addition also remains outside this lot.
