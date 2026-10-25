# Scene effect rendering — VFX lot 10

2026-10-25. [Contract](../../contracts/scene_effects_contracts.md),
[source ledger](../sources/scene_effect_sources.json). Continues VFX lots 4–9 after
the user confirmed that the other computer's graphics changes are integrated.

Implemented current light-state consumption on world/models, projectile owner lights,
volume fog with world-fog restoration, a shared translucent draw order and clipped
ballistic marks. Original source timing and recipes stay in the existing stores;
no new simulation object, RNG draw or save payload is introduced. All 72 shader
artifacts were regenerated together. Inspector diagnostics expose the 16-light budget,
fog-only light omissions, unshadowed effect-light requests and invalid fog intervals.

## Verification

- **State and ownership:** visible light selection, stable budget/ties, original
  flag propagation, fog-light exclusion, delayed light expiry, projectile pose and
  impact removal, world/volume fog precedence, exit, disabled volume override,
  invalid interval and unload. No resource dependency in the ownership probe.
- **Clipping and queue:** independent polygon edge/UV and rotated-pose checks,
  oversized polygon refusal, interleaving every transparent family, splitting and
  rejoining contiguous sprite slices, actual ballistic publication through a short
  world ray and a clipped sprite with its original plane bias.
- **GPU:** original-shader point-light addition and fog values; no-effect, close-view
  and sprite-light exclusion match the reference bytes. A back-facing-light flag
  suppresses illumination. Two sprite slices straddle a translucent world face and
  produce the independently expected red/green/blue alpha composition. Out-of-batch
  slices are refused; a clipped mark leaves the outside pixel untouched.
- **Existing integration:** shader formats, sprite geometry/resources, explosion
  sprite formulas, ballistics and shotgun assets, world alpha, flashlight, model
  shadows, original/enhanced surface reference, post-processing, energy-effect GPU
  scenes, and the train session both with and without save/load pass.

The Release selection contains **27 checks**. The first run passed 24 and exposed
three stale expectations; after the corrections below all three reruns passed.
[Initial evidence](evidence/vfx10-release-initial.txt),
[corrected checks](evidence/vfx10-release-corrections.txt).
A final scene integration run also passes 4/4 after adding the non-compositor XR fog
fade: both stereo layers are black at fade zero. [Evidence](evidence/vfx10-final-integration.txt).
The initial small ownership/clipping fixtures also needed the normal precreate
acceptance and solid surface flag; these are fixture corrections, not runtime bypasses.

The two world-alpha tests still assumed point-sampled texels after the earlier texture
sharpness change to bilinear/anisotropic sampling. Rebuilding only the shader module
with the checked-in **35122a8a** artifacts reproduced the same failure and an identical
PNG, SHA-256 `6fbe49ed5393ff068c2f38af328405bbb3f3b5add81c2431f47e9b081b105ad5`.
The tests now use an independent repeat-addressed bilinear texel oracle and retain
their one-byte tolerance, occlusion, no-depth-write, resize and retail-glass assertions.
[Control evidence](evidence/vfx10-alpha-baseline.txt). The third stale assertion expected
the now-removed “light illumination” omission string; source recipe counts remain checked.

**ASan/UBSan:** 9/9 state, clipping, sprite and ballistic checks pass, with macOS
LeakSanitizer disabled as in the existing suite. [Evidence](evidence/vfx10-asan.txt).
The Release application, inspector-enabled translation units and **Quest APK build**
pass; no emulator, simulator or headset was launched. [Quest build](evidence/vfx10-quest.txt).

## Remaining qualification

The cap, radial light approximation, unsorted intersections within individual particle
systems, source mark-ray correction and stateless same-code-volume fog update are
explicit adaptations. SkyFog, volumetric fog lights and shadows cast by FX lights
remain separate. Existing model shadows are preserved. GPU probes and source recipe
coverage do not replace matched captures from the original executable or a full
campaign walkthrough. Conditional P3 Rain/legacy emitters remain unpromoted without
an evidenced live caller. M2 remains closed with M2-GPU-001; M3 remains unaccepted.

## Forward continuation

The subsequent [per-receiver forward light selection](forward_effect_lights.md)
replaces this report's initial 16-light camera-wide budget with 12 mobile / 32 desktop
lights selected independently for every model instance and world material batch.
The evidence above retains its original scope and bytes.
