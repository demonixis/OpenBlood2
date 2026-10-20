# Compact launcher, demo delivery and REZ mods

The later [default-build continuation](launcher_default_build.md) builds both
executables together, uses only sibling executable discovery by default and
updates the window names. The implementation and test records below retain
their original scope.

Implemented in Blood2 commit `86404e8` on 2026-10-19, from baseline `5c097c7`, following the user's revised
scope. This supersedes the original L0-L2 launcher interface. Graphical settings
are cancelled; Nightmares remains a separate project. M2 closure, M2-GPU-001 and
M3 acceptance are unchanged.

The new main window is 580x356 logical pixels: original illustration or plain
fallback title, Play, Game Settings, Mods, Quit, and one status line. Play is
unavailable without validated data. The separate panels grow the window to fit
ordinary controls. A proportional font replaces the inspector-style default.
Technical executable selection is under Game Settings > Advanced. Existing game
SDL flags, pins, shaders and scene renderers have no diff.

[Usage](../../launcher.md),
[contracts](../../contracts/launcher_contracts.md),
[source/dependency ledger](../sources/launcher_redesign_sources.json).

## Implemented behavior

Game Settings chooses the original game folder, infers a demo-only root, retains
an explicit edition choice and saves on OK. Separate preset builds discover the
matching neighboring game executable, while packaged builds use a sibling game.
Play retains capability checks, archive revalidation, explicit bootstrap/log
paths, child tracking and swapchain retirement before hiding.

The demo installer downloads the user-specified [Archive.org package](https://archive.org/details/B2demo),
37,695,395 bytes, SHA-256
`2f22bed184ed2a9c39b448041603ee7199310f83cfa791e1a26cd4bdc67b9bc1`.
Its 98 `Game/` files total 60,329,668 bytes; they include music, notices and
`B2Demo.rez` with the already tested v1.0 Build 108 SHA-256
`e410ce2944210c776c7407a79e01cfa031c782132f68407cd0871379c5e7218c`.
The user's ModDB link lists a different `b2demoa.zip`; that package is not silently
accepted as this one. Download and local ZIP import share bounded extraction,
SHA/CRC checks and transactional publication under SDL user storage. The original
ZIP is retained; no downloaded installer/DLL is executed. Successful installation
is selected and persisted, then returns to Main. Cancellation/errors preserve the
previous saved selection. Storage-save failure reports the installed path and
restores the prior active selection. Crash-left staging remains unreferenced.

Mods registers local REZ files disabled by default. Persistent per-installation
and per-edition toggles produce ordered absolute `--rez` arguments. The game
advertises `profiles=rez1` and mounts data overlays after stock archives using the
existing transactional last-exact-path-wins reader. Disabled entries add no
arguments; duplicate/base files, empty/malformed archives, executable resources
and known Nightmares archives are refused. This is data overlay support, not
certification of arbitrary legacy mod behavior or expansion game code. A bad
mods configuration blocks Play instead of silently launching without its mods.

Original `GAME1` (retail) and `BLOOD2` (demo) 320x240 DIB resources are decoded from
external executable files without loading them as code. Missing/unsupported
artwork falls back. No commercial pixels or binaries were added to Git.

## Verification

All runs used Linux, GCC for Debug/Release and Clang for sanitizers. GPU runs
reported Vulkan on NVIDIA GeForce RTX 3060 Laptop GPU. The local NVIDIA ICD
override is only in test commands. Final test binaries were built from the
`5c097c7-dirty` working changes; implementation commit is `86404e8`.

| Check | Result and raw evidence |
| --- | --- |
| Launcher Debug headless, including real game query/retail/demo indexes | [15/15](evidence/launcher-compact-debug-headless-final.log) |
| Launcher Release headless | [15/15](evidence/launcher-compact-release-headless-final.log) |
| Launcher Debug GPU: Play/hide/return, 1x/1.5x/2x, both panels, real navigation clicks | [7 GPU cases + fixture setup, 8/8](evidence/launcher-compact-debug-gpu-final.log) |
| Launcher Release GPU | [7 GPU cases + fixture setup, 8/8](evidence/launcher-compact-release-gpu-final.log) |
| Clang ASan/UBSan/LSan, `detect_leaks=1`, halt-on-error, headless; miniz instrumented | [11/11](evidence/launcher-compact-sanitize-headless-final.log) |
| Real qualified ZIP import, cancellation and external bitmap under sanitizers | [Passed](evidence/launcher-compact-sanitized-import.log) |
| Live HTTPS download, complete install and demo artwork | [Passed](evidence/launcher-compact-download-final.log) |
| Local ZIP import and artwork | [Passed](evidence/launcher-compact-import.log) |
| Real Install from ZIP widget, injected native chooser reply, selected/persisted path, main-menu return and enabled Play | [Passed](evidence/launcher-compact-ui-install-final.log) |
| Actual game Release config/REZ/capability regression | [11/11](evidence/launcher-compact-game-headless.log) |
| Actual game Release menu/render regressions, retail/demo | [3/3](evidence/launcher-compact-game-gpu.log) |
| Newly downloaded installation: actual demo menu/start/death retry/Demo_02 transfer/EndGame/new game/quit promotion | [Passed, 251 frames, zero menu audio failures](evidence/launcher-compact-installed-game-menu.log) |
| Default neighboring executable discovery and retail illustration | [Rendered and inspected](evidence/launcher-compact-preview-final.log) |

The additional synthetic delivery tests cover exact size/hash/CRC failures,
traversal/absolute/backslash paths, case duplicates, file/directory collisions,
Windows device names, overlong names, entry bounds, cancellation, existing
installation preservation and allowed redirect-host parsing. Mod tests check
persistent toggles, edition isolation, override order, rollback, code/expansion
refusal, actual subprocess arguments and revalidation after modification.
Artwork malformed/truncated headers and both real edition bitmaps are exercised.
Actual chooser interaction, HTTPS error/redirect fault injection and disk-full
fault injection were not independently qualified. Native Windows/macOS execution,
packaging and cross-platform CA provisioning remain unverified. No real community
mod was certified, and no full campaign/M3 claim is made.

Commands run from `Blood2/`:

```sh
cmake --preset launcher-debug
cmake --preset launcher-release
cmake --preset launcher-sanitize
cmake --build --preset launcher-debug --parallel 6
cmake --build --preset launcher-release --parallel 6
cmake --build --preset launcher-sanitize --parallel 6
ctest --preset launcher-debug -L headless --output-on-failure
ctest --preset launcher-release -L headless --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset launcher-sanitize -L headless --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset launcher-debug -L gpu --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset launcher-release -L gpu --output-on-failure
cmake --build --preset release --target blood2_app config_tests world_tests --parallel 4
ctest --preset release -R '^(config\.|world\.(rez|resource)|cli.launcher_info)' --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --preset release -R '^gpu\.(menus|menu_traversal|demo_menu)$' --output-on-failure
```

The diagnostic `launcher_delivery_tests install/download` and launcher
`--install-smoke` used isolated `/tmp/blood2-launcher-redesign/` roots; no user's
retail/demo installation was modified. New code passes clang-format 21.1.8 and
`git diff --check`. All 893 maintained local links resolve; JSON ledgers and Markdown fences validate.
The previously documented all-target Debug `gpu_registry_tests` link problem
and M2-GPU-001 were neither investigated nor declared fixed.

Post-commit Release game/launcher rebuilds completed. The [capability and private index checks](evidence/launcher-compact-postcommit.log) pass 3/3 and report clean revision `86404e8`, with `profiles=rez1`.

## Retained intermediate failures

The [initial synthetic ZIP test](evidence/launcher-compact-headless2.log) failed
in miniz's writer before it could produce an unsafe-name fixture; the test now
injects those names after writing so the application reader is actually tested.
The [first live download test](evidence/launcher-compact-download.log) installed
successfully but failed its artwork assertion: demo names the bitmap `BLOOD2`,
not retail's `GAME1`; the bounded decoder now supports both explicit names.
The [first navigation test](evidence/launcher-compact-ui-gpu.log) clicked the next
panel before its first layout after window resize; the harness now leaves a
layout frame between clicks. Its [isolated retest](evidence/launcher-compact-navigation2.log)
and final Debug/Release suites pass. These initial failures remain failures in
the retained logs.
