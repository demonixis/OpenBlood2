# GI lot G3: the F8 overlay shell

2026-11-06, lot G3 of the [GI plan](../gi_plan.md). The contract is in
[gi_contracts.md](../../contracts/gi_contracts.md#f8-overlay-lot-g3). Host: Apple M5, macOS.

## What landed

- **`BLOOD2_GI_TOOLS`** (CMake): ON for desktop builds, Release included; OFF and refused on
  iOS and Android (Quest included).
- **F8 reserved** in `InputBindings::reserved_key`, with the **controls v3** migration: version
  1/2 F8 key lines are dropped and logged, version 3 refuses them, saves write version 3.
- **`MenuInput`**: the Closed/Open/Pinned states (`GiOverlay`), `gi_event` before the inspector
  gate, the demo-flow hand-over, text-entry and control-capture exclusions, and `tools_open()`
  replacing the inspector-only checks wherever play input is blocked.
- **`app/gi_overlay.{h,cpp}`** (`GiTools`): the Open window with the Status, Runtime, Light
  probes, Reflection probes and Bake panels (only Status works yet), the Pinned strip, and the
  camera-cell gizmo.
- **`ScenePresenter::tool_lines`**: tool gizmos drawn after the tracers with the existing line
  renderer; no shader artifact.
- **Tests:** `menus.gi_tools`, `gi_overlay.cell`, `gi_overlay.draw`, the private
  `gi_overlay.retail`, and updated `input_actions.table`, `rebind` and `controls_file`.

## Results

| Check | Result |
| --- | --- |
| Headless suite, dev build | 1,151 of 1,151 pass |
| `gi_overlay.retail` (01_SubwayA, `--gi-dir`) | No baked file → stale (albedo) → ready and chosen |
| Release preset (`BLOOD2_INSPECTOR` off, GI tools on) | `blood2_app` builds |
| ASan/UBSan debug build (ImGui asserts active) | `gi_overlay.cell`, `gi_overlay.draw`, `menus.gi_tools`: 3 of 3 pass |
| GPU frames with the overlay closed | 12 of 12 unchanged (`gpu.world`, `world_depth`, `world_alpha`, `world_alpha_private`, `world_debris`, `surface`, `post`, `lightmaps`, `world_sprite`, `screen_fade`, `menus`, `third_person`), plus `gpu.network_ballistic` (shared line upload) |

## Not done in G3

- The overlay has not been driven by hand in a running game window; its input states are
  covered by `menus.gi_tools` and its drawing by headless ImGui frames.
- Runtime, probe and Bake panels: G4, G8, G10. Placement markers and editing: G4.
- iOS, Android and Quest builds were not rebuilt (the option is off there by default).
