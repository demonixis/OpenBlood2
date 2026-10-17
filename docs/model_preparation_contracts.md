# Prepared ABC geometry and CPU sampling

This extends [model presentation](model_view_contracts.md) and
[animation/collision](model_and_collision_contracts.md). See
[evidence](../../Documentation/Implementation/m2_model_preparation.md).
This is replacement engine optimization, with no original gameplay changes.
M2 remains incomplete. Skinning and packed vertex deformation still run on the CPU.

## Immutable assets and exact reuse

`ModelView::add(shared_ptr<const Model>, ...)` may reuse a prepared local-space
triangle stream. The asset must remain immutable for its entire ownership lifetime.
The borrowed `Model*` overload always samples and validates again; use it for
mutable diagnostic fixtures. Do not mutate an asset through an earlier mutable
alias after publishing it to production objects.

The cache holds at most one variant per asset. Its key includes the shared-owner
identity, animation index, exact sample time, XYZ scale, complete node-hide mask
and whether the skin uses the explicit magenta fallback. Times after the last key
compare at that last key's time, which produces exactly the same terminal pose.
There is no time quantization, animation throttling or approximate pose sharing.
Different scales or out-of-sync instances can replace the same asset's variant.

Instance position/rotation, alpha, world/close pass, current skin material and
object identity are applied freshly on every add. They do not change the cached
local triangle stream. Validate transform, alpha, mask shape and existing frame
budgets before a possible hit. Missing models, invalid poses and skins remain
inspectable; `require_skin` still suppresses failed textures. Hidden or pending
objects never draw from an old cache entry. All-hidden geometry may cache an
empty stream with its actual hidden-triangle count.

Cache admission is optional: limits cause uncached preparation, never skipped
geometry or successful missing services. Limits are 256 assets and 1,000,000
retained vertex slots (32,000,000 bytes, plus bounded metadata). Retained vector
capacity counts against the vertex budget, including after a smaller pose/mask
result. Animated misses reuse their storage when it fits. The original independent
1,000,000 expanded frame-vertex budget still applies even on hits.

Entries keep weak asset ownership, never owning object handles or strong model
references. Prune expired owners at `begin_frame`; compare ownership control
blocks on lookup to reject recycled addresses even within a frame. `clear()`
drops every prepared entry, skin and frame. The application's existing world
revision boundary clears model/FX views and GPU geometry at NEW_LEVEL/unload.
A retained inspector snapshot must not prolong source asset or game-object life.

## Sampler and presentation invariants

The CPU sampler computes its key interval once and composes at most 256 node
poses in fixed local scratch, avoiding temporary heap pose storage. The public
pose sampler uses the same composition helper and reuses its output capacity.
Parent order, node IDs and packed deformation bounds fail safely. Repeated packed
vertex indices retain file write order; quaternion interpolation, basis conversion,
rigid transform and final deformation arithmetic/order stay unchanged.

Preparation never advances a track, emits a model key, moves an object, resolves
an attachment, performs physics or invokes an original callback. Node queries,
hit spheres, attachments and FIRE_KEY delivery retain their existing clocks and
services. No culling, AI cadence change, GPU skinning or shader revision is added.
Current expanded vertices still copy into each frame and upload through the
existing SDL_GPU path. Both bundled Vulkan and Metal shaders are unchanged.

The Model snapshot inspector reports per-frame sampled/reused/uncached instances,
reused vertices, live cache entries and retained vertex bytes. Entry rows identify
actual reuse. Counts are not timings, FPS, or a claim that all transition stutter
is fixed. Hidden/missing entries remain visible, with no sampling side effects.
