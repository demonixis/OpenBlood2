# Destruction debris — lot 1, 2026-10-25

CDebris now creates visible wood, metal and glass fragments, through the existing client
model/physics path. The [contract](../../contracts/destruction_debris_contracts.md) records
original rules and bounded adaptations; the [source ledger](../sources/destruction_debris_sources.json)
records consulted source hashes. The roadmap tracks this under beta feedback (M3.7/M4).

## Verification

- Release application and affected targets build on macOS arm64; the changed inspector
  translation unit separately compiles with `BLOOD2_INSPECTOR=1`.
- **36 targeted tests pass**: destruction_debris, brushes, destructable_models, death_effects,
  flesh_gib_collision, plus GPU destruction/death effects. They cover real publication,
  CPU/GPU-skinned transparency, material scale/launch/counts, custom paths, explicit failure,
  source removal, multi-material direction sign/order, rebounces/audio, rest, eviction/fade,
  capacity and world cleanup. Existing flesh behavior stays green.
- **Seven ASan/UBSan tests pass**: the three synthetic debris cases and four shared
  fragment-collision cases, using `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1`. LeakSanitizer is unavailable on this macOS ARM host,
  as established in the preceding control-action run; no leak-check pass is claimed.
- Retail CityHub damage actually destroys `models/worldobjects/clotheswasher.abc` (metal),
  `WindowSlats` (wood) and `shack` (metal/wood/glass); creates external ABC/DTX fragments and
  then runs ten seconds with successful fragment motion and impact audio. The washer also
  requests energy debris, which remains explicitly unsupported by this lot. This is selected
  real-object coverage, not a claim to have manually visited every crate/bin/window.
- `gpu.destruction_debris` renders each material, checks stable repeated pixels, verifies
  glass 0.4 alpha differs from opaque output, motion changes the image, and reset clears the
  scene. Captures use real retail fragment assets and were visually inspected.
- CityHub save audit passes at difficulty 1, including combat, restore and subsequent
  simulation: **348 records / 388163 bytes**. Cosmetic client fragments are cleared on load;
  no new saved classes or payload changes are introduced.
- Quest APK assembly succeeds (`cmake --build build/quest --target blood2_apk -j6`). No
  emulator, simulator or headset run. Windows/Vulkan execution is not claimed here.

Logs and copyrighted-asset captures remain in ignored `build/release/tests/destruction-debris-evidence/`
and `build/release/tests/destruction-debris-gpu/`. Other materials, generic recursive gibs,
weapon effects and the unreproduced Museum crash remain separate work. This does not accept
M3 or reopen M2-GPU-001.
