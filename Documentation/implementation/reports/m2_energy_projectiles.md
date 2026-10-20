# M2: original energy weapon and point projectile

This increment connects a second weapon dependency of Gideon: original CEnergyBlast
and the non-Zealot CEnergyBlastProjectile lifecycle. Shots now create a moving SPR
object, collide with the world/solid objects, then deliver original explosion
damage on the next scheduled update. They do not apply immediate vector damage.
The projectile can outlive the shooter while retaining safe, explicit attribution.

The engine's previous typed damage contract conflated the object sending a damage
message with the responsible actor. The source explicitly permits those identities
to differ. This extension accepts the distinct sender for checked projectiles,
including null responsibility after shooter removal, while rejecting unrelated
forged attribution. Original squared-distance area falloff is unchanged.

The actual external flare SPR/DTX renders through the existing SDL_GPU sprite pass;
source scale-zero startup and moving scale-0.25 presentation are tested through GPU
readback. Original primary/alternate fire WAV selection and detached Tesla impact
playback execute. The FIRETRAIL_2 request and EXP_TESLA_PRIMARY composite remain
explicitly unavailable. The type-8 Tesla alternate effect is not substituted.
Inspection shows active projectile state and terminal outcomes after removal.

Gideon itself, shield/wind, eye-impact composite, safe NEW_LEVEL and other required
services still keep M2 open. SubwayA counts remain 203 created, 4 transferred world
sounds, 372 editor-only, 1 unsupported (Gideon), 0 failures. The diagnostic probes
exercise weapon dependencies; they are not a production boss fight or a retail
playthrough. Flight is replacement continuous point collision, not verified retail
physics. Commercial content remains external.

See [contracts](../../contracts/energy_projectile_contracts.md),
[source ledger](../sources/m2_energy_projectile_sources.json) and
[execution evidence](evidence/m2-energy-projectiles.json).

Project commit: `64b5f6110211aeaddc67f1e4824aedddc7070ba7`.

## Validation

Debug and inspector-free Release each pass **647/647 checks**: 596 headless and
51 serial GPU, including 132 private-data cases (94 headless). Debug times are
100.01 s / 177.78 s; Release times are
12.18 s / 112.28 s. Clang ASan/UBSan/leak
checks pass **60/60** in 110.87 s, covering all seven new headless
cases, original object lifetime, AI fire, vector impact/audio, Naga, radius damage
and destination trial/lifetime (including the private destination trial).
No sanitizer report or GPU validation error/VUID was observed in retained logs.

Commands from Blood2: `cmake --build build/debug -j4`,
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
The existing Clang `build/sanitize` builds energy_tests, naga_tests,
object_contract_tests, ai_fire_tests, weapon_impact_tests, weapon_audio_tests,
world_tests and explosion_tests with `-j2`; CTest selects
`^(energy|naga|objects|ai_fire|weapon_audio|weapon_impacts|explosions)\.` plus
`^world.destination_(trials|lifetime|trial_private)$`, with `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`.

All 26 changed/new C++ files pass clang-format 23.1.3. Local documentation links
resolve. All 10,923 inventoried reference-file hashes and the six reference
repositories remain unchanged. No commercial asset or private capture is committed.

The GPU test reads actual framebuffer pixels: the source zero-scale phase changes
zero pixels; the private flare changes 8,400 pixels at z=20 and 3,736 after moving
to z=30, using the source scale 0.25. The image is a faint diffuse flare on the
current unlit alpha pass, not a verified retail lighting/blend match. Snapshot
metadata includes the three local captures and their hashes.

Coverage includes delayed impact/damage, primary and alternate sound selection,
no-ammo semantics, null hand models, ignored shooter/nonsolid/same-class contacts,
primary-world and SKY collision, direct/radius damage and attribution, strict
expiry, source HP5 destruction, callback-driven projectile removal, target unlink,
missing resources/collision, catch-up rejection, zero-scale validation, original
AI FIRE_KEY hand-node aiming with clearance bypass, original reciprocal touch callbacks,
and explicit missing-node failure.

Three private SubwayA load/entry/unload cycles decode the actual flare and sounds,
retain a live projectile across isolated destination rehearsal, and release all
objects/aggregate lists/messages/strings/voices. Synthetic and private GPU checks
compare blank startup, visible scale-0.25 pixels and a smaller projected footprint
after actual flight, then verify removal/cache release. Captures remain in ignored
build directories; evidence retains hashes only.
