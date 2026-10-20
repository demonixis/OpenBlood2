# Credits and complete-level loading merge

2026-10-20, Linux/NVIDIA. The interrupted pull merges local `9bfada1`
(complete 58-world opening/CTF continuation) with `4f30f4b` (original Credits
world/menu/text roll and revised roadmap).

The three textual conflicts preserve both sides: project guidance, source
provenance, and the required `<cctype>`/`<climits>` includes in BloodShells.cpp.
The automatic merges retain CTF reset/drop/event handling, isolated world state,
boss behavior and HUD icons alongside credits entry, resource parsing and drawing.
The revised roadmap retains its new milestone structure while updating opening
coverage to 58/58 (36 non-multiplayer paths and 22 multiplayer paths).

## Validation

Builds succeeded with the existing dependency pins:

```sh
cmake --build build/dev --parallel 8 --target blood2 menu_tests ctf_tests hud_tests reported_level_tests world_tests
cmake --build build/release --parallel 6 --target blood2_app menu_tests ctf_tests hud_tests
```

- [Dev targeted suite](evidence/merge-credits-levels-dev-final.log): 32/32,
  covering menus/credits, all CTF cases, HUD, synthetic session/transition
  behavior, and private destination/level-change tests.
- [Release targeted suite](evidence/merge-credits-levels-release.log): 25/25,
  including all menus/CTF/HUD cases and actual GPU menu traversal, normal HUD and
  CTF HUD on the RTX 3060 Laptop GPU.
- [Complete audit](evidence/merge-credits-levels-audit.log): 58/58. Command:
  `build/dev/tests/reported_level_tests audit /home/yann/Games/Blood2`.
  Every world checks entry at difficulties 1/0/2, tiny initial deltas,
  180 simulation steps, durable runtime failures and unload ownership.
- [Release Credits frontend](evidence/merge-credits-levels-gpu.log):
  `build/release/blood2 --data-dir /home/yann/Games/Blood2 --world credits --view-world --frames 180 --capture /tmp/blood2-merge-credits.png`
  completes with exit 0 and no logged error or Vulkan validation failure.
  The capture is taken at frame 3, before visible credit text; it does not certify
  text layout or the interactive text-only credits screen.

The [first Dev run](evidence/merge-credits-levels-dev.log) failed only
`world.level_change`: its expected rejection message still excluded sprites,
although sky sprite support already exists in `6bcfff1`. The incoming
[credits evidence](evidence/menu-credits-tests.log) also records that pre-existing
failure. Only the exact expected message was updated; the rejected transaction,
retained source world and cleanup assertions remain intact. The repeated test
passes. Historical failure evidence is preserved.

Reproduce the targeted suites with:

```sh
ctest --test-dir build/dev --output-on-failure -j 3 -R '^(menus\.|ctf\.|hud\.|world\.(session_start|session_messages|session_lifecycle|level_change|destination_trials|destination_messages|destination_schema|destination_lifetime|level_change_private|destination_trial_private)$)'
ctest --test-dir build/release --output-on-failure -j 3 -R '^(menus\.|ctf\.|hud\.|gpu\.(menu_traversal|ctf_hud|hud)$)'
```

This validates merge integration, not the entire test suite, campaign traversal,
network play or M3 acceptance. The previously recorded full GPU opening sweep
remains separate evidence; it was not repeated here. GPU sanitizer reservation
M2-GPU-001 and the documented replacement credits font remain unchanged.
