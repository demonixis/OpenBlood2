# FLAG_SHADOW model shadows

Models carrying the original `FLAG_SHADOW` now cast shadows onto upward-facing world
surfaces. The original game shows such shadows, and its shipped `detailhi.cfg` enables
`MaxModelShadows 1` (medium and low detail set 0). The technique is a replacement in the
spirit of LithTech's projected silhouette shadows: no retail shadow code or parameters
are available, so no parity is claimed. Reference corroboration is recorded in the
[source ledger](../implementation/sources/model_shadow_sources.json).

## Original evidence

- **Who casts.** The game only sets or clears `FLAG_SHADOW`:
  - players, AI and destructible models set it;
  - pickups, invisible or stealthed players and small destructibles clear it.
- **Who draws.** The engine draws the shadows; Blood II contains no shadow code.
- **Renderer settings.** The 1.x renderer exposes `MaxModelShadows`, `ShadowZRange` and
  `ShadowLodOffset`; their values are known (high detail: 1, 17, 100) but their meaning
  is not.
- **Later LithTech technique.** Its legacy path projects straight down (a `(0,-1,0)`
  "HACK" direction). Its live path renders the model silhouette into a small texture,
  projects it onto world polygons near the model along the strongest shadow-casting
  light, fades it with distance, and skips really-close (view) models.

## Technique

1. **Selection.** `ScenePresenter` selects visible GPU-skinned model instances whose object
   has `FLAG_SHADOW` and not `FLAG_REALLYCLOSE` (bit 6). It drops:
   - instances farther than 2048 units from the camera;
   - instances whose receiving region is outside the view frustum.

   The nearest 16 get one 128² tile of a 512² atlas.
2. **Light direction.** The strongest static source from the
   [model light](static_light_contracts.md) shines through the bounds center, at least 40°
   above the horizon. A model without a source projects straight down.
3. **Tiles.** One render pass before the scene draws each caster with the skinned shader,
   orthographically along the light: `x/z + slope · (y − top)` onto the box top. Depth is
   measured down from the top, and `shadow.frag` writes the world height (`R32_FLOAT`). The
   depth test keeps the topmost surface; uncovered texels keep a −1e30 sentinel. Hidden
   nodes stay omitted.
4. **Receivers.**
   - **Where.** World faces evaluate shadows in both fragment shaders (`modulation.z`): the
     opaque and translucent world passes, but not the sky, models, sprites or PolyGrids.
   - **Lookup.** A receiver projects onto its casters' tiles and takes a 2×2 coverage of
     texels whose caster height lies more than 2 units above it.
   - **Fade.** The result fades linearly over 96 units below the feet and requires an
     upward-facing derivative normal (normal.y above 0.25).
   - **Darkening.** It removes 60 % of the baked light (lightmap or vertex light) before
     the dynamic light is added, so shadows vanish in unlit areas and deepen under lamps.
   - **Early outs.** Faces that do not face up and points outside the union of receiving
     regions skip the caster loop.
5. **Uniform.** The per-record uniform (`ShadowUniform`, fragment slot 1) holds the count,
   strength, fade, texel size, camera, union bounds and three float4 per caster. Models
   and other draws push an empty one and bind white for slot 2.

The atlas uses `R32_FLOAT` color plus `D32_FLOAT` depth; it is checked once and created
on first use. `--no-model-shadows` disables the feature (the original low/medium detail),
and so does a missing format.

## Inspection and measurement

The inspector and `--bench` report FLAG_SHADOW candidates and drawn casters, plus those
beyond the limit, distant, outside the view or unbounded.

In the measured CityHub view, 74 objects carry FLAG_SHADOW and 5 are drawn.

Measured on Apple M5 / Metal, CityHub at 3440x1440:

| | Shadows off | Shadows on (5 casters) |
|---|---|---|
| GPU | 0.61 ms | 0.79 ms |
| Frame | 1.80 ms | 2.00 ms |

- **Before optimization:** the first version (32 casters, 1024² atlas, no culling) cost
  2.67 ms of GPU.
- **Remaining cost:** the tile pass plus receiver code on floor pixels.

## Tests

- `gpu.model_shadows` (private data) renders the same CityHub frame with and without shadows:
  - casters must be drawn and the frame uses two passes;
  - 1,477 pixels darken and no channel brightens, with Metal API validation enabled.
- `world_shaders.formats` checks the three-sampler, two-uniform fragments and the
  caster fragment.

## Pending and limits

- Shadows go only on the world: models never receive them, and the view weapon casts none.
- One light per caster, a fixed strength and 2×2 filtering, all replacement policy.
- Sun/`GlobalDirLight` worlds use the strongest static light, not the sun.
- `ShadowZRange`/`ShadowLodOffset` are not interpreted.
- Long shadows over ledges reach at most 96 units below the feet.
- Dynamic lights cast no shadows.
