# M9 — local client/server movement separation

2026-10-30, following `1e2bd7b8`. This lot separates the local movement body while
preserving the solo controller and update order. It does not enable remote clients
or complete M9. M2-GPU-001 and M3 remain independent. See the
[contract](../../contracts/multiplayer_contracts.md#local-movement-separation) and
[source ledger](../sources/m9_local_movement_sources.json).

## Implemented

`ClientSession::movement` owns a value body; `CMoveMgr` refers to it rather than
using an authoritative object as its physics body. The selected original control,
acceleration, drag, jump, gravity and container algorithms remain unchanged.
`LocalMovement` owns the checked local association and routes environment, stance,
voice and position exchanges. The original client shell still owns the controller.

Each existing solo substep copies external authoritative changes, integrates the
client once, predicts collision, submits the pre-collision destination/velocity and
receives an immediate correction. The shared swept solver operates on a scratch
body for prediction, with no original instance or gameplay callbacks. A potential
push may be planned but is committed only by the server. The server validates the
already integrated destination, executes contacts/pushes once and returns resolved
pose/velocity/support, including callback impulses. Generic server physics still
skips the associated player. No double time integration, extra random draws or
second damage/pickup callback is introduced.

Support uses slot/generation IDs in client storage. Local reports carry a world
identity, object identity, monotonic sequence, force revision and movement values.
Only the exact outstanding payload can be submitted once. The original byte move
code still wraps; the additional non-saved 64-bit revision rejects old commands
after a full byte cycle. Teardown clears the client body. Saves retain their existing
layout and restore through a fresh client/world association. An unresolved
prediction remains visible in inspection and still leaves authoritative collision
validation responsible for failure diagnostics and deferred contact semantics.

The local bridge reads the live collision scene and original environment services;
it is not yet a collision replica suitable for a remote process. Prediction adds
collision-query CPU work. No wire codec, 0.14-second multiplayer upload schedule,
server advancement between delayed reports, interpolation or multiplayer clock is
introduced. Solo retains its 0.1-second cap and at most 1/120-second substeps,
existing menu/focus behavior, audio and animation timing. Presentation/PBR shaders,
materials, lighting scopes and world-owned GPU lifetime are unchanged.

## Verification

[Raw evidence](evidence/m9_local_movement_2026-10-30.txt) records commands,
results and the before/after harness. All user directories were isolated.

| Check | Result and scope |
| --- | --- |
| Linux dev and inspector-free Release | Both build; Release movement smoke passes with **240 rendered frames**, hidden UI, resize, unload/re-entry and exit 0 |
| Targeted headless regression | **164/164**: 135 synthetic and 29 external-retail-data cases; movement, locomotion, containers, pushes, doors/platforms, cameras, death/fall/impulses, imprisonment, saves and replicas |
| Clang ASan/UBSan | **26/26**, covering original object contracts, replicas, local movement, pickup prerequisite, logical audio/server clock and dedicated help; the later rebind assertion also passes in its targeted rerun |
| Real GPU | **3/3**: `gpu.world`, `gpu.movement`, `gpu.player_restore`; NVIDIA RTX 3060 Laptop / Vulkan |
| Trajectory parity | **360/360 frames byte-identical** to the `1e2bd7b8` baseline: positions, velocities, half-height, camera height, health and movement/jump/landing counters |
| Preservation | SHA-256 of **6,182** pre-existing PBR/evidence files unchanged; 39 pre-existing PBR files remain outside the commit |

The six new local movement cases check independent prediction, exactly-once
contacts and callback impulses, speculative versus committed pushes, stale world/
object/force identities and rebind sequences, collision refusal/diagnostics and
teardown. The engine-only receiver also checks client body copy/reset semantics
without linking the original game library. Existing 30/60/120 Hz checks, original
container/crouch/support cases and save/restore suites remain unchanged apart from
one support assertion now comparing an ObjectId. The seeded CityHub roam passes
in 62.93 s within its unchanged 120 s timeout.

The [trajectory record](evidence/m9_local_movement_trajectory_2026-10-30.txt) comes
from the same synthetic 360-frame input sequence compiled and linked before edits
and after the final build. Both outputs have SHA-256
`25c2932ddcc886dc77262d628c70993c33c186ce65850b26b01f945717f9f144`.
This is a Linux reconstruction regression check, not retail physics equivalence
or a new cross-platform exact-float requirement. No new GPU reference was accepted.

## Remaining M9 work

The [client collision continuation](m9_client_collision.md) now replaces this
lot's live-server prediction query with an owned scene. The remaining list below
records the initial movement boundary; environment and other services stay pending.


Replicate collision and environment state without reading the server registry;
separate the remaining original shell, HUD/camera, FX and audio dependencies; then
connect admission, delayed movement delivery and gameplay to ENet. The dedicated
zero-client foundation remains distinct from playable network hosting. No Windows,
macOS, mobile/Quest, adverse-network or human LAN qualification is claimed here.
