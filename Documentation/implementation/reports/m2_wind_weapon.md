# M2: original Gideon wind and Behemoth shockwave

This increment connects Gideon's next weapon dependency: the original Wind
WeaponData entry and CBehemothShockwaveProjectile. Both firing modes create a
stationary one-shot. Its first original Update reads the shooter's current
position, projects a ground ray, then delivers full explosion damage to original
destructible objects within 450 units, excluding the shooter. This differs from
ordinary radius damage: no falloff, no visibility test, no AREAEFFECT flag, and
the target's position is passed as the damage position.

The unused CGideonWind vector push is not the route selected by the source weapon
definition. It stays unavailable rather than replacing the actual projectile
behavior. The reference ground ray reads an uninitialized endpoint; this port
defines it as shooter position plus the downward 1000-unit vector and documents
the existing replacement engine's entry-face query policy.

The original high-detail shockball model/skin, scaling, rotation and fading now
render through SDL_GPU. Its white light is evaluated and inspectable; it does
not illuminate surfaces. Actual windfire and explosion WAV playback execute.
Independent lifetime and missing-resource failures are preserved. Inspector
history retains the ground query, blast origin, recipients and interrupted
callback traversal after the projectile has been removed.

This is a weapon dependency, not a production boss fight. Gideon still needs
initialization, state/actions, its fire-node override, shield and powerup services.
Safe NEW_LEVEL and other required services keep M2 incomplete. SubwayA source
accounting remains 203 created, 4 transferred sounds, 372 editor-only, 1 unsupported
Gideon, 0 failed. No commercial content or reference sources are modified/copied.

See [contracts](../../contracts/wind_weapon_contracts.md),
[source ledger](../sources/m2_wind_weapon_sources.json) and
[execution evidence](evidence/m2-wind-weapon.json).

Project commit: `7799556cc4bc4bde4ddc2dd874b38b0e98fcbce5`.

## Validation

Debug and inspector-free Release each pass **655/655 checks**: 602 headless and
53 serial GPU, including 134 private-data cases (95 headless). Debug times are
85.43 s / 179.31 s; Release times are
15.42 s / 123.23 s. Clang ASan/UBSan/leak
checks pass **69/69** in 78.84 s, covering all six new headless
cases, original object lifetime, AI fire/runtime, Energy/Naga, explosion model/light
lifetimes, and destination trial/lifetime (including private destination initialization).
No sanitizer report or GPU validation error/VUID was observed in retained logs.

Commands from Blood2: `cmake --build build/debug -j4`,
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
The existing Clang `build/sanitize` builds wind_tests, energy_tests, naga_tests,
object_contract_tests, ai_fire_tests, ai_runtime_tests, explosion_model_tests,
explosion_light_tests and world_tests with `-j2`; CTest selects
`^(wind|energy|naga|objects|ai_fire|ai_runtime|explosion_models|explosion_lights)\.` plus
`^world.destination_(trials|lifetime|trial_private)$`, with `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`.

All 22 changed/new C++ files pass clang-format 23.1.3. All 2,630 local documentation
links resolve. All 10,923 inventoried reference-file hashes and the six reference
repositories remain unchanged. No commercial asset or private capture is committed.

The six headless cases cover both weapon modes, delayed first update, the exact
450-unit boundary, no falloff/LOS/size expansion, owner exclusion/self inclusion,
source attribution and raw damage fields, invalid collision/missing shooter/setup,
independent missing-resource failures, reentrant updates, callback removal of the
projectile/shooter/later target, and exclusion of callback-created targets. Tests
also cover source model/light formulas, no recipe RNG draw, raw miss normal,
independent expiry/audio tails, generic AI FIRE_KEY, and three retail-data
load/player/rehearsal/unload cycles. Each private cycle records 17 deliveries;
that count is not a claim that 17 actors lost health or that Gideon is playable.

GPU readback verifies an actual textured triangle fixture (11,665 changed pixels)
and the private original sphere (168 triangles, 2,330 changed pixels), at source
scale 850 and alpha 0.375 after 0.125 s. The retained local capture was visually
inspected; it shows a translucent gray shockball with the actual bmothball texture.
Expiry/unload releases the model, evaluated light, audio, scene and GPU geometry.
These are isolated diagnostic views, not a human playthrough or retail parity proof.
