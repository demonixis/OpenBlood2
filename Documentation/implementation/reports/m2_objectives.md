# M2 — Original mission objectives (in progress)

15 October 2026. **M2 is still open.** This increment follows
[original pickups and destruction spawns](m2_pickups.md) and connects the original
ObjectivesTrigger and client SMSG_OBJECTIVES branch. It also reads localized
strings directly from the installed game's `cres.dll` resource in REZ archives.
No reference repository, dependency pin or commercial asset was modified.

## Delivered behavior

- Original Trigger ancestry/defaults/filtering, nonempty authored title/text guard,
  resource number precedence, selected single/custom client branch and replacement
  of the current objective. Source sound requests remain explicit pending audio.
- An owned typed broadcast to the existing local client queue. Sender removal,
  stale player associations, queue exhaustion, unload and re-entry have checked
  behavior; the inspector never owns game state.
- A bounded, platform-independent PE32/PE32+ string-resource reader. It performs
  UTF-16 to UTF-8 conversion without executing DLL code or requiring Windows.
  Existing REZ override order chooses the actual external resource. Missing IDs,
  invalid images and ambiguous languages remain failures, not authored-text fallbacks.
- An Objectives inspector tab displaying all live objective triggers, including
  hidden/inactive ones, source properties and queue/sound counts, alongside actual
  client text, resource source/language and resolution diagnostics. Original `|`
  separators are shown as line breaks. Original HUD and audio remain unimplemented.

Details and bounds: [contracts](../../contracts/objectives_contracts.md).
Source selections and file hashes: [provenance ledger](../sources/m2_objective_sources.json).
The PE reader is new engine code based on Microsoft's format documentation,
not imported prototype code or a replacement gameplay implementation.

## Installed-content evidence

All **53 authored ObjectivesTrigger entries** across the **57 readable worlds**
create successfully, and every requested localized title/text pair exists.
`worlds/16_bridge.dat` retains its earlier explicit reader rejection.
Mounted `cres.dll` resolves from **BLOOD2P2.REZ**, with **790 nonempty strings**
and language **0x0409 (1033)**. No extracted table is committed.

SubwayA now has **142 created source entries + 7 owned SelfTriggers**, 372
editor-only entries, **66 unported** and zero failed. There are 149 runtime
objects before player entry and 152 afterward. The actual authored objective is
received and resolved by the client. Exact accounting assertions were updated
for this one additional source class; the M2 startup gate is unchanged.

## Validation

GCC Debug passed **176/176** in **67.61s**; GCC Release without ImGui passed **176/176** in **43.63s**. Clang ASan/UBSan/leak checks passed **163/163 headless** in **151.48s**, without findings.
Final logs and hashes are recorded in the [evidence ledger](evidence/m2-objectives.json).
All 25 changed C++ files pass formatting; final incremental builds have no warnings.
All 1,200 local Markdown targets resolve. The 10,923 inventoried reference files are unchanged and all six reference repositories are clean.
Blood2 commit: `3ab3ad0c6600a698b9e82acf034183e47eda674e`.
The configured suite has **176 tests**: 145 synthetic headless, 18 private headless
and 13 serial GPU cases. The sanitizer configuration runs the 163 headless cases.

Six new synthetic cases cover PE32/PE32+, Unicode and language selection;
all-prefix truncation, malformed offsets/directories/UTF-16 and duplicate IDs;
original defaults/filtering/once-only activation; localized precedence and owned
sender removal; missing resources/invalid payloads; queue capacity, source queue
failure, game-mode filtering and pending-player cleanup. The private catalog
creates every authored objective and delivers SubwayA's objective through the
actual player. The GPU test resolves it with the UI hidden, then unloads/reloads.
The capture has been visually checked for readable properties and client text.

Reproduce in Blood2:

```sh
cmake --preset local-debug
cmake --build --preset debug -j4
ctest --preset debug --output-on-failure -j4
cmake --preset local-release
cmake --build --preset release -j3
ctest --preset release --output-on-failure -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -L headless --output-on-failure -j2
./build/debug/blood2 --objectives-smoke --capture /tmp/objectives.png
```

The existing sanitizer CMake configuration uses Clang, BLOOD2_SANITIZERS=ON,
BLOOD2_INSPECTOR=OFF, BLOOD2_GPU_TESTS=OFF and the external private content root.
Local presets/content paths remain ignored. The exact logs, patch, image and
source-integrity checks are attached to the evidence ledger.

## Remaining M2 gate

This adds objective delivery and resource lookup, not a complete local game.
SubwayA still needs CameraObj, CinematicTrigger, KeyFramer, ExitTrigger, actor/AI,
light/sprite/sound and client effect classes. Required services, world transitions
and observable acceptance in the roadmap remain unfulfilled. Original HUD,
audio, save/restore, other platforms and XR were not validated. M2 remains open until the roadmap's observable acceptance criteria pass.
