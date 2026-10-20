# SDL3 launcher implementation and validation

The [compact launcher continuation](launcher_redesign.md) supersedes the interface
and adds qualified demo delivery and REZ data mods. Graphical settings were
cancelled by the user. The L0-L2 evidence below remains a historical record.


Date: 2026-10-19. Authorized scope: L0 through L2 of the
[launcher plan](../../history/Roadmap/06_launcher.md). Nightmares remains separate. This work
does not reopen M2, resolve M2-GPU-001, accept M3 or change the game's graphics
stack. [Contracts](../../contracts/launcher_contracts.md) define the
shared boundary and versioned schemas.

## L0 baseline

Blood2 baseline is `8fcbf35`, with code unchanged from `d149d325`. Existing
dependency revisions, hashes, shaders and SDL options are retained.

Command from Blood2:

```sh
ctest --test-dir build/dev --output-on-failure -R '^(config\.|menus\.preferences|demo\.(flow|rules)|demo_menu\.)'
```

Result: 12/12 passed. [Raw baseline](evidence/launcher-l0-baseline.log).
This headless baseline covers no new launcher UI and makes no native-platform
qualification claim. L1/L2 implementation and verification follow in later
sections as completed.

## L1 frontend

Dedicated `launcher-debug` and `launcher-release` presets build SDL with native
dialogs. Game presets keep SDL_DIALOG off and all revisions/hashes, shader files,
SDL patch and render code unchanged. The shared target compiles the existing
config and REZ source files at their original paths; it does not alter their
bytes. The UI provides manual/native paths, explicit edition, remembered
installations, keyboard navigation and diagnostic scale/capture options. Optional
commercial artwork is deferred; the plain title panel is usable without it.

| Check | Result |
| --- | --- |
| Debug headless state/dialog/help | 3/3 [passed](evidence/launcher-l1-debug-headless.log) |
| Release headless state/dialog/help | 3/3 [passed](evidence/launcher-l1-release-headless.log) |
| Debug Vulkan UI at 1, 1.5 and 2 scaling | 3/3 [passed](evidence/launcher-l1-debug-gpu.log), NVIDIA GeForce RTX 3060 Laptop GPU |
| Release Vulkan UI at the same scaling | 3/3 [passed](evidence/launcher-l1-release-gpu.log), same actual GPU |
| Game config/REZ/menu-preference/demo regressions after shared-target extraction | 15/15 [passed](evidence/launcher-l1-game-regressions-final.log) |

The UI captures were visually inspected. Scaling is forced diagnostic scaling,
not a native multi-monitor HiDPI qualification. Dialog callbacks are tested for
success, cancel, failure, worker-thread completion and late completion after
owner destruction. Native portal interaction and other desktop platforms still
need native verification.

An initial [premature CTest invocation](evidence/launcher-l1-game-regressions.log)
overlapped game linking and had three BAD_COMMAND/permission-denied failures;
these remain recorded. The final 15-case run follows successful builds of the
required game/test targets.

The attempted [full Debug build](evidence/launcher-l1-game-build.log) also exposes the existing engine-only
`gpu_registry_tests` link failure for `bc_EngineMessageFn`/`bc_ObjectMessageFn`.
Linking its unchanged object against the pre-extraction development engine
archive reproduces the same [failure](evidence/launcher-baseline-gpu-registry-link.log).
The [first manual baseline link](evidence/launcher-baseline-gpu-registry-link-initial.log)
used the wrong spdlog archive name; the corrected link above reproduces the
actual unresolved symbols. The graphics target is not modified here.
Required launcher and [selected game targets](evidence/launcher-l1-game-targets.log)
targets build; this does not claim a successful full Debug all-target build or
resolution of the unrelated GPU qualification reservation.

## L2 installation validation and game launch

The launcher now queries the real game's standalone `--launcher-info`, validates
the selected edition with the shared bounded readers on an owned SDL thread,
and revalidates before Play. It checks executable size/mtime before and after
archive preparation, reserves a unique user-owned run directory, writes the
bootstrap INI and starts the normal game with an argument vector. No gameplay
or diagnostic flags are injected. The playable child's output is inherited;
only the bounded capability query uses pipes and a deadline.

Installation records persist separately from game preferences. The UI returns
after child exit, reports nonzero exits and exposes the run's log folder.
Changing data/executable/edition invalidates prior checks. Normal closure leaves
the playable child alive; a nested subprocess test verifies that the parent can
release tracking, call SDL_Quit and exit before its child finishes.

| Final check | Result |
| --- | --- |
| Launcher Debug headless, including actual game query and two private installations | 12/12 [passed](evidence/launcher-l2-debug-headless-final.log) |
| Launcher Release headless, same coverage | 12/12 [passed](evidence/launcher-l2-release-headless-final.log) |
| Launcher Debug GPU scaling and Play/hide/return | Four GPU tests plus one synthetic setup: 5/5 [passed](evidence/launcher-l2-debug-gpu-final.log) |
| Launcher Release GPU scaling and Play/hide/return | Same 5/5 [passed](evidence/launcher-l2-release-gpu-final.log) |
| Clang ASan/UBSan/LSan, headless only | 8/8 [passed](evidence/launcher-l2-sanitizers-final.log), leak detection and halt-on-error enabled |
| Game Debug config/REZ/preferences/demo and standalone query | 16/16 [passed](evidence/launcher-l2-game-debug-headless.log) |
| Game Release, same regression selection | 16/16 [passed](evidence/launcher-l2-game-release-headless.log) |
| Real game Debug retail menu/artwork/traversal and demo menu | 3/3 [passed](evidence/launcher-l2-game-debug-gpu.log) |
| Real game inspector-free Release, same menu coverage | 3/3 [passed](evidence/launcher-l2-game-release-gpu.log) |

All GPU logs report Vulkan on NVIDIA GeForce RTX 3060 Laptop GPU. Private
launcher checks index 7,446 retail resources across four mounts and 1,364 demo
resources in one mount. They run the real game's capability query without a
display and prove ready-to-launch state, not full asset/gameplay validation.
The Play-widget test uses a synthetic subprocess recorder; real menu rendering
is verified by the separate existing game tests above. This is not a human
walkthrough of either the original Windows launcher or the complete campaign.

Meaningful failure cases include malformed/future launcher state, UTF-8 and
Unicode/space paths, query overflow/nonzero exit/timeout/cancellation, malformed
or empty REZs, edition separation and excluded expansion data, changed archives
and executables, unwritable run storage, repeated Play, child failure and parent
detachment. Asynchronous native-dialog success/cancel/error and late callbacks
are synthetic checks; interactive portal and native Windows/macOS qualification
remain pending. Optional original artwork, window-geometry persistence and L3
through L7 are not implemented in this increment.

An initial [Play-widget GPU test](evidence/launcher-l2-debug-gpu-initial.log)
timed out after the synthetic game returned. The [backtrace](evidence/launcher-l2-ui-hide-backtrace.log)
shows a blocking acquisition on the reused native Vulkan swapchain. The launcher
now retires/releases presentation before hiding and reclaims the window on
return. The isolated [retest](evidence/launcher-l2-debug-process-retest.log) and
both final GPU suites pass. The original failed result is retained. No SDL
patch, engine rendering change or M2 platform-leak investigation is included.

The [stack check](evidence/launcher-l2-stack-check.log) confirms byte-identical
config/REZ source files and preserved game SDL options. Only the launcher builds
enable native dialogs. Builds and final test runs used the `bbc7ef4-dirty`
working tree containing this increment; final committed sources reproduce it
with updated build revision strings. Sources, dependencies and commercial data
are not fetched or modified by ordinary launcher startup.

After commit `5c097c7`, the user-facing Release game and launcher were rebuilt
with clean revision metadata. The real capability query and both private
installation checks pass again, 3/3, against that exact revision
([post-commit log](evidence/launcher-l2-postcommit-release.log)).

## Reproduction and remaining scope

Follow [launcher setup](../../launcher.md) for build/run commands.
Headless tests use the launcher presets and `-L headless`; GPU checks use `-L gpu`
after opt-in configuration. Game regressions select `config.*`,
`cli.launcher_info`, `menus.preferences`, `world.rez_structure`,
`world.rez_rejection`, `world.resource_mount`, `demo.flow`, `demo.rules`,
`demo_menu.manifest` and `demo_menu.flow`. Game GPU checks select `gpu.menus`,
`gpu.menu_traversal` and `gpu.demo_menu`. Raw logs contain exact test commands.

L0 through L2 are implemented and checked for the documented Linux scope.
Graphics editing, data mods and demo installation/download remain L3 through
L7. Nightmares stays separate. Native platform qualification, the baseline full
Debug registry-test link failure and M2-GPU-001 remain explicit limits.
