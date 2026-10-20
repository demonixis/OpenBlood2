# M2: original Gideon shield and spell particles

This increment connects the original shield weapon and the two owner-bound
ObjectFX recipes required by Gideon's shield and powerup actions. The existing
particle implementation now supports the source sphere/ring emission, source
rotation and random draws, following transforms, finite lifetime and final alpha
ramp. Actual external lensflare/smoke textures render through SDL_GPU, with owned
state and failures available in the in-game inspector.

The shield weapon emits the original effect and WAV. It does not grant armor or
deliver damage: armor belongs to the separate, still-pending Gideon action. Both
primary and alternate fire retain this distinction. A failed client resource
remains inspectable without undoing the original accepted weapon shot.

This removes weapon/powerup service prerequisites for the boss; it does not
register Gideon, implement his state machine or satisfy M2. SubwayA remains 203
created, 4 transferred sounds, 372 editor-only, 1 unsupported Gideon, 0 failures.
Safe NEW_LEVEL and other required services remain open. Powerup's typed service
is exercised explicitly; its boss extra-key caller is not claimed operational.

See [contracts](../../contracts/shield_weapon_contracts.md),
[source ledger](../sources/m2_shield_weapon_sources.json) and
[execution evidence](evidence/m2-shield-weapon.json).

Project commit: `37928d12fc4720accacff4eec5f2424087ea87d7`.

## Verification

Debug and inspector-free Release each pass **664/664 checks**: 609 headless and
55 serial GPU, including 136 private-data cases (96 headless). Debug takes
106.35 s headless / 180.38 s GPU;
Release takes 13.11 s / 115.75 s.
Clang ASan/UBSan with leak detection passes **58/58** in 114.34 s:
shield, existing smoke trails/fragments, original object lifetime, AI fire/runtime,
and destination rehearsal/lifetime including private destination callbacks.
No sanitizer report or GPU validation error/VUID appears in retained logs.

Commands from Blood2: `cmake --build build/debug -j4`,
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
The existing Clang `build/sanitize` builds shield_tests, object_particle_tests,
explosion_fragment_tests, object_contract_tests, ai_fire_tests, ai_runtime_tests
and world_tests with `-j2`. CTest selects
`^(shield|object_particles|explosion_fragments|objects|ai_fire|ai_runtime)\.` plus
`^world.destination_(trials|lifetime|trial_private)$`, with `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`.

All 23 changed/new C++ files pass clang-format 23.1.3; all 2,654 local Markdown links
resolve. All 10,923 inventoried reference hashes and six reference repositories
remain unchanged. The private GPU readback has 42 textured quads and 1,367 changed
pixels; repeated renders are identical with unchanged effect clocks. The local
capture was visually inspected: small yellow/white shield motes surround a blue
powerup ring below the owner. These are isolated diagnostic effects, not a boss
fight or retail rendering comparison.

Behavioral checks exercise source color/random ordering, cached-right emission,
local-up offset, whole-system motion, per-callback rotation, first-update return,
absolute emission deadlines, no catch-up, powerup's alpha 1-to-.5 discontinuity,
exact-deadline emission and strict expiry. Capacity, invalid owner/pose/clock,
missing resources, pending removals, retained failure history and non-reused IDs
are covered. Existing fragment smoke behavior is rechecked after sharing the
extended CObjectParticleFX fields.

Weapon tests verify both modes without a collision service, no damage or armor
mutation, real WAV, explicit missing-FX failure, and rejection for player inventory.
A diagnostic original AI FIRE_KEY reaches the actual shield, without asserting
production boss behavior. Session tests advance independently of UI and render
twice without changing clocks. Private tests repeat SubwayA/player load, original
resource decode, source-preserving destination rehearsal, expiry and unload three
times. GPU tests read back the real sprite pass and check deterministic unchanged
frames, alpha-ramp presentation, owner removal and resource teardown.

Portable RNG, local particle integration/billboard sizing and rendering remain
replacement engine policies; retail parity and a human boss playthrough are not
claimed. Commercial assets and local captures remain outside both repositories.
