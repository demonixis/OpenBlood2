# M2 continuation: original sky objects

M2 remains incomplete. Blood2 now runs the original `DemoSkyWorldModel` and
`SkyPointer` callbacks and renders their bound world models through SDL_GPU.
The four CityHub pointers perform their original delayed lookup and remove
themselves; the registered targets remain independently owned.

## Implemented behavior

The source SkyDims/inner-fraction calculation defines a small camera view box.
The engine maps the current world camera into that box and draws checked sky
objects in index order before ordinary geometry. Sky rendering disables depth
reads/writes, preserves transparent surface alpha and session-time texture Pan,
and uses the existing uploaded world geometry and textures. Registered sky objects
leave the normal world draw list; authored sky faces reveal the background.

This uses original Blood II object callbacks and API definitions. The explicit
projection formula and depth ordering were checked against later LithTech engine
code; retail Blood II pixel parity is not asserted. No dependency or shader
artifact changes are needed: shared packaged SPIR-V and MSL supply both pipelines.
Native Metal execution of this increment is untested here.

Checked slots do not own original objects. Deferred removal, hidden/inactive
objects, generation changes and unload are handled without stale draws. Missing
targets, invalid definitions/indices, duplicate occupied slots and unsupported
object types fail explicitly. NEW_LEVEL validates pending pointer destinations on
a copied registry before promotion, without consuming their first update. Normal
objects' ignored Filename fields are no longer falsely reported as model assets.

The in-game 3D inspector displays sky bounds, the view box and projected eye,
slot identities and hidden/stale status, failures and actual submitted batches.
CPU state, frame updates and rendering work with inspection hidden or compiled out.

## Private content accounting

CityHub has **448 initially created entries, 31 transferred sounds, 541 editor-only,
five unsupported and one failed**, totaling 1,026. The added five source entries
are one DemoSkyWorldModel and four SkyPointers. After the first update the four
pointers are removed, while the five sky slots contain Stars1, Clouds1, Moon1,
SkyWorldModel and Buildings in indices 0 through 4. Source-entry counts are not
live-instance counts.

Two private reloads resolve every material used by those sky objects: 172 triangles,
23 batches including 12 alpha batches. NVIDIA
captures show the actual clouds and skyline, and a combined world/sky view of
the station. These are diagnostic cameras, not a completed retail playthrough.
Commercial assets and captures remain external/ignored.

Remaining CityHub entries are two ProximitiesPU, two RemotesPU, one FlashlightPU,
and the failed CDestructableBrush `fallengrills` whose named geometry is absent.
The strict NEW_LEVEL gate still rejects CityHub. No class is reclassified merely
to close M2; required unsupported services and later milestones remain explicit.

## Interactive-frame correction

The new application-level GPU test initially failed in Debug. Profiling showed
CPU simulation catch-up feeding longer frames back into itself (about 0.40 seconds
of elapsed time caused 1.28 seconds of simulation work), eventually violating the
existing 0.5-second physics limit. Excluding loading/capture work alone was not
sufficient. The frontend now caps interactive frames at 100 ms, reports discarded
wall time, and excludes explicit resource/capture stalls. Direct/diagnostic
simulation and its safety checks remain unchanged. The real 20-frame CityHub app
then passed three consecutive Debug launches with a capture, advancing simulation
and 27 ready NPCs with zero failed NPCs. Slow Debug gameplay can still run below
real time; CPU optimization is not claimed by this correction.

## Verification

- Debug and inspector-free Release: **754 unique headless cases each**, including
  **123 private-data cases**. After the frontend timing correction, all eight
  affected CLI/session checks passed again in both builds.
- Clang ASan/UBSan with leak detection: **42 unique cases**, including six private
  cases (41 object/service/world/sky/texture checks plus the final timing policy).
- NVIDIA Vulkan: **14 GPU checks per native build**, including eight private cases.
  Coverage includes exact sky pixels, actual CityHub app frames/capture, world alpha,
  depth, NEW_LEVEL, objectives, input/combat, corpse clearance, FX and UI lifetime.
- The real CityHub application passed **three consecutive Debug launches**, retaining
  all 20 frames and the capture; its source physics guard remains unchanged.
- Formatting: 35 changed/new C++ files. All local Markdown links resolve; all 10,923
  inventoried reference files and six reference repositories remain unchanged.

Builds use the existing debug/release presets and the configured Clang sanitizer
build. Native suites run `ctest --test-dir build/<configuration> -L headless
--output-on-failure -j4`. GPU runs explicitly set
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json`. The ledger records
all focused expressions, final logs and preserved development failures. No failing
assertion, unsupported class or missing geometry was suppressed to obtain these
results. The existing ItemPickups float/GameType comparison warning is unchanged.

See [service/render contracts](../../contracts/sky_contracts.md),
[source provenance](../sources/m2_sky_sources.json), and
[validation ledger](evidence/m2-sky.json).

## Limits

Only bound world-model sky targets are supported. Sky sprites/models/polygrids,
fog, lightmaps, original visibility acceleration and sky console scaling remain
unavailable. The simple full-viewport background replaces portal-extent culling
and can also appear through uncovered void. With no visible supported sky,
diagnostic portal colors remain. Existing collision and original game rules are
unchanged. Other native GPU backends and retail visual fidelity remain unverified.
