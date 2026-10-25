# Explosive weapon effects — VFX lot 7

2026-10-25, macOS arm64 / Apple M5; follows Tesla commit `0a84aecc`.
[Contract](../../contracts/explosive_effects_contracts.md),
[source hashes](../sources/explosive_effect_sources.json).

Impacts 5/9/10/11 now use their authored model/sprite/ring/debris/light-state recipes.
Grenades and Howitzer child projectiles use the existing smoke-1 emitter; both Napalm
modes use the newly connected smoke-2 comet tail. Owner restore republishes each trail
once. Damaging children, collision, damage and save payloads are unchanged.

## Verification

- Release targeted suite: 29 headless cases, including source recipes, actual projectile
  publishers/children, smoke emission, M16 grenades, object particles and saves. Initial
  two new probes used an exact double .05 below the authored float delay and assumed a
  weapon-level half-damage step in a direct projectile fixture. Corrected the probes
  to .051 and the fixture's actual parent damage / 4; all three new cases passed.
- Further regressions: 38/38 covering all explosion stores, pickup/projectile saves and
  the campaign actor path. Six save cases overlap the targeted suite. Existing unsupported
  type-2 validation now checks a genuinely unsupported ID; missing-resource assertions stay.
- New retail GPU test passed for all four composite impacts and both smoke recipes.
  Geometry/material resolution and visible pixels are asserted. Howitzer primary and
  Napalm smoke captures were inspected. This is not original-executable A/B acceptance.
- ASan/UBSan: 22/22 synthetic headless checks plus all three direct retail effect cases
  passed. LeakSanitizer explicitly disabled on macOS. Restore assertions verify one trail
  for grenade, Napalm and Howitzer fragments; no saved bytes were introduced.
- Release application, inspector syntax and Quest APK assembly passed. No simulator,
  emulator or headset install/run. No shaders, dependencies or commercial assets changed.

Evidence: [initial targeted run and corrected probes](evidence/vfx7-release.txt),
[regressions](evidence/vfx7-regressions.txt), [sanitizers](evidence/vfx7-sanitize.txt),
[Quest assembly](evidence/vfx7-quest.txt). Captures:
`build/release/tests/explosive-effects-gpu/explosion-{5,9,10,11}.png` and `smoke-{1,2}.png`.

High-detail recipes, bounded replacement particle/fragment physics and portable cosmetic
randomness remain explicit limits. Geometry illumination remains for lot 10. The user
confirmed renderer synchronization while this lot was running. M2-GPU-001 and M3 campaign
acceptance remain unchanged.
