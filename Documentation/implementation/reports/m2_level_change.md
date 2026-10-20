# M2: safe NEW_LEVEL with retained player

Original exit messages now queue a real frame-boundary transition. A prepared
session receives the supported living player's original saved inventory/damage/
fields and engine state, enters at the authored start, and replaces the old owner.
A rejected destination leaves the current world and player alive. Session and
Objectives expose the queue, transaction result, entry/resource failures and
restored values. **M2 remains incomplete.**

See [contracts](../../contracts/level_change_contracts.md),
[source ledger](../sources/m2_level_change_sources.json) and
[machine-readable verification](evidence/m2-level-change.json).

## Original behavior and engine adaptation

BloodServerShell.cpp's NEW_LEVEL branch retains difficulty, saves players with type
1, loads the world, restores players, and enables named-start entry. Existing
selected original Save/Load functions implement the value semantics here; type-1
inventory loading removes keys. Health, armor, raw attributes/caps, supported
weapons/ammo/items survive. Original named-start fallback and trigger-before-teleport
ordering are preserved; no fresh Respawn resets the player. The client still ignores
exit requests while dead.

The port adds an in-memory transaction instead of disk Keep.sav. The old session
remains allocated while the destination runs original initialization under its own
adapter, seven intrusive list/count pairs, active camera and five shared AI tables.
Failed candidates are destroyed before restoring source globals. A successful
candidate keeps its stable address while the source is destroyed under its own
adapter; only `unique_ptr` ownership changes. No original session object is moved,
source DAT replayed, destination entry repeated, or simulation tick hidden in
preparation. Existing diagnostics remain independently usable.

The commit gate rejects unknown/failed entries, generic unavailable-service calls,
failed player association/placement, recursive start-trigger exits, and missing
initial model/texture/sprite resources. Real DTX decoding supplements resource
index checks. Current difficulty is retained. The final fallible preparation
flushes old PCM and transfers the SDL output; each world's voices keep their own
lifetime. New world revision and cleared UI/input identities cause existing GPU
presentation to rebuild before its next frame.

## Observable acceptance and remaining progression limit

Synthetic checks transfer nondefault 37 HP / 23 armor / 1,234 bullets / 43 medkit
charges, level-6 speed and raw health cap/resistance, without a fresh respawn.
Destination keys are empty while rejected attempts retain the source key. Arrival
camera activation, relative zero destination clock, post-entry updates and repeated
teardown are checked. Rejections include callback execution, missing DAT/start/
ground/image/audio, unknown classes and a start-trigger recursive exit.

Private checks repeat SubwayA -> rejected CityHub -> newly initialized SubwayA
three times. The refusal preserves exact intrusive links, player/client/owner
identity, object slots, world clock/revision and voice cursors. The promotion
preserves player values and yields a new live client/session. The GPU test also
uses typed original exit-message delivery, renders world/models/transparent faces
before and after promotion, reads back 179,200 non-background pixels without a
magenta fallback each cycle, and verifies continued PCM submission on the
transferred output.

**The authored SubwayA -> CityHub exit is still safely blocked.** CityHub currently
has 435 created source entries, 31 transferred sounds, 541 editor-only entries,
18 unsupported and one failed. Those failures are visible; no class stub or
omission bypasses the gate. Re-entering SubwayA demonstrates a real transition
using installed resources, not completed campaign progression. Initialization
acceptance does not prove every future AI action, resource or service is ready.
Autosave, complete world/client restoration, later required services and the actual
M2 acceptance criteria remain open. CPU transaction safety does not cover GPU
device loss or allocation failure after promotion.

## Verification

Implementation commit: `ee60428a0091ce450230ab0de4f6daf58eb252db`. The evidence includes the exact
project patch and SHA-256 hashes of changed files.

Debug and inspector-free Release each pass **741/741 headless tests**,
including **120 private-data cases**. The focused Clang ASan/UBSan/leak run passes
**39/39**, including 6 private cases. Each build also passes **10/10 GPU
tests** on **NVIDIA GeForce RTX 3060 Laptop GPU**. Logs identify the actual device
and contain no Vulkan validation error. The new transfer capture was visually
checked after promotion. These automated runs cover Linux, not macOS.

All authored C++ changes pass clang-format; local documentation links resolve.
The 10,923 inventoried reference files and six reference repositories are unchanged.
The initial sanitizer run found one obsolete RunWorld error-text assertion; it was
updated to require the remaining autosave blocker, and the final run passes.

Commands from Blood2:

```sh
cmake --build build/debug -j4
cmake --build build/release -j3
ctest --test-dir build/debug -L headless -j4 --output-on-failure
ctest --test-dir build/release -L headless -j4 --output-on-failure
cmake --build build/sanitize -j3 --target world_tests object_contract_tests \
  player_tests player_restore_tests player_keepalive_tests exit_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -j2 --output-on-failure \
  -R '^(world\.(level_change|level_change_private|destination_trials|destination_trial_private|destination_lifetime)|objects\.|player\.|player_restore\.|player_keepalive\.|exits\.)'
# Repeat sequentially with build/release.
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --test-dir build/debug --output-on-failure \
  -R '^gpu\.(level_change|objectives|player_restore|player_keepalive|lifecycle|viewport|world_depth|model_depth|world_alpha_private|mouse_combat)$'
```

The external installation remains outside the repositories. Source/references and
pinned dependencies are unchanged. Rehearsal tests that observe queued exit messages
now assert the queue counter; new ownership tests assert actual rejection/commit.
They do not replace a successful-transfer test with a stub or relax M2's gate.

## User playtest follow-up

On 17 October the user confirmed the new macOS build runs successfully. This is
user-reported execution; local automated validation remains Linux/NVIDIA. Apple
hardware, macOS/SDK details and the native automated suite have not been recorded.

The same playtest reports intermittently frozen dead Cultists that block the train,
including enemies after the first encounter. Earlier deterministic mouse-combat
cases do not disprove that report. It remains an active gameplay bug and the next
priority after this transition increment; no fix for it is claimed here.
