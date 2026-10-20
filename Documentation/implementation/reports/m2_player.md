# M2 Continuation — Original Shell and Player Entry

> Current continuation: [m2_locomotion.md](m2_locomotion.md) adds stairs, crouch
> clearance and original player body animations. The results and omissions below
> describe their earlier implementation snapshot.

Historical snapshot: the later [MoveMgr continuation](m2_movement.md) supersedes statements below that player movement is absent. Its own documented limits still leave M2 open.

**15 October 2026 — M2 remains in progress.** Selected original single-player
shell and CPlayerObj entry/lifetime branches now execute. This advances the
previous [model/motion increment](m2_services.md); it does **not** import the full
shells or the complete player/inventory/combat implementation. The game remains
unplayable and the roadmap exit gate remains open.

## What changed

- Registered CPlayerObj and CBaseCharacter source subsets, with owned original
  damage/inventory entry aggregates, original callback ancestry and scheduling.
- Connected server creation/association/initialization/start-point/exit and client
  entry/initialization-message/exit branches through owned typed messages.
- Added checked client tokens, model replacement, object dimensions/rotation,
  string comparison and the original ground-placement helper's collision path.
- Preserved original default Caleb attributes, ammo/health resets and damage-list
  membership; added rollback and child cleanup for failure or external removal.
- Added Session inspection, headless `--inspect-player` and graphical
  `--player-smoke`, independent of inspector visibility.

Full source selections, differences and omitted branches are documented in
[player_contracts.md](../../contracts/player_contracts.md),
[provenance.md](../provenance.md) and the
[source ledger](../sources/m2_player_sources.json). The
[adaptation patch](evidence/m2-player-original-adaptations.patch) compares selected
source ranges with this port; it is not a whole-file cherry-pick.

## Observed private entry

The patched `01_SubwayA` retains **580 source objects**, **372 preprocessor-only
entries**, **18 created original world instances** and **190 missing runtime
instances**. Entering the player inspection creates three more objects:
CPlayerObj, its muzzle-light object and an original SmellHint (**21 total**).
No missing world entry has been replaced by a placeholder.

Caleb's real decoded model has **86 animations**. The original start position
`(-32,192,-3224)` is grounded at **(-32,170,-3224)**. The original initial half
dimensions are `(12,41,12)`; ground placement uses animation-0 dimensions
`(12,42,12)`. The inspectable standing camera is at `(-32,206,-3224)`.
The source client profile is **5/3/3/1**; current/max health is **100/200**, armor
**0**, bullet ammo **50**. The two entry Respawn calls follow original shell then
client-initialization ordering.

Three load/entry/120-update/unload cycles pass. Each has a real original player
instance, seven path snapshots, matching client position, and **11 received
entry/first-update messages**. Objects, client payloads, strings, lists and damage/
scent membership are empty after unload. Commercial payloads remain external.

## Validation

| Configuration | Result | Evidence |
| --- | --- | --- |
| Debug, Dear ImGui enabled | **52/52 passed** | [Build](evidence/m2-player-build-debug.log), [CTest](evidence/m2-player-ctest-debug.log), [details](evidence/m2-player-tests-debug.log) |
| Release, Dear ImGui disabled | **52/52 passed** | [Build](evidence/m2-player-build-release.log), [CTest](evidence/m2-player-ctest-release.log), [details](evidence/m2-player-tests-release.log) |
| Clang ASan/UBSan with leak detection | **49/49 headless passed** | [Build](evidence/m2-player-build-sanitize.log), [CTest](evidence/m2-player-ctest-sanitize.log), [details](evidence/m2-player-tests-sanitize.log) |
| GPU inspection | Session panel captured and visually checked | [Capture](evidence/m2-player-inspector.png) |
| Reference integrity | **10,923 inventoried files unchanged**, six reference repositories unchanged | [Integrity](evidence/m2-player-source-integrity.json), [snapshot](evidence/m2-player.json) |

There are **44 public synthetic headless tests**, five optional private headless
tests and three optional GPU tests. New contracts cover stale client handles,
duplicate association/initialization, entry attributes and original dead-player
gate, atomic model failure, missing-start rollback, external removal, owned
muzzle cleanup, bounded/nonfinite messages, reentrant entry/exit/teardown and
player removal from a start-point trigger. Private tests add actual retail data,
repeated entry/update/exit and CLI status checks. GPU checks hide the inspector,
resize, unload and re-enter through the same queued commands as the UI.

Host: Fedora 44 x86_64; GCC 16.2.1, Clang 22.1.8, CMake 4.3.0, Ninja 1.13.2;
Wayland/Vulkan on NVIDIA RTX 3060 Laptop GPU. No compiler warning, sanitizer error
or leak was reported. Sanitizers cover project code in headless runs; no driver,
other-platform or retail-executable parity claim is made.

```sh
cd Blood2
cmake --preset local-debug
cmake --build --preset debug -j8
ctest --preset debug
./build/debug/blood2 --inspect-player
./build/debug/blood2 --player-smoke --capture build/player.png
cmake --preset local-release
cmake --build --preset release -j6
ctest --preset release
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DBLOOD2_INSPECTOR=OFF -DBLOOD2_SANITIZERS=ON -DBLOOD2_GPU_TESTS=OFF \
  '-DBLOOD2_TEST_DATA_DIR=/home/yann/.steam/steam/steamapps/common/Blood II/'
cmake --build build/sanitize -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure
```

## Remaining M2 work

The complete original initialization still needs ObtainWeapon/ChangeWeaponSlot,
character animation setup/hit spheres, sound/voice and autosave. Normal player
movement, damage, AI membership, containers and remaining shell/game callbacks
are not covered by this entry subset. All **190 missing SubwayA runtime objects**
and moving/rotating BSP collision remain open. Textured game rendering belongs
to the later rendering milestone. `--start-game` executes the available player
entry but returns **6** with these blockers; no Running state was introduced.

M2 changes remain uncommitted on top of M1 commit
`3b707f539be40406000049a8f7801f28c3aaf0f4`. Dependency pins and the fresh-project
architecture are unchanged. No ECS, prototype dependency or new library was added.
