# Original statistics HUD — 2026-10-18

The user requested replacement of the demonstration in-game health/armor/ammo
panel with the UI produced by the original source. `Blood2/` now compiles a
selective `CNewStatusBar` port and presents its original PCX frames, icons, tickers
and bitmap digits. M2 remains closed with reservation M2-GPU-001. This change does
not accept M3 or alter the graphics stack assigned to the other computer.

Implementation commit: `Blood2 a51d477`.

See [contracts](../../contracts/original_hud_contracts.md) and
[source hashes/provenance](../sources/original_hud_sources.json).
The reference trees and all dependency pins/shaders are unchanged. Assets are read
from the configured external game installation; no commercial files are committed.

## Implemented

- Original `InitStatTabs`, statistical `AdjustRes`, `SetupTab` and `JustifySurface`
  code, with the source draw inclusion/order and normal/reserved weapon icon rules.
- Original high-detail and below-512x384 layouts; actual health/armor/primary ammo
  and distinct nonzero alternate ammo. The old English demo panel, system-font
  numbers, threshold coloring and bottom slot strip are removed. Numeric weapon
  commands retain their existing original inventory behavior.
- Typed maximum health/armor delivery from the actual player damage state, with
  normal message validation, lifetime and restoration. Values are not invented
  by the HUD. Maxima-only changes refresh the ticker.
- Bounded PCX/FNT reading, clipped color-keyed CPU composition and one cached
  SDL_GPU atlas through the existing UI renderer. Inspector-free Release uses the
  same presentation. Session inspection exposes resources, dimensions and maxima.

The original CoolFont implementation source is absent (only its interface/library
is present). The bitmap-font service is new and uses the observed external FNT
width data, not a system-font substitution. Other explicit adaptations include a
bounded number buffer, safe zero/over-maximum tickers and resource/GPU failure
reporting. This is source-based reconstruction, not a pixel-parity certification
against a captured 1998 executable.

## Validation

All commands ran from `Blood2/` on Linux. Private tests used the configured local
Blood II installation. GPU checks used NVIDIA GeForce RTX 3060 Laptop GPU/Vulkan,
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json` and dummy audio.

| Check | Command / result |
| --- | --- |
| Debug build | `cmake --build --preset debug --target blood2_app hud_tests screen_fade_gpu_tests -j 6` — passed |
| Debug HUD | `ctest --preset debug -R '^hud\.'` — 5/5 passed |
| Debug GPU | `ctest --preset debug -R '^gpu\.(hud\|screen_fade)$'` — 2/2 passed |
| Release build | `cmake --build --preset release --target blood2_app hud_tests screen_fade_gpu_tests -j 6` — passed, inspector disabled |
| Release HUD/GPU | Same filters with `--preset release` — 5/5 and 2/2 passed |
| Clang ASan/UBSan | `cmake --build build/sanitize --target hud_tests -j 4`, then `ctest --test-dir build/sanitize -R '^hud\.' --output-on-failure` — 5/5 passed with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` |
| Object/player regressions | Rebuilt `object_contract_tests player_tests player_restore_tests player_keepalive_tests screen_fade_tests`, then `ctest --preset debug -R '^(objects\|player\|player_restore\|player_keepalive\|screen_fade)\.'` — 33/33 passed |
| Actual application | `blood2 --data-dir <external-installation> --view-world --frames 1200 --capture build/debug/tests/original-hud-world.png` — exit 0, 1,200 inspector-hidden frames, 75.59 simulation seconds, 15 ready NPCs / zero failed |

The real-world capture option records an early intro frame, not a post-intro HUD
comparison. Dedicated GPU captures verify native statistics rendering at 1200x780,
600x400 and 320x400, changed health/ammo, knife switching and player departure;
fade captures verify the artwork stays above a fully black scene. Both the normal
and low-detail dedicated captures were visually inspected. They remain under the
ignored build directory. Tests use real PCX assets with explicitly synthetic player
scenarios, not a claim of recorded retail behavior.

Parser tests cover invalid header/stride/plane, zero/overlong RLE, missing palette,
truncation, trailing data, font overflow and missing resources. Private tests cover
source positions, resolution thresholds, all five supported normal weapon icons,
reserved-slot clearing, alternate ammo and safe extreme/zero-maximum values.
The 33 regression tests passed before the final presentation-only removal of unused
demo projection fields; the HUD/GPU/sanitizer sets were rebuilt and rerun afterwards.

Initial integration builds exposed the old `draw_hud` call in the fade test; it was
updated to exercise the actual bitmap renderer. GPU HUD/fade tests now require the
private asset root rather than silently testing replacement artwork. No runtime
or test failure is waived. GPU sanitizers were not rerun: the separate existing
M2-GPU-001 NVIDIA/D-Bus qualification failure remains open and unsuppressed.

[Debug tests](evidence/original-hud-debug-final.log),
[Debug GPU](evidence/original-hud-debug-final-gpu.log),
[Release tests](evidence/original-hud-release-final.log),
[Release GPU](evidence/original-hud-release-final-gpu.log),
[sanitizers](evidence/original-hud-sanitize-final.log),
[regressions](evidence/original-hud-regression.log),
[application session](evidence/original-hud-world.log).

## Remaining UI

This replaces the demonstration statistics HUD. Inventory scrolling, weapon-list
toggle, objective sheet/skull, power/air/throw/boss bars, communication portraits
and text, multiplayer/team displays and menus remain separate original-UI work.
The existing inspector boss bar is still diagnostic. No Windows/macOS/mobile
execution, complete-game UI or retail screenshot equivalence is claimed.
