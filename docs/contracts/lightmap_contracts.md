# Original DAT lightmaps

Lightmapped faces now render the per-texel light that the original preprocessor stored
in each DAT v56 world model. This is the original "LightMap 1" look, and it replaces
the per-vertex fallback of the [baked lighting continuation](baked_lighting_contracts.md)
on those faces. The format was reverse-engineered from retail data (all 58 worlds) and
corroborated by later LithTech code. No retail executable trace exists, so this is not
claimed as pixel parity. Reference corroboration is recorded in the
[source ledger](../implementation/sources/lightmap_sources.json).

## Format

Three parts of the DAT carry the lightmaps:

- **Surface axes.** The two vectors after the surface texture axes are the lightmap
  axes P and Q (`Surface::lightmap_p/q`). They are not unit length: |P| = 2 gives a
  texel every 10 units, and sloped faces use projected principal axes (|P| < 1). Later
  LithTech computes the same N × axis basis.
- **Polygon prefix.** The first two `u16` of the 12-byte prefix are the grid-rounded
  extents along P and Q (`Polygon::lightmap_extent`), always multiples of 20. The
  remaining 8 bytes are runtime residue (pointers, NaNs) and are skipped.
- **Lightmap block.** The model tail ends with a `u32` byte count. Then, for every
  `SURF_LIGHTMAP` polygon in polygon order, it stores `u8` width, `u8` height and
  width × height RGB565 texels, row-major along P.
  - A visible entry is either 0×0 (no lightmap: invisible faces and a few others) or
    exactly `extent / 20 + 1` in each direction.
  - Sky portals store sizes unrelated to their extents.
  - Stored black lightmaps are explicit zeros, not empty entries.

The bytes before the block (a grid header with 500-unit cells and the root minimum,
followed by per-cell lists) remain opaque and are kept as `opaque_bytes`. The reader
locates the block by its exact length and validates every entry. It accepts the block
only when exactly one offset validates; otherwise `WorldModel::lightmaps` stays false
and the model keeps its baked vertex light. Every model of the 57 readable retail worlds
validates. `16_bridge` fails earlier, in its object properties, an unrelated existing
limitation.

## Texel placement

A texel center lies on the 20-unit grid of `dot(P, point)`:
`texel = (dot(P, point) - origin) / 20`, and `+ .5` for sampling. The origin is the
polygon minimum along the axis, snapped down to the grid. A minimum lying exactly on a
grid line goes to either neighbor depending on the original preprocessor's float noise.
`lightmap_origin` therefore takes the snapped value, the line above and the line below,
and keeps the one whose span the stored texel count covers.

- **Without that check**, a plain snap fits 98.7–99 % of axes.
- **With it**, all axes of the 57 readable worlds fit (`WorldMesh::lightmap_unfitted` = 0).

Sampling the stored texels at polygon vertices correlates with the baked vertex
colors (0.85 in SubwayA) only in this orientation; swapped or flipped axes correlate
worse.

## Composition

Blood II ships `defaults.cfg` with `"Saturate" "1"`, and the 1.x renderer exposes
`Saturate`/`LightSaturate`. In later LithTech, Saturate makes the lightmap texture pass
`MODULATE2X`/`SRCCOLOR` while plain Gouraud stays unscaled. The fragment shaders
therefore compute:

- lightmapped faces: `albedo * world_saturate_scale (2) * min(lightmap + radial light, 1)`,
  following the original order (lightmap, dynamic lights added in the framebuffer, then
  the texture multiply);
- other faces: `albedo * (vertex light + radial light)`, unchanged.

`SURF_BRIGHT` faces stay fully bright; unresolved materials and sky portals take no
lightmap. Translucent lightmapped faces use their lightmap in the alpha pass. Model
sampling (`WorldShade`) reads the hit face's lightmap bilinearly, with the same scale,
so models match the floor brightness.

## GPU data

`WorldMesh::build` shelf-packs lightmaps into 1024×1024 RGBA8 pages, giving each a
one-texel replicated border. Each page is a normal `WorldMaterial` named
`lightmap page N`, so the registry uploads and releases it with the world revision.
Batches group by `(material, sky, pan rate, lightmap page)`.

`WorldVertex` gains `lightmap` page coordinates (negative = vertex light) and grows from
32 to 40 bytes; the three-million-vertex budget is now 120 MB. The vertex shaders pass
the coordinate as `TEXCOORD3` (skinned models output -1). Both fragments sample
`t1/s1` with a bilinear, clamped, single-level sampler (`GpuRegistry::lightmap_sampler`).
Faces without a lightmap bind white there, and model draws bind white as well. The
fragment uniform's `modulation.y` carries the lightmap scale. All eight shader artifacts
were regenerated.

## Inspection and measurement

The inspector and `--bench` report lightmapped faces, pages, texels and unfitted axes.

| | SubwayA | CityHub |
|---|---|---|
| Lightmapped faces | 4,166 | 9,088 |
| Pages | 1 | 2 |
| Texels | 213,675 | 358,870 |
| Vertex-lit faces | 31 (glass) | 900 |

Across the 57 readable worlds there are 271,444 lightmapped faces, at most 3 pages per
world, and reading plus building all of them takes 0.6 s.

Measured on Apple M5 / Metal at 3440x1440:

| Metric | Before | After |
|---|---|---|
| CityHub frame mean | 1.72 ms | 1.74–1.75 ms |
| CityHub GPU | 0.568 ms | 0.58 ms |
| CityHub draw calls | 238 | 296 (batches split per page) |
| CityHub record | 0.067 ms | 0.080 ms |
| SubwayA frame mean (not measured before at this size) | — | 0.74 ms |

## Tests

- `world_lighting.origin`: snap, on-line, noise and no-fit cases.
- `world_lighting.lightmap`: RGB565 expansion, page placement, border replication,
  texel-center coordinates, batch page, model sampling, and the bright/sky/empty/unlocated
  exclusions. An out-of-range reference fails the build.
- `world_lighting.retail` (private data): every SubwayA/CityHub model locates its block,
  with no unfitted axes.
- `gpu.lightmaps`: the 2× scale, the radial light added before the scale, unchanged vertex
  faces, and rejection of a missing page.
- `world_shaders.formats`: now expects two fragment samplers and varying 3.

## Pending

- The opaque pre-block grid/cell data, possibly object placement, is still undecoded.
- No light-group animation (none observed).
- Dynamic lights still ignore occlusion, as in the original.
- Not yet covered:
  - static-light model lighting with normals;
  - model shadows (`detailhi.cfg` enables `MaxModelShadows 1`, `ShadowZRange 17`, `ShadowLodOffset 100`);
  - realtime lights.

The [model shadow continuation](model_shadow_contracts.md) darkens the lightmap (or vertex light) of world receivers under `FLAG_SHADOW` casters before the dynamic light is added. Both fragment shaders now bind a third texture (the shadow tiles) and a second uniform buffer.
