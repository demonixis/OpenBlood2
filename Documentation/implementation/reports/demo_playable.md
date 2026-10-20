# Blood II menu-free demo continuation

Date: 2026-10-19. This is the separately requested demo continuation, not an M2
reopening, resolution of M2-GPU-001 or M3 acceptance. Reference sources and external
commercial content are unchanged. Graphics dependencies, shaders and the optimized
scene pass are unchanged. The implementation is in the independent `Blood2/` repo;
this workspace holds the audit and verification record. Implementation commit:
`Blood2/9a22467` (the recorded tests were built from its working tree; the final
inspector-only display addition was also compiled successfully).

## Delivered behavior

The old fixed-offset cshell.dll menu experiment is removed, including its special
navigation, ambience mapping and three tests. Edition identification, exclusive
B2Demo.rez mounting, session propagation and reviewed original `_DEMO` game rules
remain. The missing `<cmath>` in SingularityProjectile is fixed and current Debug,
Release and sanitizer targets have been rebuilt.

```sh
cd Blood2
./build/release/blood2 --demo --data-dir /path/to/B2Demo
./build/release/blood2 --demo --data-dir /path/to/B2Demo --world Demo_02
```

Direct entry uses Caleb and the existing normal difficulty. No menu canvas,
strings, fonts or independent menu mixer are initialized. Existing Audio/Mouse
preferences still apply. Esc pauses/resumes simulation and game audio, clears held
commands and releases/recaptures the mouse. Focus loss pauses; gaining focus does
not resume automatically. Window close exits. Debug retains F10 inspection.

**Enter after death restarts the current world with initial Caleb equipment and
current difficulty**, as explicitly requested by the user. Inventory, Focus and
world progress reset. This is a fresh level, not a saved-game restore. The owned
application request is consumed outside callbacks and uses transactional
`GameSession::start_new_game`; failure retains the dead session. The
[future save integration contract](../../contracts/demo_contracts.md#temporary-death-retry-and-future-saves)
identifies this handler as the replacement point for checkpoint selection and
transactional restoration. No save file format or save UI is implemented here.

The gameplay extension ports original VoodooDollPU, weapon/view animations,
FIRE_KEY variant transport, FOV/visibility targeting, primary and alternate damage,
Focus costs, self damage, sound and six particle recipes. Original Focus
regeneration runs once per live character simulation update and stops in pause
or death. Fanatics accept only the original demo bomb-inventory `NotAvailable`
result; unrelated failures remain explicit. No fake pickup or silent object
removal was added to make the demo load.

The authored first exit transfers to Demo_02 at `05_egress`, keeping supported
player state, inventory, Focus and difficulty. The existing elevator carries the
player. Case-insensitive demo `EndGame`, after the source dead-player guard,
becomes an owned terminal request. The app decodes `screens/info.pcx`, unloads the
world and displays it directly; Esc/Enter/Space then quits. Missing/corrupt end
art fails explicitly. Retail EndGame continues through its existing path.

Two compatibility corrections were required by real resources/normal execution:

- Build 108 has no `interface/statusbar/weapons/raidcanister.pcx`. Demo HUD retains
  the secondary ammunition count without that decoration. Other required artwork
  still fails if absent; retail presentation is unchanged.
- A first real-clock frame around 1e-7 seconds rounds passive pickup motion to the
  same float position. Its unnecessary zero-length placement sweep failed on the
  authored HealthPU0 wall overlap in Demo_01 and retail 05_CDM. Passive integration
  now retains container/velocity changes but skips only unchanged non-solid
  passive positions. Explicit sweeps and solid/body checks remain strict; no
  clock is clamped upwards. See [collision contracts](../../contracts/collision_traversal_contracts.md#passive-integration-below-position-precision).

Detailed [demo contracts](../../contracts/demo_contracts.md),
[Voodoo contracts](../../contracts/voodoo_contracts.md) and the
[source ledger](../sources/demo_playable_sources.json)
record source selections, hashes, adaptations and lifetime boundaries. The prior
`demo_sources.json` and historical evidence remain unchanged.

## Data and coverage

Tests use only `/home/yann/Games/B2Demo` for demo content; retail checks use the
separate existing installation. B2Demo.rez is 55,041,396 bytes, 1,364 resources,
SHA-256 `e410ce2944210c776c7407a79e01cfa031c782132f68407cd0871379c5e7218c`.
Its Demo_01/Demo_02 DAT v56 worlds are variants of 05_CDM/05_CDMB, not identical
retail files. They were loaded and stepped independently.

The [first-world catalog](evidence/demo-catalog-01.log) reports 1,289 source rows,
615 initial original instances and 120 BSP models; the
[second-world catalog](evidence/demo-catalog-02.log) reports 200 source rows,
133 initial original instances and 19 BSP models. Each retains one explicitly
reported missing-world-model CDestructableBrush under the preexisting loader
policy (Demo_01: SecScreen, source row 674). These are not newly introduced demo
omissions or successful recreated geometry. Editor-only and transferred sound
rows remain separate from runtime instances.

| Check | Exercised behavior and result |
| --- | --- |
| Debug demo + collision regressions | 8/8: flow, rules, real-resource weapon/worlds, start-departure/contact tolerance, retail elevator entry, real GPU frontend. [Log](evidence/demo-debug-final.log). |
| Shared Debug regressions | 88/88: config, retail menus/profiles/preferences, weapons, inventory, particles, exits, HUD, pickups, Fanatics/Prophets, AI runtime, player death, demo bomb rules and elevator entry. [Log](evidence/demo-regression-final.log). |
| Clang ASan/UBSan/LSan | 7/7: demo rules/flow/weapon/worlds plus collision/entry regressions, leak detection enabled and halt-on-error. Headless only; GPU qualification issue not rerun or suppressed. [Log](evidence/demo-sanitizers-final.log). |
| Inspector-free Release | Retail artwork/menu traversal and demo frontend smoke: 3/3. [Log](evidence/demo-release-final.log). |
| Ordinary real-clock Release | Demo_01, Demo_02, retail 05_CDM and 05_CDMB each completed 120 frames, exit 0. These are normal starts, not fixed-clock smoke runs. [Demo 1](evidence/demo-normal-01.log), [Demo 2](evidence/demo-normal-02.log), [retail CDM](evidence/demo-retail-cdm.log), [retail CDMB](evidence/demo-retail-cdmb.log). |
| Menu independence | External test-only REZ removed 40 menu resources, including cshell.dll, menu images/fonts/ambience/cues; only demo Music copied alongside. Real Release frontend completed restart, transfer and end with zero menu mixer starts. [Run](evidence/demo-no-menu.log), [removal manifest](evidence/demo-no-menu-resources.txt). |
| End-art failure | External fixture with a corrupt PCX header, then absent info.pcx entry: both reached the end and returned explicit exit 5. [Corrupt](evidence/demo-corrupt-end.log), [missing](evidence/demo-missing-end.log). Original installation unchanged. |

`demo.weapon` uses a labelled synthetic floor and real demo assets: actual pickup,
all five primary variants, alternate/all-target selection, nearest selection,
obstruction, dry/invalid requests, self damage, Focus consumption, decoded sounds,
six effects and client animation FIRE_KEY combat against an original Fanatic.
`demo.worlds` delivers contact to the **authored** Voodoo pickup and commands to the
**authored** ExitTriggers; it checks failed-transition retention and supported
state preservation, then rides the full original 9,920-unit Demo_02 elevator until
Door::Open ([test output](evidence/demo-sanitizers-details.log)). It also tests direct Demo_02 entry and unload. Diagnostic relocation,
contact/exit delivery and targets are not presented as a human traversal.

`gpu.demo` uses tagged SDL input and a fixed 1/60 clock. It checks pause/audio/Focus
clocks, key repeats, focus loss/gain, real lethal damage and source death, Enter
retry, transfer, terminal ownership and terminal input. Captures of gameplay,
pause, second world and final PCX were inspected. Ordinary first-world and
second-world Release captures also showed the world and original HUD without an
inspector. Commercial captures stay in ignored build directories or `/tmp`, not
this Git repository.

Earlier failures are retained as failures, with the resolution distinguished:
[initial GPU run](evidence/demo-initial-tests.log) exposed missing raidcanister art;
[initial shared regression](evidence/demo-initial-regression.log) exposed the old
pickup catalog count (four newly supported authored Voodoo pickups, 1,785→1,789);
[initial sanitizer run](evidence/demo-initial-sanitizers.log) exposed a test advancing
the live particle clock independently of simulation (the test now advances a copy);
[first real-clock launch](evidence/demo-initial-normal-01.log) exposed the passive
pickup precision issue above. Final reruns verify the corresponding corrections.
No sanitizer failure has been reclassified as a pass.

## Reproduction

From `Blood2/`, configure the existing presets with external data variables:

```sh
cmake --preset debug -DBLOOD2_DEMO_DATA_DIR=/path/to/B2Demo \
  -DBLOOD2_TEST_DATA_DIR='/path/to/Blood II' -DBLOOD2_GPU_TESTS=ON
cmake --build --preset debug --target blood2 demo_tests services_tests --parallel 4
ctest --test-dir build/debug --output-on-failure \
  -R '^(services\.(start_departure|contact_overlap|elevator_entry)|demo\.|gpu.demo)'
cmake --preset release -DBLOOD2_DEMO_DATA_DIR=/path/to/B2Demo \
  -DBLOOD2_TEST_DATA_DIR='/path/to/Blood II' -DBLOOD2_GPU_TESTS=ON
cmake --build --preset release --target blood2 menu_tests --parallel 4
ctest --test-dir build/release --output-on-failure \
  -R '^(menus.artwork|gpu.menu_traversal|gpu.demo)$'
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ -DBLOOD2_SANITIZERS=ON -DBLOOD2_GPU_TESTS=OFF \
  -DBLOOD2_DEMO_DATA_DIR=/path/to/B2Demo -DBLOOD2_TEST_DATA_DIR='/path/to/Blood II'
cmake --build build/sanitize --target demo_tests services_tests --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize --output-on-failure \
  -R '^(services\.(start_departure|contact_overlap|elevator_entry)|demo\.)'
```

Linux, GCC 16.2.1 / Clang 22, SDL pinned
`6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1`, NVIDIA RTX 3060 Laptop GPU. No additional
platform qualification is claimed. The source ledger records private content and
original-source hashes; commercial files are not test fixtures committed to Git.

## Phase 2 and remaining limits

The [menu audit](demo_menu_audit.md) inventories source `_DEMO` behavior, the
shipped Build 108 vs source Build 109 mismatch, actual resource names and strings,
promotions, options, pause and end. It compares three architectures and recommends
limited original-menu adaptations with a finite explicit edition resource provider.
The old fixed-offset reader is historical only. No new demo menu is included.
Mouse labels and all six Options entries exist in the shipped binary; disabling
them all would not reproduce the evidence. Bloodbath/Chosen/Prime Gibs are
promotional actions, not merely disabled retail items.

A complete human walkthrough of both worlds has **not** been performed. Combat,
pickups, death/retry, transition, elevator and end were tested through the specific
scripted paths above; successful entry is not proof of every encounter or trigger.
The original Windows demo client has not been interactively traversed in this run;
menu layout/timing and device-dependent behavior still need original observation
before a fidelity claim. General save/restore, multiplayer and full-campaign
acceptance remain outside this work. M2-GPU-001 remains open.
