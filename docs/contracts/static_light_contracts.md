# Static light objects and model lighting

GPU-skinned models are now lit by the world's preprocessor light objects. The light term
comes from the original data; Lambert N.L shading, the four-source limit and occlusion
are replacement policies, so this is not claimed as retail parity. It supersedes the
floor-sampling replacement of the [baked lighting continuation](baked_lighting_contracts.md).
Reference corroboration is recorded in the
[source ledger](../implementation/sources/static_light_sources.json).

## Original light term

`Light`, `DirLight` and `ObjectLight` are `CF_NORUNTIME` editor classes. Each one with
`LightObjects` yields a `StaticLight` (`src/engine/world_lights.*`) built from its DAT
properties:

- `Pos` and `LightRadius`;
- `LightColor` (`InnerColor` for `DirLight`) × `BrightScale` / 255;
- for `DirLight`, a cone from `FOV` and the `Rotation` forward vector;
- `ClipLight`.

`GlobalDirLight` lights only the world and is ignored. Invalid values are counted and
skipped. Every retail `OuterColor` is zero; nonzero values are counted but unsupported.

The preprocessor's per-vertex colors are reproduced exactly by:

```
ambient + Σ color · (1 − d/R) · cone,   cone = (cos θ − cos ½FOV) / (1 − cos ½FOV)
```

with no N.L and no occlusion. The term is checked against the baked colors at ±3 per
channel:

| World | Vertices within ±3 |
|---|---|
| SubwayA | 4,000/4,000 (100 %) |
| Steam Tunnels | 99 % |
| Train Station | 99.8 % |
| CityHub | 95.5 % |

Linear, quadratic and quartic falloffs, hard or linear cones and N.L were all tested; only
this combination fits. The remaining CityHub vertices are predicted brighter than stored,
so another source darkens them. `static_light_at` implements the term.

## Model light

`ModelLighting` (`src/app/model_lighting.*`) evaluates each light at a model's world
bounds center (view models: the camera position).

1. **Occlusion.** `ClipLight` lights are skipped when a textured opaque world face blocks
   the segment. `WorldShade::blocked` tests the segment with one-unit end margins and the
   current range transforms.
2. **Selection.** The four strongest remaining lights become per-vertex sources (ties
   keep authored order). The others add their center value to the ambient.
3. **Ambient.** World `AmbientLight` + `ModelAdd` + additive `SetObjectColor` + those
   remainders, clamped to 0–1.
4. **Shading.** `skinned.vert` evaluates the same term per vertex with Lambert N.L, using
   the ABC normal rotated by the node and object rotation, and clamps the sum to 1.
   `FLAG_MODELGOURAUDSHADE` marks Gouraud model shading, which is why N.L applies; the
   original's exact model formula is unknown.
5. **Model kinds.**
   - Models without `FLAG_MODELGOURAUDSHADE` take every term at the center without N.L
     (flat, like the vertex colors).
   - `FLAG_NOLIGHT` models use their object color.
   - Explosion FX stay fully bright, as before.
6. **Scale.** N.L terms are doubled after the clamp, like the lightmaps (`world_saturate_scale`,
   from the shipped `Saturate` default), because both include N.L:
   - Lightmap texels measure 1.0–1.4× the N.L term, while baked vertex colors omit N.L.
   - Flat and unlit models stay ×1.

The uniform carries `ambient` (w = source count) and three float4 per source; the skinned
uniform is 384 bytes. `SkinnedVertex` gains the authored normal (location 4). The radial
point light still adds in the fragment shader. Samples are reused per object while its
center is unchanged and no moving world range overlaps the box spanning the center and its
tested lights. Default instances (CPU path, tests) remain fully bright.

## Inspection and measurement

The inspector and `--bench` report:

- static lights (without `LightObjects`, invalid, `OuterColor`);
- models: sampled, reused, flat, `FLAG_NOLIGHT`;
- directional sources and occluded lights;
- occlusion traces and blocked traces.

Retail sessions have 371 (SubwayA) and 540 (CityHub) static lights. In CityHub, 204 of
the 232 models are flat (props without the Gouraud flag), and its characters get 86
directional sources in total.

Measured on Apple M5 / Metal:

| Scene | Before | After |
|---|---|---|
| CityHub 3440x1440 frame | 1.74–1.75 ms | 1.75–1.80 ms |
| CityHub models stage | 0.098 ms | 0.115 ms |
| SubwayA 1600x900 frame | — | 0.63–0.67 ms |

The model stage now includes selection, but samples are reused (230/232).

## Tests

- `world_lighting.lights`: property reading, `LightObjects`/invalid filtering, Euler cone
  axis, linear falloff and cone factor.
- `world_lighting.retail` (private data):
  - the term reproduces the baked vertex colors (SubwayA 1,776/1,776 sampled vertices,
    CityHub ≥ 90 %);
  - sessions produce directional sources, ordered strongest first;
  - ambients stay within 0–1.
- `gpu.model_skinning` still shows pixel-identical CPU/GPU skinning for unlit instances.

## Pending

- `ModelDirAdd`, `GlobalDirLight`/`ObjectLight` sun cases beyond point lights, and dynamic
  lights (explosions, muzzle flashes, `GlowingLight`) on models.
- Exact original model shading (N.L, source limit, occlusion).
- `FLAG_SHADOW` model shadows (next continuation).
