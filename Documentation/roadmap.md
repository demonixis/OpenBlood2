# Roadmap

## M0–M8

| Milestone | Completion | Scope |
| --- | --- | --- |
| M0 | 100% | Independent C++20 build, SDL_GPU, configuration, logs and inspector. |
| M1 | 100% | Original-code integration: objects, typed services, callbacks and lifetime. |
| M2 | 100% — closed with reservation | World readers, gameplay services and the selected SubwayA → CityHub session. |
| M3 | ~60% — next | Repeatable playable slice: movement, combat, AI, interactions, audio/FX/HUD and fidelity checks. |
| M4 | ~35% | Original rendering and inspection, validated on Vulkan/Linux, D3D12/Windows and Metal/macOS. |
| M5 | ~10% | Complete base campaign, remaining game rules, menus, cinematics and save/load. |
| M6 | ~15% | Desktop packages, measured optimization and sustained-play validation. |
| M7 | 0% | Android/iOS: ARM64, touch/controller input, storage, lifecycle and adaptive UI. |
| M8 | 0% | OpenXR: stereo, tracked controllers, aiming, menus, comfort and session recovery. |

Percentages are existing task estimates, not acceptance: M0/M1 are validated on Linux;
[M2](implementation/reports/m2_acceptance.md) is closed with **M2-GPU-001**, an unresolved
NVIDIA/D-Bus GPU LeakSanitizer failure. M3–M8 remain unaccepted.

## Next work

| Area | Planned work |
| --- | --- |
| Delivery order | M3, then M4/M5 → M6; mobile and VR follow the desktop baseline. |
| Input | Rebindable actions, gamepad, hotplug and saved profiles; touch for M7, tracked controllers for M8. |
| Materials / PBR | External texture packs, material definitions, normal/roughness/metalness maps and an optional PBR path; retain original rendering. |
| Post-processing | Bloom, tonemapping, GTAO and SSR, with per-effect quality settings. |
| Lighting | Baked GI/probes, reflection probes and blending for moving objects. |
| Upscaling | Render scaling and FSR1; assess the GSR SDK and device support for mobile. |
| Mobile — M7 | Android/iOS packaging, content import, touch UI, audio focus, resume and real-device thermal tests. |
| VR — M8 | SDL/OpenXR, stereo cameras, controller actions, comfort locomotion and arm IK on the original rig; standalone headsets also need Android. |
| Mods — M9 | Versioned packages, load order/conflicts, replacements/custom levels and save compatibility. Existing REZ overlays are a first subset. |
| In-game editor — M10 | Hierarchy, asset browser, gizmos, undo/redo, edit/play modes, materials/probes and mod export. |
| Later | Reassess Shogo after Blood II's campaign and engine contracts are established. |

Graphics work starts from the reviewed baseline after the other computer's push.
These are planned features, not implemented support or a request to begin M3.
