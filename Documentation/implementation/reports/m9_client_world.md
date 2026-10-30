# M9 — first local ClientWorld replica

2026-10-30, following `2beeb100`. The requested continuation starts client/server
separation in solo local play. M9 is still in progress; this lot does not enable
remote admission or a playable network host. M2-GPU-001 and M3 remain independent.
See the [contract](../../contracts/multiplayer_contracts.md#local-replica-first-presentation-lot)
and [source ledger](../sources/m9_client_world_sources.json).

## Implemented boundary

`ClientSession` owns `ClientWorld`, a passive complete-frame receiver. The local
publisher copies every live object's presentation state, including hidden and
inactive objects, without retaining original object pointers. Mutable poses,
playback, flags, node visibility, colors and attachment state are copied; decoded
immutable ABC/SPR resources may be shared locally. This is not a network packet
format. The receiver never executes ObjectDLL callbacks or advances clocks.

The production `ScenePresenter` publishes one frame before preparing its views.
Standalone scene helpers retain their immediate local entry points, which publish
and then invoke the same consumers. Models, sprites, resolved attachments,
world-model poses, sky slots, ModelAdd lighting and shadow flags now come from the
replica. The original view-weapon and muzzle visibility gate is copied with the
owner's state. PolyGrid/particle-stream owner checks use value identities, while
their existing effect pools remain shared. No game update, input, RNG, audio,
save payload, GPU shader, material rule or XR arm-placement algorithm changed.

Complete replacement handles creation and removal in slot order; pending removal
is excluded before reclamation. World epochs distinguish replacement sessions even
when their registries allocate identical object IDs. Teardown clears the client's
frame; old-world or old-generation references cannot resolve in its replacement.
Scene world geometry and lighting caches also check that epoch. An attachment
whose parent is absent is hidden as unresolved instead of retaining a stale pose.
The inspector shows the last delivered epoch/frame/time, object count and selected
replicated pose, model, skin and animation, including absence from that frame.

The common property/Euler helpers, sky projection and server world bindings were
split into separate translation units. A replica consumer executable now links
`engine_core` without `blood2_game`; its symbol table contains no original
`bc_EngineMessageFn`, `CBloodServerShell` or `Objects::advance`. This catches the
former implicit dependency through `world.h` and presentation helper objects.
The original object-contract suite is now registered for server-only builds too.

## Verification

[Raw evidence](evidence/m9_client_world_2026-10-30.txt) records the commands,
results, initial timeout and pixel digests. All runs use isolated user directories.

| Check | Result and scope |
| --- | --- |
| Linux dev and inspector-free Release client builds | Pass; Release also opens SubwayA and renders 20 frames with a capture, exit 0 |
| Targeted headless regression | **116/116**: 102 synthetic and 14 external-retail-data cases; original objects, local replica, movement/player, model/sprite/world/sky/light preparation and save/load suites |
| Clang ASan/UBSan | **19/19** final selection, plus fresh dedicated help; includes all 14 original object cases, three replica cases, multiplayer pickup prerequisite and logical audio/server clock |
| Real GPU | **4/4**: `gpu.world`, `gpu.sky_app`, `gpu.surface`, `gpu.post`; NVIDIA RTX 3060 Laptop / Vulkan |
| Presentation pixel parity | **8/8 byte-identical** 960×540 frames: SubwayA and CityHub, Original vertex/pixel lighting and Enhanced Standard/High |
| Preservation | SHA-256 of **6,181** pre-existing untracked PBR/evidence files unchanged; no shader or historical capture changed |

The pixel oracle extracts the five old app presentation source/header pairs from
`2beeb100`, links them with the same current common engine/game libraries and
uses the existing `surface_gpu_tests` setup (30 steps at 1/60 s). Only a temporary
copy of the test writes raw pixels; the pixel-lighting variant toggles that setting
without stepping again. This compares the old direct-object presentation against
the replica, not two complete historical binaries. The normal GPU suite also
checks post-processing, original-mode restoration and XR stereo/subrect/fog-fade
paths; it does not qualify a physical headset or PBR hardware.

The first extended Debug ASan/UBSan run passed 18/19 checks and hit the existing
10-second timeout on `objects.message_churn`. Running its unchanged workload
separately passed in 21 seconds under concurrent compilation, with no sanitizer
finding. Its sanitizer-only CTest timeout is now 60 seconds; the full repeated
19-test selection passed. The first timeout remains recorded as a failed run.

## Remaining separation

The [next local movement lot](m9_local_movement.md) now separates the CMoveMgr
body and immediate prediction/correction path. The remaining list below records
the boundary at this first presentation lot; independent collision/services and
network gameplay remain pending.

This is the first object-presentation lot, not completion of the replica stage.
`GameSession` still owns the original client shell and server services together;
the dedicated wrapper still contains shared CPU storage. Remaining work includes:

- Independent client movement/collision state, original shell, camera and local
  prediction; the solo MoveMgr still integrates the authoritative body once.
- Per-client HUD/inventory/camera message delivery and removal of direct original
  object reads in those paths.
- FX pool ownership, persistent effect events, dynamic lights/fog and separation
  of logical sounds from client playback. `presentation_effects.cpp` still reads
  original projectiles and volume brushes.
- Resource-name resolution and bounded gameplay wire encoding, initial admission,
  deltas/events, loss/reordering, multiple real clients and all multiplayer rules.

Windows/macOS interoperability, mobile/Quest builds, physical XR, human LAN and
the 22-map/two-client or 16-client qualification have not been run in this lot.
The pre-existing untracked PBR work and historical GPU references remain outside
the commit; no PBR hardware qualification or new campaign acceptance is claimed.
