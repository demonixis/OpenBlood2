# Enhanced renderer (physically based shading)

Options > Display can switch world faces and lit models from the original shaders to an
enhanced, physically based set. The original renderer stays the default and the
reference: its shaders, pipelines and pixel output are untouched, and switching back
reproduces the original frame byte for byte (`gpu.surface`). Everything below is
replacement rendering. The 1998 game has no material classes, normal maps or specular
light, so no retail parity is claimed. Roadmap items 5.0, 5.1 (without external maps),
5.3 and 5.4 are covered. 5.2 (emissive) and 5.5 (material inspection) remain open. The
[source ledger](../implementation/sources/enhanced_renderer_sources.json) lists the
published techniques used.

## Selection

| Setting | Values | Effect |
|---|---|---|
| `RENDERER` (setting 8) | ORIGINAL / ENHANCED | The original set, or the enhanced set for world faces and lit models. |
| `LIGHTING` (setting 7) | PER VERTEX / PER PIXEL (original); STANDARD / HIGH (enhanced) | Original: model light per vertex or per pixel. Enhanced: profile, see below. |
| `NORMAL MAPS` (setting 9) | GENERATED / OFF | Enhanced only (the row is disabled with ORIGINAL). |

The two lighting choices are stored separately (`pixel_lighting`, `quality`), so
switching the renderer keeps each one. `preferences.cfg` version 3 adds `renderer`,
`quality` and `normal_maps`. Versions 1 and 2 still load, with the renderer defaults
(original, standard, generated). Any version rejects the keys of a later one.

The bench and direct paths accept `--renderer original|standard|high` and
`--normal-maps`. The normal frontend applies the Display settings each frame instead.

| Profile | Grid lights per pixel | Specular cavity | Shaders |
|---|---|---|---|
| STANDARD | 4 | no | `pbr_frag`, `pbr_alpha_frag` |
| HIGH | 8 | yes (generated-map height) | `pbr_high_frag`, `pbr_high_alpha_frag` |

## What changes and what does not

- **Changed.** Opaque and translucent world faces, and lit GPU-skinned models (including
  view models). They use four new pipeline kinds: `SurfaceOpaque`, `SurfaceAlpha`,
  `SkinnedSurfaceOpaque` and `SkinnedSurfaceAlpha`. These are created only when the
  shading is not Original.
- **Unchanged, always original shaders.** Sky, sprites, particles, PolyGrids, explosion
  models, the shadow-atlas pass and the UI. Unlit, flat and `FLAG_NOLIGHT` models are
  drawn by the enhanced pipeline with material flag 4, which reproduces the original
  per-pixel model light.
- **Pipeline switch.** `ScenePresenter::shading` selects the set. `WorldPipelines::prepare`
  recreates all pipelines eagerly when the shading changes, so none is compiled lazily
  during play.
- **Vertex data.** `WorldVertex` gains the unit face-plane normal at vertex location 4.
  `world.vert` rotates it with the draw. The original fragment shaders do not read it.

## Shading model

The diffuse term is the original light so that rough dielectrics keep the original look:

- **World faces.** Lightmap ×2 with model shadows and the radial light, or the baked
  vertex light.
- **Models.** Ambient plus four sources, computed per pixel.

On top of it:

- **Direct specular.** GGX distribution, Hammon's height-correlated Smith visibility and
  Schlick Fresnel, times N·L and π, so that a light's 0-1 color keeps the original
  Lambert scale. It is lit by:
  - **world faces:** the static light objects from the light grid (×2 Saturate);
  - **models:** their four `ModelLighting` sources;
  - **both:** the radial point light (flashlight and similar).
- **Environment.** No probe and no cube map: the local baked light stands in for uniform
  environment radiance, through Karis' analytic split-sum approximation. Fdez-Agüera
  multiple-scattering compensation keeps rough metals from darkening against the
  original.
- **Metallic workflow.** F0 is `0.04 · specular` for dielectrics, or albedo for metals.
  Diffuse is weighted by `1 − metallic`.
- **Specular occlusion (world).** Static lights are not occluded in the grid. The baked
  light, which contains the preprocessor's shadows, bounds them: specular is scaled by
  `saturate(baked luminance / predicted luminance)`. The prediction uses the original
  light term, with the geometric N·L on lightmapped faces only (lightmaps include N·L,
  vertex light does not).
- **Normal-map relief (world).** The baked diffuse is scaled by the change in the listed
  lights' N·L between the mapped and geometric normals, relative to the baked luminance.
  A soft overhead term applies only to the baked light that no listed light explains.
  The factor is clamped to [0.25, 2].
- **Fully bright faces.** Faces without a lightmap whose face color is white
  (`SURF_BRIGHT`, unlit) return the original output.
- **Translucency.** Coverage rises to the reflected luminance, so highlights survive on
  nearly transparent glass. The output stays straight alpha for the existing blend.

There is no tangent stream. The tangent frame is the screen-derivative cotangent frame
(Schüler). World faces use the plane normal, models their interpolated authored normal.

## Materials

The built-in manifest is `config/materials.json`. CMake embeds it into
`generated/surface_manifest.h` at configure time, and editing the file reconfigures.
`SurfaceManifest` reads version 1 with a bounded strict JSON reader:

- 64 KiB, depth 8, 16384 values, strings of 512 bytes;
- no `\u` escapes, no duplicate keys;
- unknown keys and out-of-range values rejected;
- a failure leaves the previous manifest intact.

Its contents:

- **Classes.** `roughness` [0.02, 1], `metallic` [0, 1], `specular` (F0 scale, [0, 4])
  and `normal` (generated relief strength, [0, 4]; 0 disables it).
- **Rules.** Ordered; the first match wins. `*` spans any run including `/`, `?` matches
  one character, matching is case-insensitive and `\` equals `/`. The matcher is
  iterative with single-star backtracking, so it is linear-bounded. Unlike the donor, it
  honors every star.
- **Textures.** Exact per-path overrides.

Unmatched paths resolve to `default` (roughness 0.8, dielectric).

The NoMadEngine donor patterns (`TEXTURES/BRICK*` and so on) match nothing in the retail
catalogue. Retail world textures are grouped by color family (`textures/greys`, `reds`,
`blueyellow`), so the rules match words in the file name. `surface.retail` measures the
result:

- 3825 DTX in total;
- 1859 world textures, of which 618 (33%) resolve to `default`;
- the other 3825 − 1859 are skins, sprites and sky textures (classified only where a rule
  applies).

Per-material parameters are resolved once per material when a renderer prepares it.
They are pushed as fragment uniform slot 3 only when they change between batches. Slot
2 carries the camera and the grid description and is pushed once per record.

## Light grid

`build_light_grid` bins every `Light`/`DirLight`/`ObjectLight` sphere (the same static
lights as [model lighting](static_light_contracts.md)) into a world grid on the CPU. It
runs once per world revision. There is no baking, ray casting or probe.

- **Cells.** At least 128 units, at most 65536 cells (the cell grows by 1.25× until it
  fits).
- **Per cell.** At most 8 lights, strongest first by luminance × the original falloff at
  the nearest cell point. Extra references are counted as `dropped`.
- **GPU data.** Two read-only fragment storage buffers: three `float4` per light, then
  the packed cells (`first << 4 | count`) followed by the light indices. Points outside
  the grid get no light.

Measured on Apple M5 (`gpu.surface`): CityHub has 540 lights in 45540 cells (3.6 ms),
SubwayA 371 lights in 10920 cells (1.1 ms).

## Generated normal maps

`generate_normal_map` derives a tangent-space map from the albedo:

- **Height.** Luminance (transparent texels are flat).
- **Slopes.** A wrapping 3×3 Sobel filter scaled by `strength · 3`.
- **Mip levels.** Every level is rebuilt from the averaged finer normals, renormalized.
- **Encoding.** RGB holds the normal, A the height, which HIGH uses as specular cavity.

The normal map is sampled with a trilinear repeating sampler. Albedo keeps its original
point sampling.

`NormalMapCache` is content-addressed (albedo pixels and strength). It keeps maps across
world changes and is bounded to 192 MiB, evicting least recently used entries. The GPU
copies are registry content textures (`derived_texture("normal", …)`) released at each
world revision like other material textures.

Generation is cheap enough that no disk cache is kept. All 1851 retail world textures
take 204 ms in total (~0.1 ms each, 131 MiB), CityHub 35 ms and SubwayA 10 ms. Reading
the same bytes from disk would cost more than generating them.

Lightmap pages, the white slot, failed materials and animated (SPR) materials get no map.

## Cost

Headless bench, 1920×1080, Apple M5 Metal, CityHub start, GPU completion wait:

| Renderer | GPU time |
|---|---|
| Original | 0.61 ms |
| Enhanced STANDARD | 1.25 ms |
| Enhanced HIGH + normal maps | 1.81 ms |

The extra CPU per frame is one storage-buffer bind and one frame uniform per renderer,
plus a 16-byte material push only when the material changes. Shader artifacts grow by
eight files, about 280 KB of headers in total, and the build time is unchanged.

## Verification

- `surface.glob`, `surface.manifest`, `surface.builtin`, `surface.normal_maps`,
  `surface.light_grid` (headless) and `surface.retail` (private catalogue).
- `menu` and `menu_preferences`: the new rows and the version 3 migration.
- `gpu.surface` on CityHub and SubwayA. The STANDARD and HIGH frames differ from the
  original by a mean of 2-6 levels per channel, with fewer than 1% of channels moving by
  more than 64. Generated maps exist only with the option. Returning to Original
  restores the byte-identical original frame.
- The full GPU label passes 121 of 122 tests, including lightmap, model-shadow and
  world-alpha readbacks.
  - `gpu.entry_wait` fails because it launches without `--view-world` and the menu now
    opens first, so no world loads.
  - The 7 private catalog/AI headless failures also fail at the base commit `a96c5b66`.
- The original set's fragment artifacts are byte-identical. Only `world.vert` changed, to
  pass the plane normal.

Native Vulkan execution of the new shaders has not been verified on Linux or Windows.
Only Metal on Apple M5 was run.
