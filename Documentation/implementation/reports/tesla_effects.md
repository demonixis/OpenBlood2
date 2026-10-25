# Tesla and Drudge effects — VFX lot 6

2026-10-25, macOS arm64 / Apple M5; follows flare commit `c1e93044`.
[Contract](../../contracts/tesla_contracts.md#original-vfx-completion--lot-6-2026-10-25),
[source hashes](../sources/tesla_effect_sources.json).

The primary impact now composes its original sphere, randomly scaled/rolled sprite,
delayed rotating ring and evaluated light state. Alternate fire, ball attraction and
Drudge lightning publish real detached lightning with each caller's source form.
Existing type-7 energy projectiles share the impact. Damage, target choice, ammo,
alternate counter, projectile/save behavior and missing owner-light illumination stay.

## Verification

- Release: 14/14 targeted cases (12 headless, two GPU): Tesla firing/ball/bolt/dry counter,
  first-hit and miss visual endpoints, impact component parameters, Drudge priest fire,
  energy projectiles and original lightning formulas/emitter/catalog.
- Regression: 48/48 headless cases covering all six explosion stores, flare effects,
  pickup/projectile saves and restored owner-bound trails.
- New GPU test isolates model, sprite and ring with actual retail assets, then both
  lightning forms. Each produces visible blue pixels; existing world-lightning GPU
  checks detached fade and expiry. Sprite and thin-to-thin beam captures inspected.
  No matched original-executable run is claimed.
- ASan/UBSan: six direct retail runs passed (Tesla weapon, priest, lightning formulas,
  emitter, catalog and energy projectiles). LeakSanitizer explicitly disabled on macOS.
  Known original missing Thunder.wav/Drudge audio remain reported, not substituted.
- Quest APK assembly passed; no simulator, emulator or headset install/run.
- Release app build and inspector syntax check passed after adding its explicit campaign
  weapon include. No dependency, shader or commercial-asset changes.

Evidence: [targeted Release](evidence/vfx6-release.txt),
[regressions](evidence/vfx6-regressions.txt), [sanitizers](evidence/vfx6-sanitize.txt),
[Quest assembly](evidence/vfx6-quest.txt).
Captures live under `build/release/tests/tesla-effects-gpu/tesla-{0,1,2,3,4}.png`.

Scope limits: original high-detail recipes, existing portable cosmetic random streams
and replacement rendering. Illumination and translucent ordering remain separate;
M2-GPU-001 and M3 campaign acceptance are unchanged.
