# Original sky objects and world-model presentation

M2 remains incomplete. This integrates the original `DemoSkyWorldModel` and
`SkyPointer` callbacks from Blood II's `AppHeaders/engineobjects.c`. It extends
[world presentation](world_view_contracts.md) and [transparent surfaces](world_alpha_contracts.md).
See [implementation evidence](../implementation/reports/m2_sky.md).

## Original callbacks and typed services

Both classes inherit BaseClass, not B2BaseClass: no invented difficulty filter.
World-file precreate reads SkyDims, Index and InnerPercentX/Y/Z, and copies Name
to Filename. DemoSkyWorldModel forces OT_WORLDMODEL; SkyPointer forces OT_NORMAL
and reads SkyObjectName. Runtime/string creation retains the source's zero sky
size, 0.1 inner fractions, zero index and empty pointer name. Restore creation
is explicitly unavailable. The pointer's authored name limit is 98 bytes, matching
the source's 99-byte destination capacity including the terminator.

Initial update changes the definition only when **all three** sky dimensions are
nonzero. The outer bounds are current object position plus/minus SkyDims; view
bounds use the componentwise dimensions-times-inner-fractions. A zero component
preserves the previous definition. World-model binding uses the existing unique
DAT name, collision shell and DAT translation pivot, before initial update.

The model sets FLAG_SKYOBJECT (bit 12) and FLAG_FORCEOPTIMIZEOBJECT (bit 28), then
adds itself. The latter flag is retained metadata; no obsolete optimization cache
is needed. SkyPointer schedules 0.001 seconds, searches at its first update,
selects the first existing FindNamedObjects result, marks/adds that target, then
removes itself. Target creation after pointer creation therefore works. Name
lookup remains exact and case-sensitive, with existing reverse insertion order.
Removal is deferred through the ordinary callback lifetime. Unlike the original
empty-list leak, every owned name list is released.

The engine owns 30 non-owning generation-checked slots: the actual original
MAX_SKYOBJECTS is 30, so indices are **0–29**, despite the source comment saying
0–30. Already registered objects retain their membership even if their slot was replaced.
A new registration replaces an occupied slot, following later LithTech
si_AddObjectToSky; re-adding the old member is still a no-op until removal.
Invalid indices, missing/pending identities and unsupported object types fail visibly. Bound world models are the
supported subset; sprites, models and polygrids in the sky are not accepted.

Definitions require finite, bounded, increasing outer bounds and nondecreasing
view bounds. Negative dimensions/fractions reject precreate; zero view extent is
valid. Failed definitions/additions leave previous state unchanged, record an
object diagnostic and a required unavailable-service entry. The last failure is
retained for inspection. A missing pointer target also records a failure before
its original one-shot removal; it is never counted as successful sky rendering.
Source entry accounting describes initial creation, not perpetual pointer lifetime.

Pending/stale slot identities never draw and cannot refer to a reused object slot.
Hidden objects do not draw; inactive visible objects do. Sky records do not own or
remove targets. Unload resets the definition, slots, errors and counters. NEW_LEVEL
preflights pending pointer targets/slots on a copied registry without running their
update or advancing the candidate world, and leaves the source untouched on failure.

## Camera and rendering

Blood II's SkyDef describes compressing the world camera into the view box. The
explicit componentwise formula is taken from the inspected **later-generation**
LithTech `common_draw.cpp`: `viewMin + (eye-worldMin)/(worldMax-worldMin) *
(viewMax-viewMin)`. This is a compatible reference implementation, not verified
retail Blood II renderer parity. Outside-world cameras extrapolate; there is no
clamp. Degenerate/nonfinite world bounds fail before drawing. Camera rotation is
unchanged. Outer bounds remain authored metadata; they do not clip the sky geometry.

The renderer draws current checked world-model poses in ascending slot order,
using the same uploaded geometry, DTX images, sampler and session-time Pan effects
as ordinary geometry. No simulation or animation advances while drawing. Separate
opaque/alpha sky pipelines disable depth tests and writes; surfaces retain existing
bit-3 alpha classification and source-order batches. This follows the reviewed
later-engine sky ordering/depth behavior. The near plane is 0.01, far plane 65536.

A sky background pass precedes the normal world. FLAG_SKYOBJECT excludes these
objects from normal world draws while retaining binding accounting. Authored
bit-4 sky faces are separate batches and omitted from the normal opaque pass when
a sky background is prepared. With no visible supported sky, diagnostic portal
colors remain. This full-viewport background is a simple replacement for historical
portal-extent culling; it also appears in uncovered void. No stencil portal
occlusion, fog, lightmaps, sky scale console controls or retail pixel parity is
claimed. Physics flags and collision geometry are unchanged.

All passes share existing packaged SPIR-V/MSL shaders; no dependency or shader
artifact changes. Resize/revision clearing and SDL deferred GPU destruction apply
to sky pipelines and shared resources. The inspector reports actual submitted sky
batches separately, definition/view bounds, computed sky camera, checked slot
names, hidden/stale state and failures. Inspector-free Release uses the same path.
Native Metal execution of this increment has not been tested on this Linux host.

## Interactive time discovered by the integration check

The real variable-frame CityHub application test exposed a CPU catch-up feedback
loop in Debug: a roughly 0.40-second frame produced a roughly 1.28-second simulation
step, and the next frame exceeded the unchanged 0.5-second physics bound. The
fixed-delta diagnostic tests had not exercised this. Pausing loading/capture work
alone did not resolve the subsequent simulation feedback.

`app_frame_delta` now caps **interactive** input/simulation frames at 0.1 seconds.
Excess wall time is discarded, never accumulated as future catch-up. The 3D
inspector and shutdown log expose limited-frame counts and discarded wall seconds.
Resource revision preparation/uploads and diagnostic GPU capture are paused tool
work. Normal frames below the cap retain their elapsed time. Explicit diagnostic
steps and GameSession::step are unchanged, including invalid-time rejection and
the physics catch-up bound. Slow Debug execution can therefore slow game time;
this is a bounded frontend policy, not retail timing or a performance optimization.

## Evidence

Synthetic CPU tests cover callbacks, zero-component preservation, bit flags,
index/definition rejection, delayed lookup, empty-list cleanup, duplicate refusal,
non-world-model refusal, projection/extrapolation, portal batch separation,
inactive/hidden/pending/stale lifetime and unload. Private tests load CityHub twice
and verify five ordered target identities, actual textures and first-update removal.
GPU pixel tests cover painter order even when a later object is farther away,
alpha, the smaller near plane, lack of sky depth writes, normal world occlusion,
portal omission, resize, cache replacement and invalid draw recovery. Transition tests reject missing/non-world-model pointer
targets while preserving source identities, globals, clock, client and list ownership. Private GPU
captures include two sky directions and a combined sky/world view over two reloads.

## Sprite sky layers

The complete-level audit extends the accepted sky types to resource-bound
OT_SPRITE objects, used by Crypt's `moon`. Missing sprite resources still fail.
World geometry and sprite billboards share the compressed sky camera, .01 near
plane, depth-disabled pipelines and ascending authored slot order. The presenter
interleaves the two existing meshes at those slots; it does not distance-sort
sky layers or append every sprite above all clouds. Alpha, frame clocks, scaling,
rotation, object color and hidden/stale lifetime use ordinary sprite rules.
Sky sprites are excluded from the ordinary scene. A sprite-only sky still
computes the projection and suppresses sky portal faces. Geometry/texture uploads
share the existing registry and scene pass; dependencies and shaders are unchanged.
Models and PolyGrids in sky remain unsupported. This supersedes only the earlier
sprite refusal. Unload clears the separate sprite snapshot and GPU geometry.
