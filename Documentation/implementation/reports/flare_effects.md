# Original flare effects — VFX lot 5

2026-10-25, macOS arm64 / Apple M5. Built on shared-ballistics commit `f49815f8`.
[Contract](../../contracts/flare_contracts.md#original-vfx-completion--lot-5-2026-10-25),
[source hashes](../sources/flare_effect_sources.json).

Primary/fragment sparks and fizzle/burst/fragment composites now use their original
high-detail recipes and retail resources. The alternate parent remains trail-free.
The common MOVINGSOURCE render path now uses the fixed system origin, removing the
extra displacement of moving trails. Save/load republishes one trail per owner;
projectile damage and saved bytes do not change.

## Verification

- Release application and inspector syntax check passed. Relevant headless checks:
  23 passed, including all flare gameplay, actual retail resources, NPC fire,
  object-particle regression tests and the complete existing pickup/projectile save
  suite (private certification included). Restore assertions check one trail per owner.
- Five GPU cases passed: synthetic/retail flare sprite and new effects, original dual
  view models, synthetic/retail particle tint. Initial flare GPU probes incorrectly
  compared local quad vertices to world coordinates; corrected to inspect the draw
  transform and reran both failing cases successfully. Preserve both logs below.
- New checks cover four/two particles without distance sampling, cached-axis emission,
  yellow color, stop/ramp/strict expiry, pool refusal before RNG consumption, source
  component selection, absence of an alternate-parent trail and owner/world cleanup.
- GPU captures cover recipes 6/8 and 13/14/15 with decoded retail textures and assert
  nonempty rendering. A moved-source draw-position assertion protects the shared
  translation correction. Fizzle capture was visually inspected (yellow ring + smoke).
- ASan/UBSan: 18/18 focused headless checks passed plus a direct retail effects run;
  LeakSanitizer is unavailable on this macOS and was disabled explicitly.
- Quest APK assembly passed; no simulator, emulator or headset install/run.

Evidence: [Release initial run and GPU retest](evidence/vfx5-release.txt),
[sanitizers](evidence/vfx5-sanitize.txt), [Quest assembly](evidence/vfx5-quest.txt).
Local captures: `build/release/tests/flare-private-gpu/effect-{6,8,13,14,15}.png`.

This verifies source rules and replacement rendering, not a matched run of the original
executable. Low/medium-detail alternatives, owner/explosion light illumination and global
translucent ordering remain outside this lot. M2's GPU reservation and M3 campaign
acceptance are unchanged.
