# Blood II — Consolidated and Extended Roadmap

Planning baseline: 18 October 2026.

This document consolidates M0–M8 and adds the requested input, graphics, modding
and in-game authoring work. It preserves the numbering and acceptance criteria of
the [original milestones](03_milestones.md). New work is planned scope, not an
implementation or acceptance claim. Implementation remains in the independent
`Blood2/` repository; Blood II preservation takes priority over a later Shogo port.

## Current acceptance baseline

- **M0 and M1:** completed for their documented Linux scope; see
  [M0 evidence](../../implementation/reports/m0.md) and [M1 evidence](../../implementation/reports/m1.md).
- **M2:** closed with user-authorized reservation **M2-GPU-001** for the selected
  SubwayA-to-CityHub session. The NVIDIA/D-Bus GPU LeakSanitizer failure remains
  an open platform qualification issue; failed tests remain failures. See the
  [authoritative closure record](../../implementation/reports/m2_acceptance.md).
- **M3–M8:** not accepted. M3 is next and requires a separate implementation
  request. Existing partial functionality does not satisfy an entire milestone.
- Original [menus](../../implementation/reports/original_menus.md),
  [statistics HUD](../../implementation/reports/original_hud.md) and
  [crosshair](../../implementation/reports/crosshair.md) have separate implementation records.
  They do not establish full campaign, persistence or M3 acceptance.
- Graphics optimization is assigned to another computer. Preserve the current
  graphics stack until the user reports a push; review that baseline before
  beginning or integrating the graphics work below.

## Preservation and platform milestones

| Milestone | Scope and expected result | Dependencies / acceptance |
| --- | --- | --- |
| **M0 — Project foundation** | Independent build, pinned SDL3 `main`, SDL_GPU, configuration, logging and inspector shell. | Completed within recorded Linux scope. |
| **M1 — Original-code integration** | Typed engine/game boundary, original classes, lifecycle, aggregates, handles and object inspection. | M0; completed within recorded Linux scope. |
| **M2 — World, session and gameplay services** | Bounded content readers, original object/service integration and selected SubwayA-to-CityHub session. | M1; closed with reservation M2-GPU-001. Do not reopen for the new features. |
| **M3 — First playable slice** | Repeatable original movement, combat, enemy behavior, trigger/door/pickup interactions, audio/FX/HUD and live inspection. | M2; compare with original source and executable where available, record discrepancies, repeat at different frame rates and across reloads. |
| **M4 — Portable rendering and inspection** | Required original world, model, sprite, transparency, lighting, sky and UI rendering on Vulkan/Linux, D3D12/Windows and Metal/macOS. | M3 for final acceptance; actual backend execution and visual/lifetime evidence. Carry M2-GPU-001 through platform qualification. |
| **M5 — Campaign and persistence** | Complete base campaign, remaining original rules/content, menus/audio/cinematics and versioned save/load across process restarts. | M3; final validation also requires M4. Campaign coverage matrix, equivalent restored state and repeated transitions. |
| **M6 — Desktop release and optimization** | Reproducible Windows/Linux/macOS packages, measured frame times and memory, fidelity regression checks and sustained-play validation. | M4 + M5; named hardware, explicit budgets and clean-machine installation tests. |
| **M7 — Mobile delivery** | Android/iOS ARM64, touch/controller play, content import, storage, lifecycle, audio focus and adaptive UI. | M6; real-device campaign/save/transition and sustained thermal tests. Platform smoke work starts during M4. |
| **M8 — OpenXR delivery** | Stereo views, tracked poses, VR controller actions, aiming, menus, comfort options and session recovery. | M6; standalone headsets also require the Android portion of M7. Validate named headset/runtime combinations and ordinary non-VR operation. |

The preservation critical path remains **M0 → M1 → M2 → M3**, followed by
**M4 + M5 → M6**. Mobile and XR retain distinct delivery gates. Assess Shogo
separately after Blood II's campaign and engine contracts are established,
distinguishing Aegis modifications from original Shogo behavior.

## New Input System — cross-cutting workstream

Build an action-based input layer shared by gameplay, menus and the editor,
while preserving the original game command semantics. This means a new input
system in this engine, not a dependency on Unity's Input System package.

| Stage | Planned work | Validation gate |
| --- | --- | --- |
| **INPUT-1 — Actions and gamepad** | Named actions, configurable bindings, keyboard/mouse parity, analog movement/look, dead zones, sensitivity, controller hotplug and persistent profiles. Separate gameplay, menu and editor input contexts. | Complete the M3 scenario and navigate menus with a gamepad. Verify switching devices, disconnection, pause and inspector focus without stuck or duplicated commands. Target M6 desktop delivery. |
| **INPUT-2 — Touch** | Configurable virtual controls, multitouch move/look/fire, scalable layouts and safe-area handling. | M7: real Android/iOS devices, simultaneous gestures, orientation changes and background/resume without residual input. |
| **INPUT-3 — VR controllers** | OpenXR action bindings, tracked hands/controllers, aiming, menu interaction and explicit locomotion/comfort mappings. | M8: named controllers/runtimes, tracking loss and session recovery. Record adaptations to original interaction rules. |

Keep device input, camera presentation and original simulation separate. Expose
active device, bindings, action values and input ownership in the inspector.

For M8 / INPUT-3, use inverse kinematics (IK) on the arms of the existing player
rig to follow VR controller poses, reusing the original skeleton and character
assets. Validate arm reach, joint limits and alignment with held weapons, and
define a fallback to existing animation when controller tracking is lost.

## Graphics overhaul — optional presentation workstream

Retain a selectable original presentation baseline for fidelity comparisons.
Introduce enhancements independently, with capability checks, quality settings
and measured CPU/GPU/memory costs. The overhaul does not replace M4's original
rendering coverage or retroactively enlarge the M2 gate.

| Stage | Planned work | Validation gate |
| --- | --- | --- |
| **GFX-1 — External textures** | Load user-supplied replacement textures outside retail archives; define asset mapping, precedence, supported formats, color space, alpha and mip handling. Show the selected source and load failures in inspection. | Replace world/model textures, disable the pack and recover original artwork. Test missing/invalid replacements, case-sensitive paths, reload and memory lifetime. Reuse the mod package resolver when available. |
| **GFX-2 — Upscaling: FSR1 and GSR (mobile)** | Add render-resolution selection and an upscaling stage, with FSR1 and a mobile GSR path. Confirm the exact GSR SDK/version, supported devices, integration terms and SDL_GPU feasibility before implementation. Keep native resolution available. | Compare native and upscaled captures, GPU time and memory on named desktop/mobile devices. Verify resizing, UI sharpness and unsupported-device fallback. Mobile acceptance belongs with M7. |
| **GFX-3 — Post Processing Stack** | Configurable Bloom, Tonemapping, GTAO and SSR, with explicit render targets, pass ordering and required depth/normal inputs. Keep HUD/menu composition readable. | Per-effect and combined comparisons, resize/reload checks, quality presets and backend validation. Document screen-space limitations and fallbacks; measure sustained mobile costs before enabling mobile presets. |
| **GFX-4 — Lighting improvement** | Baked GI with probe volumes/placements inspired by Unity's probe workflow, interpolation for moving objects, and reflection probes with capture, influence bounds and blending. Store bake results separately from original content. | Compare static and moving objects across probe boundaries; inspect probes and missing/stale bake data. Verify rebakes, level changes, memory budgets and fallback to original lighting. |

Implement on the reviewed graphics baseline after the other computer's push.
GFX-1 can start after resource contracts are stable; GFX-2–GFX-4 depend on M4's
rendering foundations and require their own backend evidence. Establish color
space and intermediate-buffer contracts before combining post-processing and
lighting. Determine final pass ordering through implementation and measurement.
The Unity references describe authoring ergonomics; the engine remains C++20,
SDL_GPU and the existing project stack.

## M9 — Mods support

**Goal:** load, identify and disable user content without modifying the retail
installation or making base-game behavior depend on installed mods.

Planned scope:

- Versioned package manifests, content roots, dependencies, deterministic load
  order, conflict reporting and engine/content compatibility declarations.
- Texture/material replacements first, followed by supported custom levels,
  object properties and other asset types as their import contracts are defined.
- An in-game mod list with enable/disable state and explicit restart/reload
  requirements; resource inspection reports package provenance.
- Save metadata recording the active mod set, with clear handling of missing or
  incompatible packages. Keep base-game saves and modified sessions identifiable.
- Documented packaging tools and a small original example mod containing no
  commercial assets. Gameplay extensions need a separately defined API and
  versioning contract; an arbitrary native-code or scripting plugin ABI is not
  assumed by this milestone.

**Depends on:** M5 resource/session/save contracts and M6's releasable baseline.
Package design may begin alongside GFX-1; M9 need not wait for mobile or XR merely
because its identifier follows M8.

**Exit gate:** install, enable, combine, disable and remove example mods; reproduce
load order and conflict diagnostics; round-trip a modded save; recover the original
session after disabling mods. Invalid packages fail clearly without corrupting
content or saves. Qualify desktop first, then mobile/XR distribution separately.

## M10 — In-game editor, Unity-style workflow

**Goal:** extend the existing Dear ImGui inspector into an integrated authoring
workspace with familiar scene, hierarchy, inspector and asset-browser workflows.
It is an evolution of project tooling, not a migration to Unity.

Planned scope:

- Dockable scene/game views, searchable object hierarchy, property inspector,
  asset browser and diagnostic console; include hidden/inactive and failed items.
- Scene selection, transform gizmos, snapping, duplicate/delete and undo/redo,
  using checked engine identities and the supported object/property contracts.
- Separate edit/play modes, pause/single-step and an inspection camera. Define
  whether edits are temporary runtime changes or saved authoring changes.
- Author supported object placements, lights, GI/reflection probes, materials and
  links; add bake controls and visualization as GFX-4 becomes available.
- Save versioned editor scenes/overrides into user or mod directories. Preserve
  retail archives; do not imply binary DAT editing/export before its format and
  build pipeline are explicitly supported.
- Export and reopen a mod package through M9, preserving references and reporting
  unavailable asset types or services.

**Depends on:** M3/M4 inspection and picking, stable editable service contracts,
and M9 for package export. Probe tools depend on GFX-4. Basic editing can develop
before full mod export; the full authoring gate follows these dependencies.

**Exit gate:** author a small scene or level override, manipulate objects and
properties, undo/redo, play-test, save, close and reopen it, then export/reload it
as a mod. Selection and references remain valid across destruction and reload;
editor input cannot leak into gameplay. Inspector-free builds retain equivalent
simulation behavior and omit editor work.

## Delivery and verification rules

1. Accept M3 before treating broader feature coverage as a playable baseline.
2. Continue M4/M5 toward M6; integrate INPUT-1 and independently scoped graphics
   increments without making every enhancement a prerequisite for preservation.
3. Deliver INPUT-2 with M7 and INPUT-3 with M8. Qualify GSR on the chosen mobile
   devices; do not infer support from a desktop build.
4. Develop M9/M10 against stable content and editing contracts; record each
   independently accepted increment rather than assigning completion by feature
   count. Scheduling and implementation requests remain separate from this plan.
5. For each delivered increment, record source/dependency revisions, original-code
   provenance where applicable, behavioral comparisons, tests, measured costs and
   known failures. Preserve historical evidence and the M2 reservation.

No dates or implementation percentages are promised here. The
[validation plan](04_validation_and_traceability.md) and each milestone's concrete
exit gate determine acceptance; a successful compilation alone does not.
