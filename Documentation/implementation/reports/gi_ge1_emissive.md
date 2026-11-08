# GI lot GE1: emissive texels in the bake

2026-11-08, lot GE1 of the [GI plan](../gi_plan.md) (D10 (b), decisions D23 and D24), after lot
[GQ1](gi_gq1_bake_quality.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#emissive-texels-lot-ge1). Host: Apple M5, macOS,
Metal, `dev` build, 10 jobs, the four test worlds.

## What landed

- **Setting:** `--gi-emissive S` (0–4, two decimals, any preset), `bake_all.py --emissive`, and the F8
  Bake panel's "Emissive textures light the bake" with its scale. The settings line `emissive S M`
  carries the scale and the first 16 hex digits of the built-in manifest's SHA-256, so editing
  `config/materials.json` makes the file stale under `--gi-if-stale`.
- **Chains (D24):** `GiBakeScene::prepare_emission` resolves materials like the enhanced renderer
  (DTX paths only) and computes the shader's `emitted()` per mip-0 texel: c × strength ×
  saturate((luminance − threshold) / max(1 − threshold, 0.001)). Each further level is the box
  filter of the level above, not the authored mip, so small bright spots keep their energy.
- **Lookup:** `surface_radiance` adds the emission at the face's albedo mip to lightmapped and
  vertex-lit faces only. SURF_BRIGHT faces add nothing, because the renderer does not glow there.
  The lightmap passes, the light probes and both reflection sets see it.
- **Report:** `GI_RESULT` gains `emissive_materials`, `emissive_faces`, `emissive_texels` and
  `emissive_ms`; `bake_all.py` records them.
- **Tests:** `gi_lightmap.emission` (new) and additions to `gi_bake.settings`, `config.gi`,
  `gi_bake_process.parse` and `gi_overlay`.

## Verification

- **Identity:** without `--gi-emissive` (or with 0) the bake is byte for byte the GQ1 one. Final
  bakes of the four worlds equal the G11 bakes.
- **Analytic floor** (`gi_lightmap.emission`): an unlit emissive ceiling lights a floor by 0.6 ×
  0.6 × F, with F the form factor (worst error 4.6·10⁻⁴ at 4,096 rays). Scale 0.5 gives half; 1 and
  2 jobs give the same bytes. A SURF_BRIGHT ceiling of the same class adds nothing. Scale 0 restores
  the plain bake bit for bit.
- **Chains:** a checker texture's mip 1 is the box filter (1.0, half of twice white) while its
  authored mip is white. A class whose threshold no texel exceeds is left out.

## Four worlds

| World | Emissive materials | Faces | Glowing mip-0 texels | Final → + emission | Ultra → + emission |
| --- | --- | --- | --- | --- | --- |
| 01_SubwayA | 4 | 152 | 13,618 | 2,487 → 2,693 ms | 8,497 → 9,041 ms |
| 02_CityHub | 8 | 176 | 33,345 | 6,264 → 6,629 ms | 20,887 → 21,423 ms |
| 03_Museum | 6 | 90 | 8,042 | 8,019 → 8,347 ms | 26,596 → 27,099 ms |
| 06_Airship2 | 11 | 748 | 16,201 | 5,247 → 5,398 ms | 16,652 → 16,915 ms |

Emission adds 2–8 % to a bake and no memory to speak of. Only the settings line changes the file
size (31 bytes).

## The Gate H A/B

Captures at each world's start point (1280 × 720, enhanced renderer, LIGHTING HIGH, GI on,
REFLECTION PROBES HIGH), compared with the Final bake: pixels changed, then mean luminance.

| World | Final + emission | Ultra | Ultra + emission |
| --- | --- | --- | --- |
| 01_SubwayA | 19.6 %, 21.22 → 21.31 | 39.7 %, 21.28 | 46.3 %, 21.36 |
| 02_CityHub | 6.2 %, 34.23 → 34.25 | 39.0 %, 33.71 | 40.4 %, 33.74 |
| 03_Museum | 0.9 %, 60.32 → 60.33 | 62.9 %, 60.71 | 63.1 %, 60.71 |
| 06_Airship2 | 24.4 %, 29.97 → 30.08 | 89.1 %, 31.52 | 90.7 %, 31.64 |

At the start points emission is subtle: it brightens surfaces near emissive panels and lamps by a
few luminance steps and leaves the rest. Ultra changes more of the frame: softer shadow edges and
less noise.

**Playing it:** bake a world with the F8 Bake panel (Final or Ultra, with or without "Emissive
textures light the bake"), or with the command line into separate folders, then run the game with
`--gi-dir <folder>`, Options > Display GLOBAL ILLUMINATION on. The bakes and captures stay local,
because they contain retail art (decision D3).

## Open

- **Gate H:** the default preset (Final or Ultra) and whether emission is on by default (and at
  which scale).
- The full rebake of the 36 worlds, at the end of the GI work.
