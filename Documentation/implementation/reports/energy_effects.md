# Energy, boss and rift effects — VFX lot 8

2026-10-25, macOS arm64 / Apple M5; follows lot 7 commit `6260992c`.
[Contract](../../contracts/energy_effects_contracts.md),
[source hashes](../sources/energy_effect_sources.json).

Restores composites 12/18/19/23–25/28–31, enemy trails 4/27/31 with their authored
lifetime scales, absorption sphere 9, Death Ray power rings 37/38 and electricity 39.
Source-absent components now succeed empty instead of reporting an invented omission.
Gameplay damage, source query order and saved field bodies stay unchanged.

## Verification

- Source-parameter/resource checks cover all ten new composite recipes and seven
  particle recipes with the retail resources, including two-model rotation retention,
  inward velocities, power-ring forward velocity, absence of Naga fragment trails,
  lifetime scaling, first-update suppression and checked owner removal.
- The retail GPU probe renders all ten composites and seven particle recipes, checks
  resolved materials and visible pixels, and writes captures under
  `build/release/tests/energy-effects-gpu/`. Singularity's capture was inspected.
  Small flame/particle fixtures use a nearer camera; the large rift uses a farther
  camera and later fade phase. Initial pixel-threshold failures were fixture timing
  and framing failures, not accepted as passes.
- Existing unsupported-recipe tests now use ID 255, since 24 is implemented. Existing
  Singularity and Spawner expectations now distinguish restored effects and genuine
  missing fixture resources. Save-family tests require exactly one restored trail
  for EnergyBlast, GroundStrike and Shikari as well as the earlier trail families.
- Release regression suite: 60/60 passed, including seven save cases. Application build and inspector syntax pass (one pre-existing boolean-bitwise warning).
- ASan/UBSan retail recipes/particles and synthetic save family pass, with macOS
  LeakSanitizer disabled. Quest APK assembly passes; no install, emulator or headset run.

Source parameter coverage and isolated port images do not certify original-executable
raster parity, LOW detail or complete campaign traversal. Rift 24's caller remains
unestablished. The user has confirmed synchronization for renderer lot 10; geometry
illumination is still separate from the light states restored here.

Evidence: [Release](evidence/vfx8-release.txt), [initial failures](evidence/vfx8-initial.txt), [Quest assembly](evidence/vfx8-quest.txt).
