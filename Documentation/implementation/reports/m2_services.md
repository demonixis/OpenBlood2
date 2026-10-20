# M2 Continuation — Models, Motion and Original World Services

> Current continuation: [m2_locomotion.md](m2_locomotion.md) adds stairs, crouch
> clearance and original player body animations. The results and omissions below
> describe their earlier implementation snapshot.

Historical snapshot: the later [MoveMgr continuation](m2_movement.md) supersedes statements below that player movement is absent. Its own documented limits still leave M2 open.

Historical increment: the later [player entry continuation](m2_player.md) supersedes statements below that shells/CPlayerObj are absent. Full gameplay remains incomplete.

**15 October 2026 — M2 remains incomplete.** This continuation adds bounded ABC
models, animation/vertex deformation, model-string callbacks, primary-world
collision, motion services, original `WorldProperties` and `SmellHint`, and their
in-game inspection. It does **not** deliver the requested complete original
session/player or all missing classes. `CBloodClientShell`, `CBloodServerShell`
and `CPlayerObj` are still absent from the build; `--start-game` fails explicitly.
The roadmap's exit gate has not been weakened.

The [previous M2 record](m2.md) describes the initial reader/world increment.
Both increments remain uncommitted on top of the requested M1 commits. No new
library/ECS, prototype project dependency or commercial resource was introduced.

## Implemented and exercised

| Area | Current result |
| --- | --- |
| Original world properties | Original class body transfers world name, fog/music/sky settings and gravity into typed services; inactive state and destructor cleanup preserved |
| Original scent objects | Original `SmellHint` list membership, lookup, 20-second expiry and teardown; initialized link avoids indeterminate destructor state |
| ABC v6 | Strict section chain, geometry, hierarchy, clips, dimensions, keys and compressed vertex animation; shared decoded resources available before INITIALUPDATE |
| Animation services | Original API signatures for index/state/loop/reset/dimensions; original BaseClass/aggregate model-string dispatch, independent of visibility |
| Collision | Primary DAT solid polygons indexed in a BVH; continuous AABB/triangle and AABB/object queries, bounded sliding and inspectable contacts |
| Motion | Typed velocity, acceleration, global-force and teleport services; original object-to-object touch dispatch with deferred removal |
| Inspector | Model clip/marker/node selection, animated wireframe including Caleb's coat, motion/contact data, collision counts, original console variables |
| Lifecycle | Reject reentrant world teardown/stepping from callbacks; remove/reset/clip changes cancel obsolete animation events; unload/reload releases and rebuilds resources |

Detailed boundaries, limits and time/event policy are in
[model_and_collision_contracts.md](../../contracts/model_and_collision_contracts.md).
Imports and donor references are recorded in
[provenance.md](../provenance.md) and the
[continuation source ledger](../sources/m2_services_sources.json).

## Private installation observations

The same external base-game archive stack resolves **7,446 paths**. The patched
`01_SubwayA` still contains **580 source entries and 116 world models**:

- **18 original instances**: one GameStartPoint, nine Key, seven PathPoint and one
  WorldProperties. SmellHint is runtime-created and tested separately.
- **372 preprocessor-only entries**, retained for inspection.
- **190 required unported runtime objects**. None is replaced by a generic success
  object or removed from the source accounting.

Caleb's patched ABC decodes **784 stored vertices** (509 primary), **941 faces**,
**20 nodes** and **86 clips**. All clips were sampled and played to completion in
headless tests; **46 model-key commands** reached the real original dispatch
chain through a synthetic receiver. This tests the engine path, not CPlayerObj's
reaction to those commands. Standing dimensions are `(12,42,12)`; crawling uses
`(12,28,12)` from AnimDims.

The coat uses 189 deformation entries referencing 120 distinct vertices. Some
clips contain differing samples for repeated indices. The implementation retains
and applies file order, with a test for that behavior; this interpretation still
needs comparison against the original executable. The inspector capture
identified and helped correct the initial mistake of drawing packed coat
coordinates as rigid vertices.

The primary world contributes **14,300 nondegenerate solid triangles** and
**4,095 BVH nodes**. A downward box query using standing Caleb dimensions from
the original spawn `(-32,192,-3224)` hits source polygon **2549**, with box center
at **y=170**, without starting inside the surface. This is a targeted placement
check. The 115 additional world models are not yet collision instances.

## Validation

Tests cover every-prefix ABC truncation, 1,500 deterministic mutations, malformed
sections/counts, hierarchy and packed deformation, animation event ordering and
loop boundaries, callback-driven removal/reset, reentrant teardown rejection,
high-speed collision, triangle seams, touching/parallel/outward movement,
sliding, gravity/grounding, trigger contacts, and original settings/scent lifecycle.
Private tests use the external installation without copying its payloads.

Host: Fedora 44 x86_64, GCC 16.2.1, Clang 22.1.8, CMake 4.3.0 and
Ninja 1.13.2. GPU checks use Wayland/Vulkan on the NVIDIA RTX 3060 Laptop GPU.

| Configuration | Result | Evidence |
| --- | --- | --- |
| Debug, inspector on | **45/45 passed**, including private content and two GPU cases | [Build](evidence/m2-services-build-debug.log), [CTest](evidence/m2-services-ctest-debug.log), [details](evidence/m2-services-tests-debug.log) |
| Release, inspector off | **45/45 passed** | [Build](evidence/m2-services-build-release.log), [CTest](evidence/m2-services-ctest-release.log), [details](evidence/m2-services-tests-release.log) |
| Clang ASan/UBSan, leak detection and halt-on-error | **43/43 headless cases passed** | [Build](evidence/m2-services-build-sanitize.log), [CTest](evidence/m2-services-ctest-sanitize.log), [details](evidence/m2-services-tests-sanitize.log) |
| GPU preview | Captured and visually inspected skeletal/coat wireframe and original clip metadata | [Capture](evidence/m2-services-inspector.png) |
| Reference preservation | **10,923 inventoried files unchanged**, six reference repository states unchanged | [Integrity](evidence/m2-services-source-integrity.json), [snapshot](evidence/m2-services.json) |

No compiler warning, sanitizer failure or leak was reported in these checks.
Sanitizers instrument project code, not SDL/dependencies/drivers. No GPU sanitizer
claim is made. Configure logs are saved beside the build logs. The
[adaptation patch](evidence/m2-services-original-adaptations.patch) records this
batch's original-source edits.

The suite contains **39 public synthetic headless cases**, four optional private
headless cases and two optional GPU cases. GPU world tests select the Models tab,
check animation while the UI is hidden, resize, unload/reload through the inspector
command path, and verify restored original-instance/string counts and model/
collision data. The inspector-free Release variant exercises the same lifecycle.
No retail-executable behavioral comparison, other desktop backend, mobile or XR
validation is claimed.

## Reproduce

From `Blood2/` with the configured private installation:

```sh
cmake --preset local-debug
cmake --build --preset debug -j8
ctest --preset debug
ctest --preset debug -L services
./build/debug/blood2 --world-smoke --capture build/services.png
cmake --preset local-release
cmake --build --preset release -j6
ctest --preset release
```

The [initial M2 record](m2.md#reproduce) includes Clang sanitizer configuration.
The same command now discovers the additional service tests. Public-only builds
omit `BLOOD2_TEST_DATA_DIR` and `BLOOD2_GPU_TESTS`. Model and physics services do
not depend on a graphics context.

## Integration still required

The immediate original path remains `CBloodServerShell::LoadWorld` → pre/post
world callbacks → client entry → `CreatePlayer` → `CPlayerObj::InitialUpdate`,
`Respawn` and `GoToStartPoint`, with the corresponding original client startup.
Reviewing these functions showed that player initialization immediately reaches
inventory, damage aggregates, AI list membership, model/network state and a scent
object. ABC and SmellHint now have real services/code, but the inventory/damage/
client ownership dependencies have not been imported. Neither a model preview
nor a BaseClass test receiver is a player port.

Required remaining work includes:

1. Whole original shell/player lifecycle and typed local client association,
   without stubbing inventory/damage/AI branches as successful work.
2. Actual original classes for the 190 missing entries (notably 103 destructible
   brushes, seven doors, triggers and actors) and their reached services.
3. Moving/rotating world models, stairs, standing-object following, attachments,
   collision information/world handles, crush response and original movement
   semantics. The current solver is a tested foundation, not a parity claim.
4. Animation transitions/blending, original node/attachment queries, skins,
   animated normals and final model rendering. The preview is diagnostic.
5. Reference-executable comparisons for coordinates, repeated vertex samples,
   event timing and collision/player behavior before closing the M2 gate.

Lighting guidance is tracked in the
[lighting clarification](m2.md#lighting-clarification-for-later-milestones-2026-10-15):
DirLight represents a spotlight; investigate the implicit main directional light
and Shogo's explicit property before designing the lighting reconstruction.
