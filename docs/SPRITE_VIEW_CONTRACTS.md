# Sprite presentation contracts

The [Flare Gun continuation](FLARE_CONTRACTS.md) adds original ID 3, primary sticking/burn ticks, alternate fragments, pickup/drop and sprite tint/alpha. Earlier blanket omissions are historical for this subset; remaining effects, death services and M2 stay open.

Continuation: [original explosion sprites](EXPLOSION_SPRITE_CONTRACTS.md). Client explosion sprites now join the same read-only scene and use per-instance alpha multiplied by decoded DTX alpha. This supersedes the blanket alpha-service omission only for typed client FX state, not generic object tint/alpha or other composite services.

M2 remains incomplete. This extends [original sprite placement/clocks](SPRITE_CONTRACTS.md)
and the [DTX decoder](TEXTURE_CONTRACTS.md). See [evidence](../../Documentation/Implementation/M2_SPRITE_VIEW.md).

## Original state and replacement geometry

`update_sprite_scene` reads alive OT_SPRITE records each frame, outside ImGui. Hidden
objects retain inspection/resource diagnostics but emit no quad; inactive objects
can still draw. Pending/removed objects disappear immediately from the snapshot.
No object pointer, original handle, callback, simulation step or sprite clock is
retained or advanced by presentation. Original DetailSprite flush/repeat rules and
its unchanged authored rotation remain untouched.

The current SPR frame supplies an exact-path DTX resource. Rotatable sprites use
the current object quaternion; others use camera right/up. Geometry uses DTX width
and height multiplied by X/Y scale as **half extents**, following the examined
later-generation LithTech renderer. That renderer is a reference, not an imported
engine or verified retail Blood II raster specification. UV top-left is (0,0),
both windings render, and there is no position/depth bias or guessed flush rotation.
Zero X/Y scale emits no geometry. Invalid/nonfinite/negative transforms or unsupported
mode flags remain named diagnostics and emit no geometry.

The Blood II SDK explicitly says FLAG_SPRITECHROMAKEY is disabled. The renderer
therefore does not remove RGB black (or any guessed key color). SPR transparency,
translucency and color-key metadata remain inspectable; the current presentation
policy uses actual decoded DTX alpha for all supported sprites. Glow, view bias,
no-Z and sky modes are explicitly unavailable; those sprites do not silently render
with ordinary semantics. Generic object tint/alpha services, lighting, polygon clipping,
additive/multiply effects and retail blending parity are not implemented here.

## Cache, alpha and lifetime

A scene owns up to 4,096 entries / 24,576 vertices, 4,096 named materials plus one
white fallback slot, and 128 MiB decoded RGBA. Existing -2 DTX validation/limits
apply. Missing/unsupported frames stay visible as named errors; no guessed-size
magenta quad is substituted. Materials cache successes and failures by normalized
exact path for the loaded world revision. Frame changes reuse already decoded
materials; alpha counts are computed once on load. World revision/unload clears
both CPU and GPU geometry/material caches, even when UI is hidden or reload occurs
between presented frames. SDL deferred release owns in-flight GPU lifetimes.

`WorldRenderer` has an explicit SpriteAlpha style; the opaque world/model default
is unchanged. A separate fragment shader discards zero-alpha texels and emits DTX
RGBA with optional typed client-effect alpha modulation. The pipeline uses straight source-alpha blending, depth LESS against shared
world/model depth, **no depth writes**, nearest mip-zero sampling and clamp-to-edge.
Sprites sort stably from far to near by center camera depth. This handles ordinary
separate quads; intersecting translucent surfaces are not order-independent.
Sprites draw after world/models and before close-view weapons, with the same camera
and cinematic letterbox. No-UI Release follows the identical path. Vulkan/SPIR-V
is the only validated shader/backend combination.

## Inspection and verification

The Sprites tab distinguishes prepared quads from submitted GPU passes/uploads,
shows decoded dimensions, zero/partial-alpha counts, mode and frame-specific errors,
and retains source failures/hidden/inactive/repeat placement inspection. Prepared
geometry is not proof that a quad lies in the camera frustum.

SubwayA has 12 supported DetailSprites sharing 128x128 bloodsplat3.dtx: 14,828
zero-alpha and 1,556 partial-alpha pixels. They retain source scale .5 and Y=129.
Source accounting stays 203 created / 4 transferred sounds / 372 editor-only /
1 unsupported Gideon / 0 failed. Remaining required FX components, safe automatic NEW_LEVEL,
other required services and the observable M2 gate remain open.

CPU checks cover orientation, original half extents, UVs, stable depth sorting,
frame cache/failures, unsupported flags, budgets, unchanged clocks, inactive/hidden/
pending lifetime and three actual unload/reloads. GPU readback verifies real zero,
partial, opaque and opaque-black pixels, overlap order, world occlusion, absence of
depth writes, resize/reupload and a real private bloodsplat at its authored pose.
The application smoke additionally requires all 12 quads with hidden UI and after
reload. These are harness observations, not a retail executable comparison.

Continuation: [original explosion rings](EXPLOSION_RING_CONTRACTS.md) now supplies
selected fire/smoke particle rings and alpha billboards. Earlier missing-ring statements
are historical; fragments/flames, illumination and the full M2 gate remain open.

Continuation: [original fragment smoke trails](OBJECT_PARTICLE_CONTRACTS.md) supplies
selected high-detail OBJFX_SMOKETRAIL_1 behavior. Earlier trail omissions and the
4096 sprite-entry limit are historical; the shared renderer now holds 16384 entries.
Other ObjectFX modes, required effects and the actual M2 gate remain open.

Continuation: [original explosion flame particles](EXPLOSION_FLAME_CONTRACTS.md)
now implements the selected Tesla component. Earlier blanket flame omissions are
historical for that recipe; surface illumination and the actual M2 gate remain open.

Continuation: [weapon impact flash/smoke](WEAPON_SPRITE_CONTRACTS.md) supersedes the earlier blanket impact-visual omission for wood/stone bullet sprites only. Other composite components and M2 remain open.

The [world-alpha correction](WORLD_ALPHA_CONTRACTS.md) supersedes earlier blanket world-opacity/transparent-marker statements for resolved bit-3 surfaces. Sky and rejected resources remain opaque markers; ordinary surfaces still ignore alpha. The new sorted world pass precedes the separately sorted translucent FX passes. M2 remains incomplete.


Continuation: [original corpse blood pools](BLOOD_SPLAT_CONTRACTS.md) supersedes the pool omission for corpse ground hits. Blood-spurt particles, gib impact callers, shared SFX eviction and other required services remain incomplete. The M2 exit gate is unchanged.
