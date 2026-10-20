# Original menus — UI-M0 through UI-M6

Authorized scope: source menus, F10 inspector, true menu pause, four player profiles,
Audio/Mouse preferences, unavailable save/load screens and final navigation.
Full saves and Action rules are explicitly deferred. This is separate from global
M2 closure/M3 acceptance. Base implementation: bb98fd7 (graphics phases 0–1).

Implementation is complete for the authorized UI scope. Selected regression suites
pass in Debug, inspector-free Release, and headless ASan/UBSan; scope and remaining
platform/feature limits are recorded below.

## Baseline and UI-M0

Post-pull Debug baseline: 24 targeted headless tests and four NVIDIA GPU tests pass,
including lifecycle, HUD and both world benchmarks. The first build invocation used
an incorrect target name (player_control_tests); corrected to player_controls_tests.

The foundation imports original menu construction, identifiers and navigation into
an owned controller, composes external PCX/FNT/cres.dll content, and presents it via
the existing UI pass. All ten page captures were produced on NVIDIA; the main page
was visually inspected. Source typography's unavailable space implementation is an
explicit adaptation. The mm_font final-glyph one-pixel overhang is bounded and tested.

Initial tests failed because the selective import script stopped at the base Build
call and the original FNT's last advance exceeded its bitmap by one pixel. The import
was corrected and a final-glyph-only opt-in added; HUD defaults remain strict.

Current UI-M0 validation: five selected headless cases and gpu.menus pass. Captures
remain in ignored build/debug/tests/menu-gpu. This is controller/render validation,
not application integration or completion of the later UI stages. Native macOS was
not run. No sanitizer reservation is waived.

Evidence: [baseline headless](evidence/ui-baseline-headless.log),
[baseline GPU](evidence/ui-baseline-gpu.log), [UI-M0 headless](evidence/ui-m0-headless.log),
[UI-M0 GPU](evidence/ui-m0-gpu.log).

## UI-M1/UI-M2 — Pause, audio and normal startup

Normal launch displays original menus; `--view-world` remains direct and existing
smoke/benchmark dispatch remains separate. F10 toggles inspection and Esc manages
the pause hierarchy. The simulation/transition/scene-update branch is skipped while
menus own input; the game SDL output pauses, while a separate menu mixer continues.
The new-game boundary prepares and validates the destination before replacing its
source. At this stage all requests use Caleb and medium difficulty.

Validation: Debug application and menu test builds pass; all three `menus.*` tests
pass (including real SubwayA entry, failed replacement retention, subsequent valid
replacement, and three seconds of independent audio advancement). See
[evidence](evidence/ui-menu-session.log). A five-frame normal-launch NVIDIA capture
was inspected with the inspector absent and ambience starts=1/failures=0. Dummy
audio validates SDL submission, not listening on speakers. Complete input traversal,
Release and sanitizers remain for the final stage.

## UI-M3 — Four original profiles and difficulty

Story now fixes Caleb; Chosen applies the selected original attributes, default
model/skin, camera eye, sound root, gender footsteps/landing and view-weapon hand
skins/melee models. Existing atomic animation resizing is preserved. Saved player
validation accepts all four default profiles and existing level transfer retains
both profile and difficulty. This adds no Action-mode rules or save-file support.

Debug: the 21 other selected player/state/restore/menu checks passed. The first
profile test incorrectly attempted crouching during the original camera intro;
its corrected version waits for both source cameras to expire. All four profiles
then passed real-resource entry, footsteps/landing, standing/crouching, knife hand
resources, capture/restore, CityHub transfer with difficulty, and original death.
See [final profile result](evidence/ui-menu-profiles.log). This is automated behavior
coverage; four-profile visual and audible human acceptance is not claimed.

## UI-M4 — Persistent Audio and Mouse options

Sound/music enable flags and volumes are independent; muted voices/instruments keep
their clocks. Default audio uses the previous mixing path. Sliders retain source
ranges/steps, and default sensitivity exactly preserves the existing SDL mapping.
Atomic preference replacement uses SDL's user directory; tests use isolated paths.
Malformed reads and write failures are explicit, with no partial settings commit.

## UI-M5 — Unavailable save/load screens

Eight source slots, plus Restart/Quickload for Load, are visible and disabled. The
original `%1!s!` insertion is resolved to an unavailable label; the visible screen
states that saving/loading is unavailable. No save artifact or partial-player save
claim is added. The final captures were reviewed, including Audio, Mouse, Load and
the original quit question.

## UI-M6 — End-to-end traversal and regressions

Quit is connected with default-No confirmation, mouse/keyboard selection and Y/N.
`--menu-smoke` drives the actual SDL frontend from main menu through new game,
pause, inspector over pause, options, resume, inspector alone and confirmed quit.
It freezes source camera/NPC transforms, updates, animation/weapon tracks, game
voice cursors and the instrument clock for over three simulated seconds while
menu audio continues. Resume skips the paused interval; later inspection advances
the world. Tagged test input excludes unrelated compositor/physical device events;
normal focus cancellation remains in the separate input tests.

Debug: **85 selected headless/private tests passed**, including existing player,
restore, locomotion, character/landing/death audio, HUD, world startup/transfers,
input, config, DLS scalar equivalence and all five menu suites. Existing lifecycle,
HUD and both world GPU benchmark checks passed; ten-page menu captures passed.
The final SDL traversal passed on NVIDIA after correcting bounded feedback voice
saturation and isolating scripted input from compositor focus events. These earlier
failed development runs are retained in local build logs, not reclassified as passes.

All captures/logs are local build evidence. No commercial resource files are copied
into either repository. Default scene batching, shaders and dependency pins remain
unchanged. Runtime native macOS, genuine high-DPI hardware and human audible/retail
comparison have not been performed by this run. M2-GPU-001 is neither investigated
nor cleared by these UI tests.

Release without the inspector: **45 selected tests passed**, plus menu captures and
the actual SDL traversal. ASan/UBSan with leak detection enabled: **36 selected
headless/private tests passed**. No sanitizer GPU qualification is claimed. Final
input-latch and nonmodal audio-notice changes receive focused rechecks after those
full selected runs; results are included in the final evidence below.

Two separate normal Release application launches accepted the same isolated SDL
preference file unchanged. A malformed file produced an explicit warning and
visible error modal. Audio failures use a nonmodal notice and never stop menu
navigation. The root user README change is preserved and excluded from our commits.

### Reproduction commands

The test data directory was supplied through the existing private CMake cache;
no machine-specific content path was added to production source.

```sh
cmake --build --preset debug --target blood2_app menu_tests config_tests player_controls_tests dls_sampler_tests world_tests hud_tests player_tests player_state_tests player_restore_tests character_audio_tests movement_audio_tests player_death_tests locomotion_tests -j 3
SDL_AUDIODRIVER=dummy ctest --test-dir build/debug -R '^(menus\.|config\.|player_controls\.|hud\.|dls_sampler\.|world\.(session_start|level_change)|player\.|player_state\.|player_restore\.|character_audio\.|movement_audio\.|player_death\.|locomotion\.)' --output-on-failure
cmake --build --preset release --target blood2_app menu_tests config_tests player_controls_tests dls_sampler_tests world_tests hud_tests player_restore_tests -j 3
SDL_AUDIODRIVER=dummy ctest --test-dir build/release -R '^(menus\.|config\.|player_controls\.|hud\.|dls_sampler\.|world\.(session_start|level_change)|player_restore\.)' --output-on-failure
cmake --build build/sanitize --target menu_tests player_restore_tests player_state_tests player_controls_tests dls_sampler_tests -j 3
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 SDL_AUDIODRIVER=dummy ctest --test-dir build/sanitize -R '^(menus\.|player_state\.|player_restore\.|player_controls\.|dls_sampler\.)' --output-on-failure
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json SDL_AUDIODRIVER=dummy ctest --test-dir build/debug -R '^gpu\.(menus|menu_traversal|lifecycle|hud|bench_01_SubwayA|bench_02_CityHub)$' --output-on-failure
```

GPU traversal/captures also run under `build/release`. The sanitizer build was
already configured with `BLOOD2_SANITIZERS=ON` and GPU tests disabled. The existing
GPU benchmark assertions pass; simultaneous build/test timings are not a new FPS
performance qualification.

### Final evidence

- [Debug: 85 tests](evidence/ui-menu-debug-tests.log)
- [Release: 45 tests](evidence/ui-menu-release-tests.log)
- [ASan/UBSan with leak detection: 36 tests](evidence/ui-menu-sanitizers.log)
- [Final focused rechecks](evidence/ui-menu-final-rechecks.log)
- [Debug SDL traversal](evidence/ui-menu-debug-traversal.log) and [Release SDL traversal](evidence/ui-menu-release-traversal.log)
- [Ten-page final GPU capture check](evidence/ui-menu-final-captures.log)
- [Preference restarts and malformed-file observation](evidence/ui-menu-preferences-restart.log)
- [Earlier GPU run, including the corrected traversal failure](evidence/ui-menu-gpu-development.log)

Code stages were committed as `aacb188` (UI-M0), `88536bf` (UI-M1/UI-M2),
`c715913` (UI-M3), and `2962b61` (UI-M4/UI-M5/UI-M6: Audio/Mouse, screens and final traversal).
Root validation records have matching commits; the user's root README stays local.
