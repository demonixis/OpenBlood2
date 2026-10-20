# Demo font and spawned-Fanatic retakes

Date: 2026-10-19. User-reported menu-size difference and Demo_01 simulation stop,
on Blood2 baseline `9e8a02d`. Implementation commit: `d149d32`. M2 stays closed with reservation M2-GPU-001;
this does not accept M3 or modify the graphics stack.

## Findings and changes

The supplied demo fading font (`interface/mainmenus/mm_font.fnt`, corresponding
PCXs) is 41 pixels high; the supplied retail font is 31. Both editions select the
same semantic paths. Original `CMainMenus::InitFonts` and
`CMenuBase::AddLargeTextItemOption` select those assets, not a normalized size.
The shared canvas already uses their actual metrics for drawing, row advance and
pointer rectangles. Their size difference is preserved. Private asset tests now
assert both heights and demo large-row rectangle heights.

The simulation failure is a newly spawned Fanatic, not the existing missing
`SecScreen` BSP omission. The authored `spawn101` at (-1888, 2144, -2384) requests
`Fanatic`; its initial 19-unit horizontal half extent crosses primary polygon
9174 by exactly 3 units. A labelled TRIGGER delivery and subsequent upward move
reproduced that overlap and the pre-fix refusal. See the retained
[pre-fix failure](evidence/demo-retakes-before.txt). A separate unattended
125-second Debug probe was interrupted before completion and provides no
long-session validation.

Non-world NPC initialization (except Gideon) now uses the existing initial
placement correction, extracted into `ResolveInitialWorldOverlap`: at most four
face-normal corrections, total travel bounded by the smaller horizontal half
extent, and commit only after the primary-world box is clear. This spawn moves
Z by -3.001, then Y by about -6.135 to clear a second ceiling overlap. It retains
X and its original (0, 500, 0) launch velocity; it is not lowered onto the floor.
World-only MoveObjectToGround retains its prior descent guard and ground drop.
The source's skip of ground dropping for spawned actors remains intact.

This is replacement engine placement, not a verified retail depenetration solver.
It neither changes authored resources/Spawner poses nor relaxes later MoveObject
collision rules. Unresolved placement fails without a partial correction.
The existing counters expose corrections for hidden/inactive NPCs. The obsolete
client status string claiming M2 acceptance was still open now reflects the
selected-session closure and explicitly retains unvalidated saves/gameplay.

See [placement contracts](../../contracts/contact_overlap_contracts.md#spawned-npc-placement),
[menu contracts](../../contracts/demo_menu_contracts.md),
and [source hashes](../sources/demo_retakes_sources.json).

## Verification

Builds:

```sh
cmake --build --preset debug --target demo_tests demo_menu_tests menu_tests services_tests fanatic_prophet_tests spawner_tests blood2_app -j 6
cmake --build --preset release --target demo_tests demo_menu_tests menu_tests services_tests fanatic_prophet_tests spawner_tests blood2_app -j 4
cmake --build --preset debug --target object_contract_tests -j 2
cmake --build build/sanitize --target demo_tests services_tests -j 2
```

Changed source/tests were rebuilt after the initial test corrections. The installed
formatter is unavailable; style and `git diff --check` were inspected manually.
Existing compiler warnings in untouched code remain, including ItemPickups.cpp's
enum/float comparison; build success does not clear those warnings.

```sh
ctest --preset debug -R '^(demo[.]|demo_menu[.]|menus[.]|services[.]|fanatic_prophet[.]|spawner[.])' -LE gpu -j 2
ctest --preset debug -R '^demo[.]spawned_overlap$' --output-on-failure
ctest --preset debug -R '^objects[.]' --output-on-failure -j 2
ctest --preset release -R '^(demo[.]|demo_menu[.]|menus[.]|services[.]|fanatic_prophet[.]|spawner[.])' -LE gpu -j 2
ctest --preset debug -R '^gpu[.](menus|demo_menu|demo)$' --output-on-failure
ctest --preset release -R '^gpu[.](menus|demo_menu|demo)$' --output-on-failure
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize -R '^(services[.]|demo[.])' -LE gpu --output-on-failure -j 2
```

- Debug: the initial broad run passed 38/39; the new regression incorrectly
  expected unchanged height despite the second ceiling overlap. A subsequent
  attempt incorrectly expected upward travel into that ceiling. Both failures
  remain retained in [initial suite](evidence/demo-retakes-debug-initial.log),
  [spawn pose](evidence/demo-retakes-spawned-initial.log) and
  [ceiling expectation](evidence/demo-retakes-debug-ceiling-initial.log).
  After correcting the test to require actual physics integration and a clear box,
  the final targeted rerun passes 1/1. These are test expectation corrections;
  the bounded solver was unchanged after the initial implementation.
- Debug object contracts: 14/14 pass.
- Release targeted suite: 39/39 pass with the final regression.
- Linux/NVIDIA GPU: 3/3 pass in each of Debug and inspector-free Release
  (`gpu.menus`, `gpu.demo`, `gpu.demo_menu`). Both main-menu captures were visually
  inspected; the edition's larger demo glyphs remain visible as expected.
- Clang ASan/UBSan with leak detection enabled: 19/19 pass, including the real
  spawned-Fanatic regression and complete existing demo-world test. No suppression
  or sanitizer opt-out was used.

The spawn test deliberately delivers a trigger at the authored Spawner, rather
than navigating a complete playthrough or reproducing the user's input history.
It verifies both initial overlaps, the bounded correction, unchanged launch
velocity, collision-aware integration at delta .01379385 and three seconds of
subsequent session updates. Existing demo tests also cover world transfer and the
full authored arrival elevator. This is Linux validation; no new macOS/Windows
qualification or resolution of M2-GPU-001 is claimed. Missing `SecScreen` geometry
remains an explicit, independent authored-resource omission.

Final logs: [Debug targeted rerun](evidence/demo-retakes-debug-retest-final.log),
[object contracts](evidence/demo-retakes-objects-tests.log),
[Release suite](evidence/demo-retakes-release-tests.log),
[Debug GPU](evidence/demo-retakes-debug-gpu.log),
[Release GPU](evidence/demo-retakes-release-gpu.log),
[ASan/UBSan](evidence/demo-retakes-sanitizers.log).
Build logs: [Debug](evidence/demo-retakes-debug-build.log),
[Release](evidence/demo-retakes-release-build.log),
[Clang sanitizers](evidence/demo-retakes-sanitize-build.log).

Local links resolve, original reference sources remain unchanged, and commercial
resources remain external. The pre-existing workspace README edit is excluded
from this run's documentation commit.
