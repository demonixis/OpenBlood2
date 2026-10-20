# M2: original Naga eye weapon, a Gideon dependency

Gideon remains the single unsupported authored SubwayA entry. Reviewing its source
shows four weapon dependencies rather than a reusable Cultist loadout. This increment
implements one of them: original `CNagaEyes`, with actual damage, original AI
head-node/FIRE_KEY delivery, primary WAV playback and the original red beam model.
Gideon is not registered and M2 remains incomplete.

The complete Naga Eye Beam definition is retained. The original visual ray ignores
the shooter's class family, while the separate ordinary damage ray can hit those
same actors. Tests distinguish those results instead of asserting that the visible
endpoint must always be the damaged target. Primary fire uses the external laser
WAV; alternate fire remains silent. AMMO_NONE stays unlimited, and null held-model
paths intentionally create no attachment.

The model beam uses the existing shared-depth translucent SDL_GPU pass. Its lifetime
starts on its first update, lasts half a second and continues after the shooter or
weapon disappears. The inspector exposes current beam clocks/scale/alpha, retained
publication failures, the weapon's separate visual endpoint and missing impact FX.
Rendering does not advance gameplay or effect clocks.

The additional source `EXP_NAGA_EYE_BEAM` composite is **not implemented**. Its source
position/normal quirk is documented and the missing service is explicit; no generic
explosion or successful placeholder substitutes for it. Gideon's shield, wind,
projectiles and dedicated state/lifecycle integration remain subsequent work.

See [contracts](../../contracts/naga_weapon_contracts.md),
[source ledger](../sources/m2_naga_weapon_sources.json) and
[execution evidence](evidence/m2-naga-weapon.json).

Project commit: `49aec13de30da9e22e158780143b3c5f619eb531`.

## Validation

Debug and inspector-free Release each pass **638/638 checks**: 589 headless and
49 serial GPU, including 130 private-data cases (93 headless). Debug times are
82.01 s / 177.01 s; Release times are
14.62 s / 112.94 s. Clang ASan/UBSan/leak
checks pass **44/44** in 67.52 s across original object lifetime,
AI firing, weapon audio/impacts, destination callback guards and all six new Naga
headless cases. No sanitizer report or GPU validation error/VUID was observed in
the retained transcripts.

Commands from Blood2: `cmake --build build/debug -j4` and
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
The existing Clang `build/sanitize` builds naga_tests, object_contract_tests,
ai_fire_tests, weapon_impact_tests, weapon_audio_tests and world_tests with `-j2`;
CTest selects `^(naga|objects|ai_fire|weapon_audio|weapon_impacts)\.` plus the
world destination-trials/lifetime guards, using `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.

All 27 changed/new C++ files pass clang-format 23.1.3 and local Markdown links
resolve. The 10,923 inventoried reference files and six reference repositories
are unchanged. Missing compile includes and an incorrect private world request
were fixed during validation. The GPU test now distinguishes whole-model coverage
from the exact single-face blend equation; it does not assert an incorrect linear
relationship for a multi-layer transparent tube. Logs, selected detailed transcripts,
source hashes and the committed project patch are retained; private captures stay
in ignored build directories.

The private tests deliberately emit diagnostic shots/models through actual resources;
they are not an authored Gideon fight. Three load/unload cycles check actual beam
geometry, DTX skin and WAV decoding, effect lifetime, monotonic IDs and source-beam
preservation during isolated destination rehearsal. Synthetic tests cover same-class
and derived-class filtering, different solid/RAYHIT selection, real damage, missing
resources, no-ammo firing, clock boundaries, capacity, callback removal and original
AI model keys. Private and synthetic GPU readback checks use actual model geometry
and verify the 0.5-to-0.25 alpha change against pixel values on one actual model
face. The full model is rendered separately: overlapping translucent faces require
compositing and cannot use a single-layer linear-alpha expectation.

SubwayA remains **203 created / 4 transferred sounds / 372 editor-only / 1 unsupported
Gideon / 0 failed**. No class has been replaced by a success-shaped placeholder.
The actual M2 exit criteria, safe NEW_LEVEL, remaining required services and retail
behavior comparisons remain open. Linux/Vulkan is the tested platform; no additional
platform or full game-completion claim is made.
