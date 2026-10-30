# M9 — local client cameras and HUD

Implemented 2026-10-30 on `e14bd081`, following the authorized
[camera/HUD contract](../../contracts/multiplayer_contracts.md#local-client-cameras-and-hud).
This is an immediate solo-local separation. M9 remains in progress; M2-GPU-001 and
M3 acceptance remain independent.

## Delivered boundary

`ClientSession::camera` owns player-eye/view/listener state, original death orbit,
desktop chase state and passive camera descriptors. World/sequence/identity, pose,
clock, camera type, duplicate slots and the 4,096 descriptor bound are checked
before replacing a received frame. Renderer and XR camera views use generation-
qualified IDs. The original shell keeps references into this state for existing
input/event handlers; camera selection no longer reads original objects.

The local publisher copies player pose/eye height/alive state, checked owned Eye/Orb
targets and camera settings in original descriptor creation order. Selection retains
all source priorities, visibility, listener and look/input gates. Initial camera
settings remain distinct from subsequent source Setup changes and from the original
server active-listener global. Eye view FOV, zoom fallback and XR third-person
refusal are unchanged.

Death/chase query an independent copied collision scene. The ray kernel retains
primary-world and object tie order, attached objects, solid/ray-hit flags, object
interior skipping and the VolumeBrush versus built-in Container distinction. The
five-ray chase envelope, aim convergence and death orbit/transition arithmetic are
unchanged. Shared orientation math is extracted verbatim from the ObjectRecord node
adapter; this removes the otherwise hidden original-dispatch link dependency.

Only the local command bridge mutates server permissions/Eye rotation. It checks
world/object/sequence and aim, rejects replays and changed camera selections, and
recomputes camera movement/USE/fire gates from the authoritative objects. Rendered
camera state cannot clear these gates. No camera operation integrates player motion;
CMoveMgr remains unchanged, with one integration and immediate authoritative correction.

`ClientSession::hud` owns stats/inventory and receives addressed, sequenced values.
The server and receiver reuse the previous field validation, including all **13**
inventory slots although only ten normal slots appear in the status bar. Duplicate
item events cannot replay the slide-in request. Typed values, show/message counters,
maxima, crosshair/CTF settings, artwork/layout and source event timing are retained.
Both frontends now consume the common `HudSnapshot`, using a published client clock
and world identity. Other original client notices remain in the shell; this does
not create previously absent HUD screens. Inspection shows camera/HUD identities,
sequences, local clock, camera command counts and publication errors.

The static source weapon catalog and crosshair helpers now link with the CPU client
consumer. No dependency, save payload, original-source reference file or shader is
changed. The [source ledger](../sources/m9_client_view_sources.json) records the
baseline files and original references.

## Verification

Linux is the only platform executed for this lot. Synthetic checks and private
retail-data checks are distinct;
none qualifies multiplayer admission, multiple clients or human LAN play.

The engine-core-only receiver covers camera priority, listener/visibility/look
rules, death/chase/zoom/XR selection, invalid/stale frames, duplicate/excess camera
descriptors, addressed HUD values, reserved slots, invalid stats/item paths, duplicate
show requests and world replacement. Its symbol table has no ServerAdapter, Objects,
CBloodServerShell or original object dispatcher.

Local tests cover server-derived camera gates even after rendered state is changed,
replay/wrong-world/changed-selection refusal, HUD ownership, entry/exit/re-entry,
pure camera/HUD queries without server service calls or player integration, and
camera/HUD continuation after the server is destroyed. A deterministic set of 6,000
client/server ray comparisons covers all three object modes, both start modes,
VolumeBrush exclusion and built-in containers, checking identity, exact hit fraction,
point, normal, polygon and material. Original selected-weapon HUD values arrive on
the first playing tick, as before; the tests preserve this startup ordering.

The private test runs 900 unchanged 1/60-second ticks on each of SubwayA and CityHub,
checking original scripted-camera poses, HUD values/clock and movement prediction
acknowledgements, then exercising chase selection after the opening sequence. This
is opening/short-scene coverage, not campaign traversal or retail executable parity.

The 300-frame synthetic camera/HUD trace captured **before** this lot is byte-identical
with the new consumer: body/eye/view/listener poses, camera/visibility/permission
flags, HUD stats, death-orbit state and movement counts. It exercises chase,
walk/jump, a scripted camera and the death transition. SHA-256:
`a2ddcc0bb1b170373c36574ab272c407f0523f73ac087562a6d339528caa14e9`.
The [trace](evidence/m9_client_view_trajectory_2026-10-30.txt) and
[harness](evidence/m9_client_view_oracle_2026-10-30.cpp) are retained. Compile with
the `movement_tests` compile/link flags and `tests/` include directory; pass a
temporary fixture directory and isolate `BLOOD2_USER_DIR`.

The previous 360-frame dry-movement and 720-frame Water/Ladder/Conveyor/FreeFall
traces also remain byte-identical, including pose, velocity, air/health and movement
counts. Their SHA-256 values remain respectively
`25c2932ddcc886dc77262d628c70993c33c186ce65850b26b01f945717f9f144`
and `59e0b11a9e0f563fa8109e444bfdf81dfbe0a117ecd9f487cea3d8907fb2af73`.
Neither original evidence file was regenerated.

Recorded results:

- GCC development: **250/250** selected headless tests pass (204 synthetic, 46
  external retail-data cases), covering movement/environment, cameras, inventory,
  death/impulses, Orb, HUD, controls, pure XR player input and solo save/load.
  The separately rebuilt Museum skylight scene also passes (**1/1**, private data).
- Linux NVIDIA/Vulkan: **12/12** GPU cases pass: world, movement, containers,
  conveyor, cameras, player restore, HUD, screen fade, CTF HUD, third person,
  synthetic flare and retail flare. Release without inspector builds and its
  rendered movement/unload/re-entry smoke exits 0. No reference capture is replaced.
- Clang Debug ASan/UBSan with client/inspector disabled: **38/38** targeted
  synthetic tests pass. The new private view test also passes 900 ticks on each
  retail map without diagnostics.
- Final review preserves the device XR mode across world reset, matching the
  previous shell. After that adjustment, the rebuilt camera/view/XR-player subset
  passes **18/18**, the sanitizer view subset **4/4**, the private sanitizer scene
  test and Release rendered smoke pass again. The 300-frame trace is still exact.
  These are CPU XR tests, not headset execution.
- The engine-only receiver symbol audit remains free of server/object dispatch.
  All **6,188** protected PBR/evidence files match the pre-lot SHA-256 snapshot.
  Formatting of 52 changed C++ files/ranges, diff whitespace, 16 source-ledger
  hashes and 1,453 local links pass the final checks.

The development tree again emitted Ninja's “premature end of file; recovering”
warning. Two link attempts found a zero-byte `save_fingerprint.cpp.o` member in
`libengine_core.a`, despite the compiled object being nonempty. Broken archives
are retained in the ignored build directory. Repacking from the compiled objects
and replaying Ninja's recorded final links produced the tested binaries. Final
small rebuilds used the same compiler/link flags directly (without an absent
module mapper for the previously unbuilt skylight test); the sanitizer rebuild
uses ordinary CMake/Ninja. The underlying development-build issue is neither
diagnosed nor claimed fixed.

Initial tests remain visible in the [validation record](evidence/m9_client_view_validation_2026-10-30.txt):
the new synthetic HUD fixture needed the original first playing tick; the old
third-person fixture used a scripted CameraObj as a fake Eye/Orb and now checks
that it cannot impersonate a remote view. Actual owned Eye/Orb tests pass. A
missing direct `<algorithm>` include in the flare GPU test was restored after
removing a transitive include. An overbroad XR test pattern selected nine unbuilt
math/input executables (Not Run); the final 18-test selection names only the
rebuilt camera/view/player executables. No failed or unrun invocation is counted
as passing.

Commands (the validation record contains the full executable/test selection):

```sh
cmake --build build/dev --target blood2_app network_view_receiver_tests \
  network_view_local_tests camera_tests hud_tests --parallel 6
# Rebuild the other selected regression targets listed in the evidence as well.
ctest --test-dir build/dev --tests-from-file /tmp/blood2-m9-view-test-names.txt \
  --output-on-failure -j6
ctest --test-dir build/dev -R '^gpu\.(world|movement|player_restore|cameras|hud|screen_fade|third_person|ctf_hud|containers|conveyor)$' \
  --output-on-failure -j1
ctest --test-dir build/dev -R '^(gpu\.flare(_private)?|skylight_spawn\.museum)$' \
  --output-on-failure -j1
cmake --build build/server-clang-sanitizers --target blood2_server \
  network_view_receiver_tests network_view_local_tests --parallel 4
ctest --test-dir build/server-clang-sanitizers \
  -R '^(objects\.|network\.(view_|environment_|collision_|movement_|replica_|pickups|server_clock_audio|server_help))' \
  --output-on-failure -j4
BLOOD2_USER_DIR=/tmp/blood2-m9-view-asan-final-user \
  build/server-clang-sanitizers/tests/network/network_view_local_tests private /path/to/Blood2
cmake --build build/release --target blood2_app --parallel 6
BLOOD2_USER_DIR=/tmp/blood2-m9-view-release-final-user build/release/blood2 \
  --config build/release/tests/empty.ini --data-dir /path/to/Blood2 \
  --movement-smoke --capture /tmp/m9-view-release-final.png
```

## Limits and preservation

The local bridge still publishes complete synchronous frames; it is not a wire codec,
delayed network simulation, remote admission or complete client-shell extraction.
Original inventory/audio handle events, authoritative stance/animation and weapon/FX
services remain outside this lot. Complete collision/camera publications add CPU
snapshot work at local selection boundaries; no network delta or cache policy is
qualified here. No LAN or impaired-network qualification is claimed.
Native Windows/macOS/mobile/Quest and XR hardware qualification remain outstanding;
no simulator/emulator was launched. The pure XR input tests do not run a headset.

Pre-existing PBR files, original sources and immutable evidence/captures are preserved.
The root's pre-existing PBR guidance remains unstaged. M2-GPU-001 is not investigated
or suppressed, and M3 is not accepted by these checks.
