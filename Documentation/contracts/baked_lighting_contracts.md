# Baked world lighting and replacement model light

The world now renders the original preprocessed per-vertex light stored in DAT v56
files, and GPU-skinned models read that baked light from the surface below them. This
is the "LightMap 0" look of the original renderer (Gouraud vertex light instead of
lightmaps). Lightmaps, static light objects on models, model shadows and realtime
lights remain pending. No retail renderer parity is claimed. Reference corroboration
is recorded in the [source ledger](../implementation/sources/baked_lighting_sources.json).

## Original data

Blood II light classes (`Light`, `DirLight`, `GlobalDirLight`, `ObjectLight`,
`LightAnimator`) are `CF_NORUNTIME` engine classes in `AppHeaders/engineobjects.c`:
the preprocessor reads them; the game never creates them. `Light` is a point light
and `DirLight` a cone (`FOV`) despite its name. `GlobalDirLight` is the sun, which
lights only the world. It is absent from SubwayA and CityHub but present in other
retail worlds.

Each DAT polygon vertex stores 3 bytes of baked RGB after its point index; the reader
used to skip them. `Polygon::colors` now keeps them in point order. Analysis of both
selected worlds shows:

- every polygon carries colors, including lightmapped (`SURF_LIGHTMAP`, bit 7) faces;
  later LithTech also computes vertex colors for lightmapped polygons;
- the world `AmbientLight` is already included: CityHub's darkest lightmapped vertex is
  exactly its `AmbientLight 17 23 23`, and SubwayA's is `0 0 0`;
- `SURF_BRIGHT` (bit 5) and sky faces store 255; SubwayA's flat-shaded train glass stores 128.

The 12-byte polygon prefix (extents that are multiples of the 20-unit lightmap grid)
and the root-model tail remain undecoded. The tail starts with a header of grid
dimensions, a cell size of 500 and the root minimum, followed by data that is not RGB
samples. It is not a light grid that we can read yet.

## World composition

`WorldMesh::build` sets each corner of a resolved, non-sky textured face to its baked
color divided by 255. Faces with `SURF_BRIGHT`, and generated geometry without colors
(empty `colors`), stay fully bright. Unresolved materials keep the magenta marker, and
mesh builds without resources keep orientation colors. `WorldMesh::lit` and
`fullbright` count textured faces. Translucent faces use their baked light as well.

Both fragment shaders compute `albedo * (faceColor + lightColor * radial)`. The
bounded radial point light (the [flashlight](flashlight_contracts.md)) is therefore
added to the baked light before the albedo multiply. Later LithTech does the same:
`(lightmap + dynamic lights) * texture`. With `faceColor = 1` the result equals the
former `albedo * 1 * (1 + light)`. Sprites receive no radial light, so they are
unchanged. Only the two fragment artifacts changed when the shaders were
regenerated. There is no overbright: the 8-bit target clamps the result.

## Replacement model light

The original model shading service is not available. Blood II passes `ModelAdd`/
`ModelDirAdd` to the engine console, uses `SetObjectColor` ("brightens a model's
shading") and calls `GetPointShade`. Later LithTech lights models from a baked grid
plus static and dynamic lights. In place of that, `ModelLighting`
(`src/app/model_lighting.*`) gives each GPU-skinned model one light color, written to
its existing skinned-uniform face color:

1. **Sample:** the interpolated face light of the first textured opaque face crossed
   by a vertical segment from the model's world bounds center down to the world
   minimum Y. View models sample below the camera. `WorldShade`
   (`src/engine/world_shade.*`) builds a BVH per world range from the presentation
   mesh's own triangles, colors and pivots, and traces with the current range
   transforms. Translucent, sky-portal and unresolved faces are excluded, and both
   sides are accepted.
2. **Fallback:** when nothing is below, the world `AmbientLight`.
3. **Additive terms:** the original WorldProperties `ModelAdd` (server console values,
   truncated to integers like the client's `"ModelAdd %d %d %d"`, then divided by 255),
   then the object's `SetObjectColor` RGB. That color is additive for models:
   `CDestructableModel` sets its default `TintColor` of 0.
4. **FLAG_NOLIGHT** models use their object color alone ("just use the object's color").
5. **Clamp:** each channel is clamped to 0–1.

`ModelDirAdd` is read and inspected but not applied: it needs model normals.
`GlobalLightScale` has no producer in the current client shell. The composited
[screen fade](screen_fade_contracts.md) is unchanged.

Samples are cached per checked object identity. A sample is reused while the bounds
center is unchanged and no world range that moved, appeared or disappeared has a world
box overlapping the sampled segment (from the hit to the center). Unbounded moving
ranges invalidate every sample. View models are sampled every frame. The CPU model
path (standalone renderers, tests, borrowed models) stays unlit. Presentation reads
no simulation service and changes no object state; collision work counters are
unaffected because `WorldShade` is separate from `CollisionWorld`.

This is replacement policy, close to classic "light under the model" sampling.
Models lit by authored `Light`/`DirLight` objects with `LightObjects`, per-vertex
normals, and shadows remain pending.

## Inspection and measurement

The inspector's world section shows lit/fully bright faces, models
(sampled/reused/ambient fallback/`FLAG_NOLIGHT`), trace work, `AmbientLight`,
`ModelAdd` and the unapplied `ModelDirAdd`. `--bench` prints the same lighting line.
`--bench --capture FILE.png` saves the last measured frame.

Measured on Apple M5 / Metal:

| Scene | Before | After |
|---|---|---|
| CityHub 3440x1440, frame mean / p95 | 1.65–1.70 / 3.36 ms | 1.72 / 3.40 ms |
| CityHub 3440x1440, model stage | 0.09 ms | 0.096 ms |
| SubwayA 1600x900, frame mean | 0.67 ms | 0.67 ms |

Over 600 frames, CityHub has 232 lit models, of which 230 reuse their sample each
frame; the 360-degree sweep gives the same result. SubwayA reports `ModelAdd 10 10 10`;
CityHub reports 0 and its 89 `SURF_BRIGHT` faces are fully bright.

## Tests

- `world_lighting.mesh`: fan-corner baked colors, `SURF_BRIGHT`, absent colors,
  unresolved and resource-free builds.
- `world_lighting.shade`: barycentric light, either side, misses, transformed ranges,
  translucent exclusion.
- `world_lighting.ambient`: `AmbientLight` record parsing.
- `world_lighting.retail` (private data): checks that
  - every SubwayA/CityHub polygon carries colors;
  - the darkest lightmapped light equals `AmbientLight`;
  - sessions light GPU-skinned models;
  - unchanged models reuse their samples.
- `world_view.subway` now expects the authored glass light of 128.

## Pending

- Decode DAT lightmaps (polygon prefix and model tail) for per-texel light.
- Light models with authored `Light`/`DirLight`/`ObjectLight` (`LightObjects`), using normals and the cone.
- Model shadows (`FLAG_SHADOW`).
- Realtime point lights on the world and models.
- A later baked-plus-realtime design.

The [lightmap continuation](lightmap_contracts.md) decodes the stored DAT lightmaps. Lightmapped faces now draw `albedo * 2 * min(lightmap + radial light, 1)` (the shipped Saturate default), and model sampling reads the hit face's lightmap with the same scale. Per-vertex light remains for Gouraud-only faces, for empty entries and for models whose lightmap block does not validate. Earlier statements that lightmaps are pending or undecoded are superseded.

The [static light continuation](static_light_contracts.md) replaces the floor-sampling model light with the original light objects: Lambert-shaded sources plus `AmbientLight`, `ModelAdd` and object color, with N.L terms doubled like lightmaps. `WorldShade` now serves occlusion tests; `trace` remains for tests. Earlier statements that model lights come from the face below are superseded.
