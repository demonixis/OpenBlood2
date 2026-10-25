# Ballistic VFX lot 4 — 2026-10-25

Shared ballistic presentation now extends the shotgun components to ordinary bullet/BMG
impacts, with per-ammo casing/smoke/mark/bubble rules and authored view-weapon flashes.
Original tracers use the shared hardware-line renderer. See the
[contract](../../contracts/ballistic_effects_contracts.md) for original quirks and
replacement boundaries, and the [source ledger](../sources/ballistic_effect_sources.json).

## Verification

macOS arm64, Apple M5, native Metal. Commercial data remains external; user directories
are isolated. No simulator, emulator or headset run was used.

| Check | Result |
| --- | --- |
| Inspector-free full Release build | Pass |
| Focused Release suite | 66 checks: 63 initially passed; the three stale test expectations were corrected and their three retests pass (61 headless / 5 GPU total). |
| ASan/UBSan focused suite | 14/14 pass; `detect_leaks=0` (unsupported macOS LeakSanitizer), both halt-on-error enabled. |
| ASan/UBSan retail muzzle/shotgun check | Pass |
| Inspector effect panel syntax, inspector enabled | Pass |
| Quest OpenXR APK, NDK 29 | Build and assembly pass only |
| Shader regeneration | All 72 generated; prior 68 byte-identical |
| Source hashes, local links, formatting and diff whitespace | Pass |

The three initial test failures remain recorded in the
[initial Release log](evidence/ballistic-effects-release-initial.log): the weapon-sprite
session needed to count and expire its new muzzle smoke; both Pan GPU cases sampled
between texels while expecting exact nearest colours. Their fixture now samples texel
centres under the existing trilinear sampler, preserving exact axis/clock assertions.
No production filtering change was made. [Retests](evidence/ballistic-effects-release-retest.log)
pass. [Sanitizer suite](evidence/ballistic-effects-sanitize.log) and
[retail sanitizer run](evidence/ballistic-effects-sanitize-retail.log) also pass.

The synthetic ballistic case checks bullet versus BMG/shell recipes, both ejection hands,
actual shared weapon fire, complete misses and Minigun suppression, four-bubble liquid
substitutions, the impact-normal tracer ray, near-hit rejection, stationary strict expiry
and world-reset cleanup. Existing shotgun tests retain both barrel modes and suppressed
first-contact behavior. Retail checks load every ballistic definition's authored muzzle
resources. Metal readback exercises shotgun/Beretta flashes, casings, and the three tracer
lines in Original, MSAA, enhanced auxiliary and reflection passes. `gpu.surface` and
`gpu.post` retain their original-reference and post-processing checks.

The native tracer capture was inspected: the expected thin yellow-to-red gradient is
visible. Captures remain in `build/release/tests/shotgun-effects-gpu/`. No original-executable
A/B recording, complete campaign traversal or VR visual acceptance is claimed.
