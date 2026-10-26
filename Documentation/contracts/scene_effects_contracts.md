# Scene effect rendering — VFX lot 10

2026-10-25. Continues the authorized VFX queue after the user's confirmation that the
other computer's renderer optimizations are integrated. [Verification](../implementation/reports/scene_effects.md),
[source ledger](../implementation/sources/scene_effect_sources.json).

## Lights

`gather_scene_effects` reads current presentation state without advancing simulation,
drawing random numbers, creating server objects or changing save payloads. It gathers
visible `CLightFX`, live explosion/lightning lights and the source's owned projectile
lights. Flare primary/alternate use radius 200, flare children 75, Leech 100 with its
existing color, Tesla 100 blue (.65,.65,1), Napalm and Howitzer fragments 100 orange
(1,.75,0), GroundStrike 150 orange. Source Init-before-Setup behavior of EnergyBlast
is retained; no speculative Zealot light is added after initialization.

The [per-receiver forward continuation](../implementation/reports/forward_effect_lights.md)
replaces the initial 16-light camera selection. The CPU snapshot retains every live
candidate in source/creation order. Each GPU-skinned model instance selects from its
posed, scaled bounds. World geometry selects per material batch, including individual
translucent faces, rather than one list for the entire level. Batch bounds are computed
when vertices are uploaded and reused during rendering; moving brushes transform the
light into their local bounds. Existing geometry batching and draw order are preserved.

The per-receiver limit is **12 on Android/Quest and iOS, 32 on desktop**, enforced before
upload. Spheres outside the receiver's oriented bounds are rejected. Remaining lights
are ranked by `max(R,G,B) * (1 - distanceToBounds / radius)`, an upper bound on their
radial diffuse contribution. Stable ties retain source order. Selection has no eye
position/orientation input, so both eyes use the same lights for the same receiver.
A fixed-size insertion list avoids per-draw allocations and sorting the whole pool.
An unavailable bound is conservative; it never becomes a fabricated point receiver.
Material batches spanning disconnected geometry can retain lights in their bounding
box's empty space; spatially splitting such batches remains separate work.

The inspector shows the live pool, platform budget, receiver count, selected/dropped
references and peak selected count. Drops are **per-receiver references**, not lights
removed globally. The existing flashlight and static-light paths keep their own rules.
`OnlyLightWorldFlag` excludes a light before ranking model submissions; `DontLightBackfacingFlag`
rejects back-facing geometry. Fog-only lights are excluded and counted as unsupported.
Effect-light shadow requests are reported as unshadowed; the existing independent model
shadow pass is unchanged. The legacy `SolidLightFlag` optimization uses the same radial
shader approximation. Source lifetime, color/radius interpolation, visibility, delay and restored ownership
remain in their existing game services. Projectile impact/failure/removal removes the
implicit light on the next snapshot.

The original shaders add these lights through the existing radial diffuse attenuation;
enhanced materials also use their direct specular response. The 1376-byte fragment
uniform includes a 1088-byte effect tail. Its 32-slot ABI is shared by all shader backends;
the counted shader loop visits at most 12 entries on mobile and 32 on desktop. No active lights and no fog keep the former
pixel output. Close-view weapons, sky and sprite/FX illumination keep their separate
policies; sprites can receive fog but never get lit by the new point lights. HUD and
menus still render after the scene. XR uses the same scene code without post-processing.

## Fog

The first containing volume wins, including a hidden volume preventing a later one
from taking precedence. A visible VolumeBrush with its own fog replaces world fog.
Departure (2026-10-26): the original also turned the fog off inside a visible fog-off
volume, so CityHub's escalator `Conveyor` volumes switched its teal world fog off
underfoot, which testers reported as a shading bug. A fog-off volume now keeps the
current fog. Outside it, the stored `FogEnable`, `FogNearZ`, `FogFarZ`, `FogR/G/B`
values resume. Parameters truncate to integers as the original client console writes
did; color is clamped to 0–255 and an invalid/empty interval is explicitly refused.
The linear factor uses view-forward depth, not radial distance. Each view evaluates
its own eye and orientation. Rebuilding the snapshot also handles loads and teleports.

This is a stateless adaptation: the original client only changed fog on a container-code
transition, so adjacent same-code volumes could retain the previous parameters. The
port uses the currently containing volume. `SkyFog` remains a separate pending sky
policy; volume fog does not hide or transform the authored sky layers. There is no
volumetric scattering. Enhanced glow and reflection weight attenuate through the fog;
alpha and depth stay unchanged. Fog applies inside the scene pass, before tonemap/AA.
The XR fallback scene fade also scales the fog color, including the clear color.

## Transparency

A shared far-to-near queue interleaves translucent world polygons, world models, effect
models, PolyGrids and sprites. Each world/PolyGrid polygon uses its posed center; models
use their bounds center. Stable depth ties preserve family then source order. Particle
systems preserve their existing internal order and are represented by their center,
not split into individual particles. Intersecting translucent surfaces still have the
usual center-sorting limitation; this is not order-independent transparency.

Sprite batching still uploads consecutive compatible geometry once, with world-space
vertices. A validated material/vertex slice lets the queue split a batch where another
transparent object intervenes and rejoin contiguous slices afterward. A slice cannot
cross a material boundary, overflow its range or begin between triangle vertices.
Opaque depth, sky, view-model depth range and ballistic line submission remain separate.

## Clipped ballistic marks

The original `CMarkSFX::CreateObject` calls `ClipSprite` with a ray-selected polygon.
At publication the port copies the selected convex BSP base ring (3–512 vertices),
including the current world-model transform. The mark retains no object handle or
resource pointer to mutable world geometry. Its UVs interpolate at clipped edges and
its original 1.25-unit normal bias remains intact. Invalid/non-convex geometry is
reported and the cosmetic mark is omitted; damage and other effects proceed.

The source ray mistakenly starts at the last mark-list `vPos` (uninitialized when
there is no old mark), and its length is shorter than the port's bias. The checked
adaptation casts three units behind the actual destination. A nonpolygon hit/miss
keeps the ordinary sprite, as an unsuccessful source `ClipSprite` would. Region reuse
retains the old mark's polygon, rotation and resource, changing only its position.
Marks remain fixed in world space after creation, including on subsequently moving
brushes, and their existing 300-mark pool/lifetime rules remain unchanged.

## Boundaries

This completes the requested lot's implementation, not original-executable raster
parity, whole-campaign VFX acceptance or M3 closure. The light budget, clipping-ray
correction and stateless fog are explicit renderer adaptations. Existing absent retail
resources stay visible as failures. The conditional P3 Rain/legacy-emitter inventory
still requires an evidenced live caller. M2 remains closed with M2-GPU-001.
