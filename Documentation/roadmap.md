# Roadmap

Reviewed on 2026-10-20; level-opening coverage reconciled with `9bfada1` and
[audit 19](implementation/reports/evidence/all-levels-audit-19.log).
Percentages are task estimates, not acceptance. Only the user accepts a milestone.

## Overall progress

| Scope | Estimate | Basis |
| --- | ---: | --- |
| Mounted non-multiplayer worlds that open | 36 / 36 | Includes Ancient City, Ancient One, Credits, End and Drop; opening is not campaign traversal. |
| All mounted worlds that open | 58 / 58 | Includes 22 multiplayer maps, inspected locally, with the four CTF maps. |
| Beta-ready game (M0–M4) | **~88%** | Weighted milestones below. |
| With the graphics upgrade (M0–M6) | ~78% | Adds optional PBR and post-processing. |
| Whole roadmap (M0–M9) | **~58%** | Adds gamepad, Quest VR/Android and LAN. |

Weights reflect estimated effort: M0 4, M1 6, M2 28, M3 14, M4 8, M5 7,
M6 6, M7 3, M8 11, M9 13. Later work (M10+) is not counted.

"Opens" means the world passes the
[all-level audit](implementation/reports/all_level_loading.md): player entry at
three difficulties and three seconds of simulation. That audit does not test
full traversal or combat. UndeadGideon, AncientOne, AncientOneTentacle and
FlagStand now have original-source ports and focused behavior tests. All 58
worlds also passed the inspector-free Release GPU opening sweep. This does not
accept M3 or provide network play.

## Milestones

| Milestone | Completion | Scope |
| --- | --- | --- |
| M0 | 100% | Independent C++20 build, SDL_GPU, configuration, logs and inspector. |
| M1 | 100% | Original-code integration: objects, typed services, callbacks and lifetime. |
| M2 | 100% — closed with reservation | World readers, gameplay services and the selected SubwayA → CityHub session. |
| M3 | ~55% — current | Every level playable, credits, save/load, then a public beta whose bug list validates the campaign. |
| M4 | ~85% | Original rendering on Vulkan (Linux, Windows) and Metal (macOS). D3D12 is not planned. |
| M5 | ~65% | Optional PBR shader set with JSON texture/material bindings; the legacy set remains selectable. The [enhanced renderer](contracts/enhanced_renderer_contracts.md) covers 5.0, 5.1 (built-in manifest), 5.3 and 5.4; emissive and inspection remain. |
| M6 | 0% | PBR-only post-processing: bloom from emissive maps, GTAO, SSR, tonemapping; AA for both sets. |
| M7 | ~5% | Gamepad: action layer, rebinding, hotplug, menus; a parallel track, needed before M8. |
| M8 | 0% | VR on Meta Quest through OpenXR; also the Android gate. |
| M9 | ~10% | LAN multiplayer: Bloodbath, CTF and Team on the original multiplayer maps. |

The milestones after M4 were renumbered on 2026-10-20. Earlier reports use the
old numbers: M5 campaign/persistence, M6 packages and optimization, M7 mobile,
M8 OpenXR. [M2](implementation/reports/m2_acceptance.md) is closed with
**M2-GPU-001**, an unresolved NVIDIA/D-Bus GPU LeakSanitizer failure.

## Delivery order

1. **M3.** The Linux agent makes every level playable. Then the menu's Credits
   entry opens the credits world, save/load is implemented, and a beta collects
   bugs.
2. **M5 and M6.** The graphics upgrade: optional PBR first, then post-processing.
   The PBR texture work identifies emissive surfaces, which makes bloom, GTAO and
   SSR meaningful.
3. **M8.** VR on Quest, which also establishes the Android build.
4. **M9.** LAN multiplayer.

M7 (gamepad) runs alongside these and must land before M8. M4's remaining items
are fixed as the beta finds them.

## M3 — playable campaign and beta

The gate is real players, not scripts: a beta build is played from a new game to
the credits, and the reported bug list is triaged. No remaining defect may block
progress, and every one is recorded.

| # | Task | Status |
| --- | --- | --- |
| 3.1 | Every mounted world opens (all-level audit) | Done: 58 / 58 at three difficulties, plus the Release GPU opening sweep. Full traversal remains separate. |
| 3.2 | Menu Credits entry opens the `credits` world | Done: original custom-type request, the CCredits text roll from cres.dll over the world, and the in-world text-only credits screen ([report](implementation/reports/menu_credits.md)). The font is a replacement. |
| 3.3 | Save/load: the original slots, Quicksave/Quickload and Restart rows on the existing screens, and whole-world plus player serialization | Phase 0 done: [audit](implementation/save_load_audit/readme.md), [contract and 15-lot plan](contracts/save_load_contracts.md); the seven decisions were accepted on 2026-10-20. Lot 1 (save-message primitives, [evidence](implementation/reports/save_load_messages.md)), lot 2 (container and fingerprint, [evidence](implementation/reports/save_load_container.md)), lot 3 (engine records, clock and two-pass `SaveObjects`/`RestoreObjects` over an in-memory store, [evidence](implementation/reports/save_load_records.md)) lot 4 (save-support declarations, `USRFLG_SAVEABLE` marking, restore-mode world and the `save_game`/`load_game` candidate transaction, [evidence](implementation/reports/save_load_session.md)) lot 5 (the player, its aggregates and client blob, reattached to a new client, [evidence](implementation/reports/save_load_player.md)) and lot 6 (slot, quick and Restart files, enabled Save/Load rows, F5/F6 and death to the Load menu, [evidence](implementation/reports/save_load_slots.md)) are done, as is lot 8 (movers and brushes: original Save/Load of doors, rotators, destructible brushes and models, volumes, TripLaser and Tchernotronic; all 50 sky worlds restore their sky objects, [evidence](implementation/reports/save_load_movers.md)), lot 7 (world logic: triggers, spawners, keyframers, cameras and cinematics, [evidence](implementation/reports/save_load_world_logic.md)), lot 13 (native state: engine classes, sky and PolyGrid republication checked once per load with both renderers, VoiceMgr/CommLink across a load, CTF saves refused by name, [evidence](implementation/reports/save_load_native.md)), lot 10 (effect publishers and audio emitters: original Save/Load of SoundFX, RandomSoundFX, lights, scene explosions, particle streams, lightning, warp gates and DetailSprite, with republished effects and four decision-4 fixes; with lots 7 and 11 five worlds save and load whole, [evidence](implementation/reports/save_load_effects.md)), lot 11 (AI core: AI_Mgr with its script and path lists, the NPC classes and CCorpse; all 35 NPC worlds restore their supported objects, [evidence](implementation/reports/save_load_ai.md)) and lot 12 (bosses and special enemies: the Gideon family, the Ancient One and its tentacles, Naga, Zealot, BoneLeech/Thief/TheHand with the player attachment and DrudgePriest, [evidence](implementation/reports/save_load_bosses.md)) and lot 9 (pickups and projectiles: original Save/Load of PickupObject, KeyPickup, CProjectile and SeeingEye, with the shooter kept (D15) so restored remote charges and thrown eyes stay their player's, [evidence](implementation/reports/save_load_pickups.md)); lot 14 (whole-world certification) is pending, and the private-data checks must be re-run once the game data is restored. Existing building blocks: typed CInventoryMgr, CDestructable and player-field state used by NEW_LEVEL. The screens work; real games are refused by class name until the class lots land. |
| 3.4 | Campaign chaining: every authored NEW_LEVEL/EndEpisode in order, with KeepAlive inventory and stats | SubwayA → CityHub has a scripted test. The user has played the rest without a recorded result. |
| 3.5 | Bosses and endgame: Gideon, EnhancedGideon, UndeadGideon, AncientOne, End script, credits | Original-source ports, focused behavior tests and world opening pass; a complete player-driven endgame remains unverified. |
| 3.6 | Beta build: CI archives for the three platforms, logs and crash capture, a bug report channel | CI archives, logs and crash capture exist. |
| 3.7 | Beta bug triage and fixes, each fix with a regression test; retest the existing reports (post-transition stutter, idle Cultists, frozen corpses, NPC placement) | Waits on 3.6. |
| 3.8 | M3 acceptance record for the user's decision | Not started. |

## M4 — original rendering

Metal has been tested extensively on macOS, Vulkan on Fedora 44, and Vulkan
briefly on Windows. D3D12/DXIL is dropped. The work left is broader Windows
testing, plus any gaps the beta finds. Known gaps include underwater fog,
global translucent ordering and realtime light illumination. The original
renderer remains the reference and stays selectable after M5.

## M5 — optional PBR

Donor: NoMadEngine `6d91073` (2026-01-22). It is an inspiration, never a build
dependency. What it actually provides:

- `assets/blood2/config/materials.json`: 10 material classes (roughness,
  metallic, specular). 28 case-insensitive patterns map texture paths to a
  class, and the first match wins.
- A rule that classifies surfaces from words in the texture name: water,
  glass, lava, sky, clip and invisible. Each class sets alpha and material
  values.
- No normal, roughness or emissive maps. Bump comes from screen derivatives
  of albedo luminance, and lava only imitates glow through specular.
- Lighting is Blinn-Phong with roughness mapped to shininess, so it is not a
  GGX/Cook-Torrance model.
- OpenGL/GLSL only, so none of its shaders can be used as they are in the
  HLSL/SDL_GPU pipeline.
- Its matcher splits a pattern at the first `*` only. Patterns with two
  stars, such as `SKINS/*SKIN*`, therefore never match. Do not copy this.

| # | Task |
| --- | --- |
| 5.0 | Measure the donor patterns against the retail texture catalogue (world DATs, ABC skins, sprites). List what is covered, what is missed and what is ambiguous. |
| 5.1 | Versioned JSON manifest: classes plus wildcard patterns, exact per-texture overrides and optional maps (normal, roughness/metal, AO, emissive). Bounded parsing with explicit errors. Texture packs stay external. |
| 5.2 | Emissive identification: DAT fullbright/SURF_BRIGHT faces, the donor's lava/fluid classes, manifest entries and emissive maps. This is the input for M6 bloom. |
| 5.3 | A PBR shader set beside the legacy set: GGX/Cook-Torrance world and skinned-model shaders that output tangents, on the existing lightmaps, static lights and model shadows. Normal maps where they exist, and an optional derivative bump otherwise. Sprite, particle, shadow-atlas, sky and UI shaders stay shared. Regenerate all SPIR-V/MSL artifacts together. |
| 5.4 | Display option to switch between PBR and the legacy set. The legacy set is frozen as the original reference: it keeps its current pixel output, checked by the GPU readback tests, and takes no PBR inputs. |
| 5.5 | Material inspection (resolved class, maps, emissive flag) and fallback reporting for missing or invalid bindings. |

Status (2026-10-20, [contract](contracts/enhanced_renderer_contracts.md)):

- **Done.** 5.0: the retail catalogue is measured by `surface.retail`, and the donor
  patterns match nothing in it. 5.3 and 5.4: RENDERER/LIGHTING/NORMAL MAPS rows, and the
  original set is byte-identical.
- **Partial.** 5.1: versioned built-in manifest with rules and overrides; external maps
  and texture packs are not loaded yet. Generated normal maps replace the
  derivative-bump idea.
- **Open.** 5.2 (emissive) and 5.5 (inspection).
- **Not verified.** Vulkan execution, Linux and Windows.

## M6 — post-processing

The donor has a GL post stack: a threshold bloom (downsample, Kawase blur,
upsample), SSAO (not GTAO), SSR, SSGI and motion blur, plus an "uber" pass
with ACES tonemapping, FXAA, sharpening, vignette and color grading. These
are design references only. The target, all for the PBR set:

- An HDR scene target.
- Bloom driven by the emissive mask from 5.2 instead of a luminance threshold
  alone.
- GTAO, then SSR.
- Tonemapping and anti-aliasing.
- A quality setting per effect, and every effect can be turned off.

Post-processing is PBR-only. The legacy set renders in low dynamic range and
has no roughness or normal inputs. Tonemapping, bloom, SSR and GTAO would
therefore either change the original look or be meaningless there. Only
effects that keep the image's meaning stay available to both sets:
anti-aliasing, and later render scaling and FSR1.

## M8 — VR on Quest

- Android ARM64 build, APK packaging and installing the external game data on the device.
- SDL_GPU on Vulkan under Android.
- OpenXR session and stereo rendering at the headset's frame budget.
- Tracked-controller actions on top of the M7 action layer.
- Comfort locomotion, arm IK on the original rig, menus in VR, and pause/resume recovery.

Main risk: the frame budget on a mobile GPU. The Quest version may need
reduced settings, and PBR/post-processing are likely optional there.

## M9 — LAN multiplayer (evaluation)

Original model: the authoritative server runs the ObjectDLL objects. The client
shell runs its own movement (CMoveMgr) and effects. The engine copies object
state to every client. OpenBlood2 runs one session in one process with one
client. Presentation, HUD and audio read server objects directly. Typed client
messages exist as in-process values only, and there is no network code.

Original references are available. These include the multiplayer branches in
PlayerObj, BloodServerShell, the pickups and B2BaseClass, as well as FlagObjects,
TeamMgr, FragInfo, MessageMgr and NetStart. NetStart is a Win32 dialog wizard
that must be replaced. The later LithTech network layer, about 9k lines, is a
reference only. The source names Co-op, but only Bloodbath, CTF and Team maps
ship, so Co-op is assessed separately.

| # | Task | Size | Depends on |
| --- | --- | --- | --- |
| 9.0 | Contracts and audit: every direct server-state read by the app, every original multiplayer branch, the choice of transport library (for example a pinned ENet) | S | — |
| 9.1 | Client replica: presentation (original, PBR and post-processing paths), HUD, audio and inspector read a client world that server snapshots feed. Loopback solo must match current behavior, checked by the GPU readback tests. | XL | 9.0 |
| 9.2 | Wire protocol and transport: versioned, bounded encoding of every typed message; UDP with reliable and unreliable channels; fuzzed decoders | M | 9.0 |
| 9.3 | Sessions: listen and headless dedicated server, LAN discovery, handshake (protocol, data and mod fingerprint), join/leave/timeout, map rotation | M | 9.2 |
| 9.4 | Multi-player server: N players and clients, the original multiplayer branches, respawn, slow death and humiliation, frags, item respawn, game-type filtering, start points, frag and time limits | L | 9.1 |
| 9.5 | Game modes: Bloodbath; CTF with FlagStand, FlagObjects and TeamMgr; Team. The CTF objects, capture rules, world-owned scores and team HUD already run in local inspection ([contracts](contracts/ctf_contracts.md)); network team management and the other modes remain. | M | 9.4 |
| 9.6 | Network movement and fire: the original client-side movement upload with server bounds, remote interpolation, server-side hits, loss and latency | M–L | 9.2, 9.4 |
| 9.7 | Multiplayer UI: host and join screens, name, character and team, FragInfo scoreboard, MessageMgr chat, gamepad navigation | M | 9.3, M7 |
| 9.8 | Validation: headless multi-client loopback on every bb_ and ctf_ map, simulated loss and latency, sanitizers, Windows/Linux/macOS interoperability, a LAN playtest | M | all |

Sizes: S ≈ 1 lot, M ≈ 2–3, L ≈ 4–5, XL ≈ 6–8, where a lot is one
continuation-sized change. Total: about 25–35 lots. Task 9.1 carries most of the
risk because it changes how every renderer path reads state. Internet play, a
master server and NAT traversal are out of scope.

## Later

| Area | Planned work |
| --- | --- |
| Mobile — M10 | Phones/tablets on Android and iOS: touch UI, audio focus, resume, thermal tests. This reuses the M8 Android build. |
| Mods — M11 | Versioned packages, load order/conflicts, replacements/custom levels and save compatibility. Existing REZ overlays are a first subset. |
| In-game editor — M12 | Hierarchy, asset browser, gizmos, undo/redo, edit/play modes, materials/probes and mod export. |
| Rendering extras | Baked GI/probes, reflection probes, render scaling and FSR1. |
| Desktop release | Signed/notarized installers, sustained-play soak tests and measured optimization, continuing from the M3 beta. |
| Shogo | Reassess after Blood II's campaign and engine contracts are established. |
