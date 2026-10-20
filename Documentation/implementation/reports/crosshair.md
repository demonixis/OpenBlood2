# Original crosshair — 2026-10-18

The user's HUD follow-up adds the original default crosshair. The client source
renders it separately from `CNewStatusBar`: `CBloodClientShell::PostUpdate` loads a
numbered PCX, centers it inside the camera rectangle and uses black transparency.
The reconstruction now executes the selected source gates/placement using the
existing typed client state and HUD SDL_GPU renderer.

The installed game's defaults enable crosshair 1. The external
`interface/crosshairs/crosshair01.pcx` is 15x13 and is shown at its native size.
It disappears outside a world, during an external camera and after the received
player death event. The death camera's existing `active` state is authoritative;
health is not used to invent a second death rule. Statistics remain visible after
death, and crosshair visibility is independent of weapon/statistics validity.

See [contracts](../../contracts/crosshair_contracts.md),
[source hashes](../sources/crosshair_sources.json)
and the earlier [statistics HUD record](original_hud.md). No commercial assets,
source-tree changes, dependency updates or renderer/shader optimizations are
included. M2 remains closed with M2-GPU-001; M3 is not accepted here.

## Validation

Built `blood2_app`, `hud_tests` and `screen_fade_gpu_tests` with both the Debug and
Release presets. Release has inspection disabled. Rebuilt `hud_tests` in the
existing Clang sanitizer configuration. Commands ran from `Blood2/`.

| Check | Result |
| --- | --- |
| `ctest --preset debug -R '^hud\.'` | 5/5 passed |
| Debug `gpu.hud` and `gpu.screen_fade` | 2/2 passed |
| `ctest --preset release -R '^hud\.'` | 5/5 passed |
| Release `gpu.hud` and `gpu.screen_fade` | 2/2 passed |
| `ctest --test-dir build/sanitize -R '^hud\.' --output-on-failure` | 5/5 passed |

GPU tests used NVIDIA GeForce RTX 3060 Laptop GPU/Vulkan with
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json` and
`SDL_AUDIODRIVER=dummy`. Sanitizer tests used
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
GPU sanitizers were not rerun; the prior platform failure remains unsuppressed.

The GPU test renders eight states at each of 1200x780, 601x401 and 320x401:
live player, changed statistics, knife, dead player, external camera, live recovery
on a colored background, world exit, and crosshair without a valid statistics
panel. It compares every pixel in the source image rectangle with the decoded PCX,
using the actual RGBA/BGRA swapchain format. Black pixels must expose the background;
nonblack pixels must match the artwork. It checks native integer centering and
suppression without hiding the dead player's statistics. A colored-background
capture was visually inspected. Captures remain in the ignored build tree.

The screen-fade test now samples the scene 32 pixels away from center so its
scene-darkening assertion does not accidentally sample the visible crosshair.
No assertion is disabled or waived. An intermediate GPU run used the test binary
built before the new no-statistics frame input was saved and failed that new
assertion; the [failed run](evidence/crosshair-debug-gpu-initial.log) is retained.
After rebuilding the completed test input, all final checks above passed.

[Debug HUD](evidence/crosshair-debug-tests.log),
[Debug GPU](evidence/crosshair-debug-gpu.log),
[Release HUD](evidence/crosshair-release-tests.log),
[Release GPU](evidence/crosshair-release-gpu.log),
[sanitizers](evidence/crosshair-sanitize-tests.log).

## Limits

This enables the original default crosshair. The original cycling/toggle commands,
user CFG preference persistence, zoom-specific reticles, seeing-eye/orb modes,
multiplayer scoreboard and transition third-person camera remain unported.
Crosshair slots larger than 64x75 fail explicitly. Native Windows/macOS/mobile
execution and pixel parity against the original executable have not been tested.
