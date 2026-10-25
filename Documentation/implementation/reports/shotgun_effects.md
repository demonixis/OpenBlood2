# Shotgun VFX lot 2 — 2026-10-25

Weapon 4 now displays its original primary/alternate muzzle sprites, alternating barrel
animations and reload audio, secondary-only shell ejection/muzzle smoke, material marks
and wood/stone fragments, and liquid bubbles/splashes/ripples. The original sixteen-ray
server fire/damage path and shared spark/blood/impact-sprite implementations are preserved.
See the [contract and engine adaptations](../../contracts/shotgun_contracts.md#original-vfx-completion-lot-2-2026-10-25)
and [source hashes](../sources/shotgun_effect_sources.json).

## Verification

macOS arm64, Apple M5, native Metal; commercial assets are external.

| Check | Result |
| --- | --- |
| Inspector-free Release application build | Pass |
| Release focused CTest selection | **54/54 pass**: 53 headless, 1 GPU; 11 use retail data (including the GPU test) |
| Clang ASan/UBSan focused CTest selection | **32/32 pass**: object contracts, weapons, impact sprites, synthetic shotgun recipes/lifecycle/liquids/failures |
| ASan/UBSan retail shotgun effect and server weapon cases | **2/2 pass** |
| Museum whole-world save/load after combat, difficulty 1 | Pass: 444 records, 478303 bytes, post-restore simulation |
| Inspector effect panel syntax check, `BLOOD2_INSPECTOR=1` | Pass |
| Quest `blood2_apk` (NDK 29, OpenXR enabled) | Build/assembly pass; no installation, emulator or headset run |
| Formatting, diff whitespace and source hashes | Pass |

Focused test coverage includes one/two-shell costs, sixteen pellets, primary/secondary
flash selection, second-barrel alternate-to-primary conversion, real reload SOUND_KEY
playback, dry fire and complete misses, one ejection pair per secondary discharge,
mirrored casing launch, gravity/rebounds/audio, strict 20-second expiry, pool eviction,
mark-region reuse, underwater substitutions, splash lifetime, invalid payloads, missing
model resources with surviving other components, save-callback publication refusal and
world-reset cleanup. A headless tracked-arm test moves the muzzle flash by exactly the
controller translation; this is not headset visual acceptance.

The Metal readback test exercises the real sprite/model presentation functions with retail
art. Primary and secondary flashes change 9290 and 12947 pixels respectively at 400×300;
images differ. Casings submit two visible, textured 20-triangle models. Their DTX has zero
alpha: they therefore use the **ordinary opaque model pass**, like the source OT_MODEL,
not the alpha effect pass. Visual inspection confirms distinct simple/double flashes and
the red/brass casing. Captures stay in the ignored build tree:
`build/release/tests/shotgun-effects-gpu/{primary,secondary,casings}.png`.

A retail reload key uses doubled backslashes; normalization at the shotgun SOUND_KEY call
site preserves the strict audio service and restores playback. Initial build/test failures
for that path and the casing alpha pass were corrected before the final passing runs.

Evidence logs: [Release](evidence/shotgun-effects-release.log),
[ASan/UBSan](evidence/shotgun-effects-sanitize.log),
[ASan retail](evidence/shotgun-effects-sanitize-retail.log),
[ASan server fire](evidence/shotgun-effects-sanitize-weapon.log),
[Museum save audit](evidence/shotgun-effects-museum-save.log).
LeakSanitizer is unavailable on this macOS arm64 runtime (`detect_leaks=0`);
ASan/UBSan use `halt_on_error=1`. Tests isolate their user directories.

## Remaining scope

Existing camera-origin player shots, primary-world casing collision, unavailable polygon clipping, separate bounded FX pools
and retirement of empty parentless splashes remain explicit in the contract. No claim of
pixel-identical original rendering, full campaign traversal, a reproduced Museum crash
or headset validation is made. Remaining weapon/environment VFX inventory is lot 3.
M2 closure with M2-GPU-001 and M3 campaign acceptance remain unchanged.
