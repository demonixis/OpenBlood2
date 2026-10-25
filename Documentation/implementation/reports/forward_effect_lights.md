# Per-receiver forward effect lighting

2026-10-25. Implements the requested continuation of VFX lot 10:
**12 effect lights per receiver on Android/Quest and iOS, 32 on desktop**.
[Contract](../../contracts/scene_effects_contracts.md).

The scene snapshot retains all valid live effect lights. Each GPU-skinned model
instance and each world material batch rejects light spheres outside its posed
bounds, then selects the strongest contributions. Ranking uses the maximum color
channel times linear attenuation at the nearest point on the receiver's bounds;
ties retain source order. World-only lights are removed before model ranking.
Camera distance no longer discards lights needed by a different object.

Selection uses a fixed 32-entry insertion list without per-draw allocation. World
batch bounds are cached at vertex upload; model bounds reuse the existing posed,
scaled skinning bounds. The draw order and geometry batches remain unchanged.
The shader ABI has 32 slots on every platform, but the counted loop evaluates only
the selected entries, capped at 12 on mobile. All 72 shader artifacts were regenerated
together. Inspector counters distinguish the full pool from per-receiver selected
and budget-dropped references.

## Verification

- **Selection/state, Release:** 2 headless tests pass. Covers both budgets, a light
  beyond the former global cutoff reaching a distant receiver, zero budget,
  world-only filtering, rotated long bounds, tangent rejection, contribution order,
  stable ties, conservative unknown bounds, invalid light input and fog preservation.
  Existing ownership, lifetime and scene-state cases remain green.
- **GPU, Metal:** `gpu.scene_effects` passes. Actual world batches and skinned model
  instances select independent lists with both budgets: 40 red lights surround one
  receiver while a later green light reaches the other. Pixel readback checks the
  expected light sum and absence of cross-object illumination. Strong world-only
  lights cannot consume model slots. Existing no-effect reference, fog, clipping and
  transparency checks also pass.
- **Regression:** 10 tests pass: shader formats, model skinning (synthetic and
  private data), world alpha (synthetic and private data), model shadows, original
  and enhanced surfaces, post-processing, flashlight and shotgun effects.
- **ASan/UBSan:** both headless selection/state tests pass. macOS LeakSanitizer is
  disabled for this run; this is not leak certification.
- **Builds:** inspector-free Release application and Quest APK build successfully.
  Inspector translation units pass syntax checking, with the existing boolean `|`
  warning in `inspector.cpp`. No device or simulator was run.

Raw evidence:
[targeted tests](evidence/forward-fx-tests.txt),
[regression](evidence/forward-fx-regression.txt),
[sanitizers](evidence/forward-fx-asan.txt),
[Quest build](evidence/forward-fx-quest.txt),
[inspector](evidence/forward-fx-inspector.txt),
[shader regeneration](evidence/forward-fx-shaders.txt).

## Boundaries

Bounds are conservative: disconnected geometry sharing a material batch can keep
lights in empty space inside its box. This continuation does not spatially subdivide
the existing batches. Unknown bounds retain candidates instead of inventing a point
receiver. The ranking is a diffuse-contribution bound, not a per-pixel specular ranking.
Selection scans the live pool per submitted receiver; no device performance result
is claimed by the build-only Quest check.

Static lights, flashlight, source VFX recipes and clocks, close-view weapons and
sprite illumination retain their existing policies. Effect-light shadows and fog-only
lights remain explicit omissions. M2-GPU-001, whole-campaign VFX acceptance and M3
closure are unchanged.
