# M2 continuation: original weapon impact flash and smoke

Wood/stone bullet impacts now create the original flash and smoke recipes before
material audio, using actual SPR/DTX resources. CSmokeImpactFX preserves its first
update epoch, delay return, unused rotation flag, XY scale test, per-callback motion
and terminal update. Wood tint and original scale/alpha are rendered through the
existing shared-depth sprite pass. No dependency or shader changes. **M2 remains open.**

See [contracts](../../contracts/weapon_sprite_contracts.md),
[source ledger](../sources/m2_weapon_sprite_sources.json) and
[evidence](evidence/m2-weapon-sprites.json).

The original first overlapping container masks later ones even when hidden. Its
visible liquid branch suppresses flash and requests bubbles, which remain explicitly
unavailable. Missing resources and capacity errors preserve siblings and original
shot damage/audio/hearing. Detached effects survive shooter/weapon removal, update
without the inspector and clear on unload. Inspection covers active state, historical
reports and presentation errors. No new mutable game globals are introduced.

Synthetic behavioral checks cover source formulas, zero-time delay sentinel,
per-callback motion and terminal expiry, selection/RNG order, default/empty resources,
missing flash with surviving smoke, finite validation, capacities, first/hidden
containers and a real Beretta shot against a synthetic wood wall. Read-only scene
snapshots preserve clocks. Private checks issue explicitly diagnostic wood recipes
in three SubwayA sessions and verify actual resource decoding, detached lifetime and
unload. Disposable CityHub rehearsals preserve source sprite IDs, clocks, poses and resource ownership. They do not claim those diagnostic positions are real bullet contacts.
GPU readback verifies RGB/alpha blending and actual flash/smoke pixels without ImGui.

## Validation

Debug passes **576/576 headless** (101.53 s) and
**43/43 GPU checks** (156.67 s). Inspector-free Release passes
**576/576 headless** (7.42 s) and **43/43 GPU checks**
(41.23 s): **619/619 per configuration**, including 123
private-data checks (90 headless). Targeted Clang ASan/UBSan/leak coverage passes
**76/76** (230.95 s).
Neither GPU matrix emits a Vulkan validation error/VUID. Private readback captures
were visually inspected: actual yellow flash and brown translucent smoke; captures
stay ignored. This is an isolated effect render, not a gameplay/UI screenshot or
retail executable comparison.

Builds: `cmake --build build/debug -j3`, `cmake --build build/release -j2`.
Tests: `ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Existing Clang `build/sanitize` builds weapon_sprite_tests, weapon_impact_tests,
weapon_audio_tests, weapon_tests, object_contract_tests, world_tests,
character_audio_tests, explosion_sprite_tests and sprite_view_tests, then runs
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest`
at `-j2` with regex `^(weapon_sprites|weapon_impacts|weapon_audio|weapons|objects|world|character_audio|explosion_sprites|sprite_view)\.`.

`--inspect-player` exits 0 and `--start-game` retains exit 6. All 21 changed C++
files pass clang-format 23.1.3, 2,504 local Markdown links resolve, and all 10,923
reference files/six repositories remain unchanged. Evidence retains logs, selection,
source hashes, project patch/commit and capture review metadata. Commercial payloads,
reference projects and dependency revisions are unchanged. The first Release build
ended with signal status 143 without a compiler diagnostic; an incremental resume
completed the build before Release validation. Both logs are retained.

## Next priority from user playtesting

After this in-flight increment, fix missing keyboard/mouse fire and mouse look,
then investigate NPCs that do not attack and the missing train-window material.
Source inspection confirms that MovementControls currently maps movement, USE and arrow-key look, while mouse look and mouse/keyboard firing are not connected to its app input path. NPC attack and glass observations remain unresolved user reports, not confirmed diagnoses or fixes. They take priority over adding further cosmetic effects. Keyboard movement
and scripted test shots do not prove a usable interactive shooting loop.

Marks, bubbles, sparks, fragments, muzzle smoke, retail lighting/blending parity,
Gideon, safe automatic NEW_LEVEL and other required services remain incomplete.
The world source counts and observable M2 acceptance criteria are unchanged.

Continuation: [keyboard fire and relative mouse](m2_player_controls.md) now addresses the missing fire/mouse bindings. The earlier omission is historical; NPC attacks, train glass and M2 remain open.
