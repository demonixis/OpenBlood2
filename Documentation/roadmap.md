# Roadmap

Reviewed on 2026-10-25 (beta VFX lots 1–10); level-opening coverage reconciled with `9bfada1` and
[audit 19](implementation/reports/evidence/all-levels-audit-19.log), save/load with lot 14.
Percentages are task estimates, not acceptance. Only the user accepts a milestone.

## Overall progress

| Scope | Estimate | Basis |
| --- | ---: | --- |
| Mounted non-multiplayer worlds that open | 36 / 36 | Includes Ancient City, Ancient One, Credits, End and Drop; opening is not campaign traversal. |
| All mounted worlds that open | 58 / 58 | Includes 22 multiplayer maps, inspected locally, with the four CTF maps. |
| Beta-ready game (M0–M4) | **~90%** | Weighted milestones below. |
| With the graphics upgrade (M0–M6) | ~90% | Adds optional PBR and post-processing. |
| Whole roadmap (M0–M9) | **~66%** | Adds gamepad, Quest VR/Android and LAN. |

Weights reflect estimated effort: M0 4, M1 6, M2 28, M3 14, M4 8, M5 7,
M6 6, M7 3, M8 11, M9 13. Later work (M10+) is not counted.

"Opens" means the world passes the
[all-level audit](implementation/reports/all_level_loading.md): player entry at
three difficulties and three seconds of simulation. That audit does not test
full traversal or combat. UndeadGideon, AncientOne, AncientOneTentacle and
FlagStand now have original-source ports and focused behavior tests. All 58
worlds also passed the inspector-free Release GPU opening sweep. This does not
accept M3 or provide network play.

The [2026-10-25 tester corrections](implementation/reports/tester_feedback_2026_10_25.md)
cover CityHub's laundromat doors, cutscene look and texture
filtering. The [additional controls](implementation/reports/control_actions.md) now implement
all nine previously disabled action rows. The [desktop chase camera](contracts/camera_contracts.md)
adds an optional C toggle with the crosshair retained, disabled in VR. The [destruction-debris lot](contracts/destruction_debris_contracts.md)
connects original wood, metal and glass fragments, their motion and impact sounds.
The [class coverage inventory](contracts/class_coverage.md) distinguishes absent ports
from replacement services. This does not expand milestone acceptance.

The [2026-11-05 corrections](implementation/reports/tester_feedback_2026_11_05.md) (0.5.2)
fix the CDM smoke-trail crash, sky culling/SkyFog/FarZ, interleaved GOTHRUWORLD doors,
exploding-debris effects, chroma-keyed glass, underwater tint, conveyor/water-jump cadence,
embedded trip lasers, view-weapon sound keys and the Gabriella spawn, and add an adjustable
field of view.

The [2026-10-28 corrections](implementation/reports/tester_feedback_2026_10_28.md) (0.5.1)
stop NPCs freezing (and blocking saves) after damage-volume or self damage, domain-edge
rays against moved world models and normal-less obstacles, and port the original F1
weapon tabs.

The [2026-10-27 correction](implementation/reports/tester_feedback_2026_10_27.md) stops
objects authored over the void (02_CityHubB's `startcrate`) from failing the session after
100 s: original `FLAG_REMOVEIFOUTSIDE` removal and a hold for other lost bodies.

The [2026-10-26 tester corrections](implementation/reports/tester_feedback_2026_10_26.md)
(0.5.0) reproduce and fix the Museum crash (Fanatics spawned into the skylight glass
under Gideon's last hologram), add Hor+ widescreen projection, black letterbox bars,
filtered sprites, escalator fog, gravity-free resize and `cres.dll` data mods.
[Activation](contracts/activation_visibility_contracts.md) now walks the decoded DAT leaf
visibility lists (CityHub's idle Gideon, Train Station guards). Chosen games now start
the original ACTION type without cut scenes ([menu contracts](contracts/menu_contracts.md#chosen-games-gametype_action)).
Next: door portals for activation; Nightmares stays a separate port (see the
[Nightmares plan](implementation/nightmares_plan.md); lots N1, N2 and N8, the edition, mount, menu, world paths and robe Cultists, are done).

The [Demo_01 computer/door correction](implementation/reports/demo_computer_doors.md)
fixes adjacent sliding leaves refusing exact closed-endpoint contact after the
ventilation sequence. Frame-specific failure diagnostics replace misleading startup
history in the crash log. The reported 93-second simulation stop remains unconfirmed;
the checked sequence and repeated door cycles are not a full demo traversal.

## Milestones

| Milestone | Completion | Scope |
| --- | --- | --- |
| M0 | 100% | Independent C++20 build, SDL_GPU, configuration, logs and inspector. |
| M1 | 100% | Original-code integration: objects, typed services, callbacks and lifetime. |
| M2 | 100% — closed with reservation | World readers, gameplay services and the selected SubwayA → CityHub session. |
| M3 | ~65% — current | Every level playable, credits, save/load, then a public beta whose bug list validates the campaign. |
| M4 | ~85% | Original rendering on Vulkan (Linux, Windows) and Metal (macOS). D3D12 is not planned. |
| M5 | ~85% | Optional PBR shader set with JSON texture/material bindings; the legacy set remains selectable. The [enhanced renderer](contracts/enhanced_renderer_contracts.md) covers 5.0, 5.1 (built-in manifest), 5.2 (emission), 5.3, 5.4 and 5.5 (material inspection); external maps, texture packs and Vulkan verification remain. |
| M6 | ~90% | PBR-only post-processing: bloom from emissive maps, GTAO, SSR, tonemapping; AA for both sets. [Post-processing](contracts/post_processing_contracts.md) covers the HDR scene, emission-driven bloom, NEUTRAL/FILMIC tonemaps, MSAA/FXAA, half-size GTAO and screen-space reflections (LOW/HIGH each); Vulkan verification and XR remain. |
| M7 | ~90% | Gamepad: action layer, rebinding, hotplug, menus; a parallel track, needed before M8. The [input actions](contracts/input_action_contracts.md) cover keyboard, mouse and gamepad (console layout, hot-plug, menus, weapon cycling), the original [inventory cursor](contracts/inventory_cursor_contracts.md) with its item tabs, a touch camera drag, the original Controls page with `controls.cfg`, all nine previously missing control actions, and on-screen touch controls. Device testing on a touch screen, menu auto-repeat and the Keyboard/Joystick pages remain. |
| M8 | ~55% | VR through OpenXR: Meta Quest first, PCVR on Linux last; also the Android gate. The [OpenXR plan](implementation/openxr_plan.md) orders the lots; 8.1–8.2 reached [first light on a Quest 3](implementation/reports/xr_first_light.md) 8.3 [renders SubwayA in stereo at 72 Hz](implementation/reports/xr_stereo_world.md) 8.5 adds [controller play](implementation/reports/xr_controls.md), 8.6 the [menus and HUD](implementation/reports/xr_menus_hud.md), 8.7 the [menu laser](implementation/reports/xr_laser_playtest.md) and 8.9 [arm IK](implementation/reports/xr_arms.md). |
| M9 | ~10% | LAN multiplayer: Bloodbath, CTF and Team on the original multiplayer maps. |
| M10 | ~22% | Baked GI: GI lightmaps that override the original ones, light probes for models and reflection probes for the enhanced renderer, with an F8 tools overlay and a bake-all script. The [GI plan](implementation/gi_plan.md) orders the lots; G1 adds the [world queries](contracts/gi_contracts.md), G2 the [baked file, keys and search order](contracts/gi_contracts.md#baked-file-and-keys-lot-g2), G3 the [F8 overlay shell](contracts/gi_contracts.md#f8-overlay-lot-g3), G4 [probe placement](contracts/gi_contracts.md#probe-placement-lot-g4) with `--gi-place` and F8 editing (Gate A review pending). |

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
| 3.3 | Save/load: the original slots, Quicksave/Quickload and Restart rows on the existing screens, and whole-world plus player serialization | Done: the [contract](contracts/save_load_contracts.md) and its 14 lots, from the [audit](implementation/save_load_audit/readme.md) to the [whole-world certification](implementation/reports/save_load_certification.md). All 36 single-player worlds save and load whole at difficulties 0–2, at entry and after combat, then run 30 s; Restart after a load, a mid-route SubwayA save that still reaches CityHub, GPU readback, ASan/UBSan and the inspector-free Release build pass. Multiplayer and CTF saves are refused (out of scope); lot 15 (NEW_LEVEL on the save path) is optional. |
| 3.4 | Campaign chaining: every authored NEW_LEVEL/EndEpisode in order, with KeepAlive inventory and stats | SubwayA → CityHub has a scripted test. The user has played the rest without a recorded result. |
| 3.5 | Bosses and endgame: Gideon, EnhancedGideon, UndeadGideon, AncientOne, End script, credits | Original-source ports, focused behavior tests and world opening pass; a complete player-driven endgame remains unverified. |
| 3.6 | Beta build: CI archives for the three platforms, logs and crash capture, a bug report channel | CI archives, logs and crash capture exist. |
| 3.7 | Beta bug triage and fixes, each fix with a regression test; retest the existing reports (post-transition stutter, idle Cultists, frozen corpses, NPC placement) | In progress: Discord tester fixes, all nine control actions and VFX lots 1–10 implemented (details below). Museum crash remains unreproduced; full campaign retest remains. |
| 3.8 | M3 acceptance record for the user's decision | Not started. |

## M4 — original rendering

Metal has been tested extensively on macOS, Vulkan on Fedora 44, and Vulkan
briefly on Windows. D3D12/DXIL is dropped. The work left is broader Windows
testing, plus any gaps the beta finds. VFX lot 10 implements scene light illumination,
volume/world fog and global translucent ordering. Fog lights, FX-light shadows, SkyFog
and matched original-executable visual qualification remain explicit limits. The original
renderer remains the reference and stays selectable after M5.

### Original VFX completion (beta feedback, M3.7 / M4)

| Lot | Scope | Status |
| --- | --- | --- |
| 1 | Wood, metal and glass destruction fragments: original resources, launch, scale, transparency, bounce sounds and eviction/fade | Implemented; [contract and verification](contracts/destruction_debris_contracts.md). Applies to CDebris callers on destructible models and brushes. Other materials and explosion components remain separate. |
| 2 | Sawed-off shotgun (weapon 4): compare primary/alternate fire against the original, complete missing casing/muzzle/impact components | Implemented: original single/double muzzle flashes, barrel/reload animation, secondary casings/smoke, material marks/fragments and liquids; [contract and verification](contracts/shotgun_contracts.md#original-vfx-completion-lot-2-2026-10-25). Engine collision/clipping/pool adaptations remain explicit. |
| 3 | Inventory of remaining weapon/environment VFX, separating missing implementation, partial effects and absent original resources | Done: [source audit](implementation/reports/vfx_inventory.md), including all 32 base explosion recipes, connected callers and conditional legacy emitters. Source coverage is not visual parity. |
| 4 | Shared ballistic presentation: authored muzzle flashes, casings, tracers, marks/fragments and liquid effects for ordinary bullet weapons | Implemented; [contract and verification](contracts/ballistic_effects_contracts.md). Per-ammo rules, dual hands, original tracer normal-ray quirk and Minigun's empty impact path preserved. Clipping/collision/lighting boundaries remain explicit. |
| 5 | Flare gun: primary/secondary trails and fizzle/burst/fragment effects (13–15) | Implemented; [contract and verification](contracts/flare_contracts.md#original-vfx-completion--lot-5-2026-10-25). Primary/fragment trails, impacts 13–15 and restore republication; alternate parent remains trail-free. Illumination stays separate. |
| 6 | Tesla: primary impact (7), alternate beam and ball arcs; connect Drudge lightning through the same service | Implemented; [contract and verification](contracts/tesla_contracts.md#original-vfx-completion--lot-6-2026-10-25). Impact 7 and actual Tesla/Drudge lightning, with separate visual/damage rays and original forms. Illumination stays separate. |
| 7 | Explosive weapons: Howitzer impacts (9–11), Napalm alternate (5) and missing projectile smoke connections | Implemented; [contract and verification](contracts/explosive_effects_contracts.md). Impacts 5/9/10/11, Napalm smoke and M16-grenade/Howitzer fragment trails; existing damage, grenade/bomb and Napalm primary effects preserved. |
| 8 | Remaining energy/boss/environment recipes: Singularity, Death Ray, enemy trails/impacts, Naga and Spawner rift | Implemented; [contract and verification](contracts/energy_effects_contracts.md). Source composites 12/18/19/23–25/28–31, enemy trails, Singularity absorption and Death Ray rings/electricity; restored trails republished once. Rift 24 has no established live caller. |
| 9 | Remaining destruction materials and evidenced environment emitters | Implemented: stone/plastic/terrain/flesh and original metal fallback for liquid/energy; [verification](implementation/reports/remaining_debris.md). Rain/legacy emitters remain conditional P3 until an exercised level/caller is identified. |
| 10 | M4 rendering dependencies: realtime FX illumination, underwater fog, global translucent ordering and clipped marks | Implemented; [contract and verification](contracts/scene_effects_contracts.md). Dynamic lights selected per receiver (12 mobile / 32 desktop), volume/world fog, interleaved transparent families and polygon-clipped marks; source flags and explicit renderer adaptations. Fog lights, FX shadows and exact original raster parity remain separate. |

The user authorized the continuation on 2026-10-25; lots 4–10 are implemented, after the user's renderer synchronization confirmation for lot 10. P1 restores frequent weapon feedback;
P2 completes less common weapon/boss/destruction effects; P3 requires evidence of
campaign use. Each implementation lot needs source-parameter checks and matched
original/port visual scenarios; the inventory itself changes no runtime behavior.

These lots restore original presentation. They do not reopen M2 or replace M3's
player-driven campaign/endgame acceptance. The reported Museum crash takes priority
when a reproducible route, save or log becomes available.

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

Status (2026-10-21, [contract](contracts/enhanced_renderer_contracts.md)):

- **Done.** 5.0: the retail catalogue is measured by `surface.retail`, and the donor
  patterns match nothing in it. 5.2: the DAT's fully bright faces are counted per material,
  and manifest emission (light, neon, screen and hazard classes) adds a light-independent
  glow in the enhanced shaders, the input for M6 bloom. 5.3 and 5.4: RENDERER/LIGHTING/NORMAL
  MAPS rows, and the original set is byte-identical. 5.5: the inspector's "Enhanced
  materials" report (class, binding, maps, emission, fallbacks).
- **Partial.** 5.1: versioned built-in manifest with rules and overrides; external maps
  (including emissive maps) and texture packs are not loaded yet. Generated normal maps
  replace the derivative-bump idea.
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

Status (2026-10-22, [contract](contracts/post_processing_contracts.md)):

- **Done.** RGBA16F scene for the enhanced set; bloom from a glow target that only the
  manifest emission writes (Jimenez chain, 6 levels); OFF/NEUTRAL (Khronos PBR Neutral
  shoulder, no toe)/FILMIC (Hill's ACES fit) tonemaps; MSAA 2x/4x and FXAA for both sets.
  Display rows TONEMAP, BLOOM and ANTI-ALIASING, preferences version 7. The original set
  keeps its frame with anti-aliasing off.
- **Done (second increment).** Half-size GTAO (AMBIENT OCCLUSION [OFF/LOW/HIGH], preferences
  version 8) from a depth copy that survives MSAA, masked away from emission, translucent
  surfaces and view models; under 1 ms at 1080p on Apple M5.
- **Done (third increment).** Half-size screen-space reflections (REFLECTIONS [OFF/LOW/HIGH],
  preferences version 9) that replace the environment light of the enhanced specular instead of
  adding to it; visible on the manifest's glossy classes; about 0.45 ms at 1080p on Apple M5.
- **Done.** A minimum-depth pyramid (Hi-Z) shared by GTAO (far taps from coarser levels, screen
  radius 96 → 256) and SSR (hierarchical traversal instead of 32 linear steps).
- **Remaining.** Post-processing in XR; glossier classes for polished retail floors if wanted.
- **Not verified.** Vulkan execution, Linux and Windows.

The separately requested [display options](contracts/menu_contracts.md#window-frame-pacing-and-headset-display-options)
add fullscreen modes, pixel resolution, VSync, FPS ceilings, headset refresh selection and
50–100% eye render scale (preferences v10). This does not change milestone acceptance.

## M8 — VR on Quest, then PCVR

The [OpenXR plan](implementation/openxr_plan.md) holds the design and the lot
details. The pinned SDL already creates OpenXR sessions and swapchains; multiview
needs a small local Vulkan patch.

| # | Task | Size |
| --- | --- | --- |
| 8.0 | Contract, view-weapon rig audit, world scale | S |
| 8.1 | Build: OpenXR headers and loader, immersive Quest APK, Android data path | S–M — done (`/sdcard/openblood2/{game,demo}`) |
| 8.2 | OpenXR runtime core: session, spaces, swapchains, frame loop, lifecycle | M — done on Quest 3, controllers checked worn |
| 8.3 | Stereo rendering (per-eye passes) and a desktop mock mode | M–L — stereo world done on Quest 3; mock mode pending |
| 8.4 | Multiview (local SDL patch, shader variants) | M |
| 8.5 | Controllers: bindings, aim, locomotion, turning | M — done; second-playtest layout, menu hands and VR options page (turn, laser, crosshair) added; worn check pending |
| 8.6 | HUD locked to the view, menus 0.75 m in front | M — done (compositor quads); worn check pending |
| 8.7 | Selection laser for menus | S — done; worn check pending |
| 8.8 | Data setup screen and in-headset demo download | M — superseded by the cross-platform [setup screen](contracts/setup_screen_contracts.md) (`/sdcard/openblood2`, Java HTTPS); headset check pending |
| 8.9 | View weapons on the controllers, arm IK | L — done; worn check pending |
| 8.10 | Comfort: walls, cinematics, death camera, zoom | M |
| 8.11 | Quest performance tuning and evidence | M |
| 8.12 | PCVR on Linux | M |
| 8.13 | Acceptance record | S |

Main risk: the frame budget on a mobile GPU. The Quest version may need
reduced settings, and PBR/post-processing are likely optional there.

## M9 — LAN multiplayer (authorized, in progress)

The 2026-10-30 [accepted contract](contracts/multiplayer_contracts.md) selects
retail Bloodbath → Team → CTF, a console dedicated server and playable host,
ENet upstream pinned by commit/hash, IPv4, 16 players and UDP 27888. The
[foundation report](implementation/reports/m9_multiplayer.md) records the current
headless build, bounded transport, full-content fingerprint and zero-client
world tests. The [first local replica lot](implementation/reports/m9_client_world.md)
feeds object presentation from `ClientWorld`. The [local movement lot](implementation/reports/m9_local_movement.md)
adds a separate CMoveMgr body, callback-free prediction and immediate server correction
without double integration. Its [collision replica](implementation/reports/m9_client_collision.md)
now owns primary/posed geometry and collider snapshots for independent prediction.
The [environment replica](implementation/reports/m9_client_environment.md) supplies
liquid/ladder/conveyor membership without server callbacks; solo CMoveMgr math and
authoritative air/damage remain unchanged. The
[local camera/HUD lot](implementation/reports/m9_client_view.md) moves view selection,
death/chase rays and HUD state/clock to client ownership, keeping server permissions.
The [local audio lot](implementation/reports/m9_client_audio.md) separates logical server
voices from independent client playback, music, caches and SDL output. Device queue
lead no longer changes authoritative sound completion.
The [local effects presentation lot](implementation/reports/m9_client_effects.md)
feeds every scene-effect consumer, including lights/fog, from owned client snapshots;
the [runtime continuation](implementation/reports/m9_effect_queries.md) now owns the
original pools on the client and makes fragment collision updates independent.
[Owner-bound effects](implementation/reports/m9_effect_owners.md) now update from
world-qualified ClientWorld identities and copied owner values.
[Moving FX services](implementation/reports/m9_effect_services.md) now use client
geometry and typed child outputs. The [flashlight](implementation/reports/m9_flashlight.md)
also uses replica owners/poses and client rays. Immediate creation queries still use
the local adapter; remote effect events remain pending.
[Cosmetic impact audio](implementation/reports/m9_effect_audio.md) now plays only on the client,
without consuming server voices or sound identities.
The [server connection lot](implementation/reports/m9_player_connections.md) now allows
16 checked internal connections and original Bloodbath marker selection.
[Fresh Bloodbath profiles](implementation/reports/m9_player_profiles.md) now use original
skins, entry ammo and private startup announcements.
[ENet admission](implementation/reports/m9_admission.md) now checks protocol, edition,
complete content hash and loaded-world generation before creating an original
Bloodbath player. It owns join/departure/timeout cleanup and bounded heartbeats.
[Initial network world snapshots](implementation/reports/m9_world_baseline.md) now
rebuild object presentation with independently loaded client assets at admission.
[Continuous object membership](implementation/reports/m9_world_membership.md) now
replicates creations, metadata changes and removals after a checked baseline ACK.
[Absolute poses at 20 Hz](implementation/reports/m9_world_state.md) now update
independent client replicas with bounded interpolation and reliable teleport revisions.
[Wire collision/environment](implementation/reports/m9_collision_transport.md) now
reconstructs client-owned geometry and volume properties from the admitted DAT.
[Independent CMoveMgr](implementation/reports/m9_movement_services.md) now runs
against replica collision and prepares original-cadence reports without server linkage.
[Movement transport](implementation/reports/m9_movement_transport.md) now delivers
private setup/report/ACK values and checked server velocity advancement without double integration.
[Client correction](implementation/reports/m9_movement_client.md) now applies private setup/ACKs,
replays bounded CMoveMgr history and drives two independent ENet client processes.
[Private player events](implementation/reports/m9_private_events.md) now reach bounded client inboxes;
HUD/items apply independently and original null-client broadcasts reach every player.
[Network camera state](implementation/reports/m9_camera_transport.md) now drives original replica-based
view selection and server movement/look gates without client-selected permissions.
[Independent scene inputs](implementation/reports/m9_scene_inputs.md) now feed the existing renderer
without GameSession, including client weapon visuals and checked GPU world ownership.
[NetworkClient](implementation/reports/m9_network_client.md) now integrates admission, replication,
CMoveMgr, private HUD and camera under a bounded, server-independent endpoint and mapped clock.
[Graphical connection/host](implementation/reports/m9_network_frontend.md) now displays that endpoint
with the existing renderer/HUD and controls; a host owns one server and a loopback client.
[Independent view weapons](implementation/reports/m9_weapon_services.md) now retain original animation/fire-key
logic behind client services; the synchronous solo bridge remains, with network fire delivery still pending.
[Private weapon loadouts](implementation/reports/m9_weapon_loadout.md) now deliver both hands, mode-specific ammo
and server permissions to independent remote views; draw/rest animations render in dedicated/host sessions.
Firing input, authoritative shot validation and remote audio/FX remain to connect.
[Reliable player commands](implementation/reports/m9_player_commands.md) now connect selection/cycling, USE,
items, alternate zoom and detonation through bounded, generation-checked server dispatch and ACKs.
[Animation-validated fire](implementation/reports/m9_network_fire.md) now transports bounded weapon frames,
runs original animation cadence on the server and checks actual shots independently of prediction.
[Bloodbath life transitions](implementation/reports/m9_network_death.md) now preserve original death/slow-death,
USE rescue/respawn and frag attribution, with reliable weapon cancellation across coalesced life changes.
[Remote damage/door impulses](implementation/reports/m9_network_impulses.md) now reach CMoveMgr reliably,
with impulse-aware corrections, bounded per-peer history and passive dead-player movement.
[Real-process Bloodbath combat](implementation/reports/m9_pvp.md) now verifies damage, frag and USE respawn
on dedicated and graphical-host sessions; the graphical frontend forwards held fire through the same endpoint.
[Remote logical audio](implementation/reports/m9_network_audio.md) now delivers reliable sound lifetimes and independent playback.
[Independent speech](implementation/reports/m9_client_speech.md) now plays private VoiceMgr/CommLink events and startup announcements.
[Weapon animation sounds](implementation/reports/m9_weapon_sounds.md) now run from checked server keys, including retained loops and stops.
The [client roster](implementation/reports/m9_roster.md) now owns reliable names, characters and frag scores, including late arrivals and departures.
[Combat announcements](implementation/reports/m9_combat_announcer.md) now deliver original frag/suicide/FinishHim speech and observer notices.
[Network audio cache reclamation](implementation/reports/m9_audio_cache.md) now retains fixed budgets across sequential playback without evicting live or prepared clips.
[Original Bloodbath scores and player notices](implementation/reports/m9_scoreboard.md) now consume reliable roster/frag updates, preserving tie order, source PCX/FNT layout, death/revival visibility and localized join/leave text.
[Bloodbath match limits and rotation](implementation/reports/m9_rotation.md) now retain peers across cyclic maps, with original intermission, fresh world/life identities and strict stale-packet gates.
[Server Team rules](implementation/reports/m9_team_rules.md) now preserve original assignment, skins/spawns, accumulated team frags and friendly-fire/Voodoo gates; Team admission and client/team-score transport remain to connect.
[Team network play](implementation/reports/m9_team_network.md) now connects assignment/skins, absolute totals, team limits and retained-team rotation, with original team HUD/notices/score columns.
[CTF server rules](implementation/reports/m9_ctf_rules.md) now connect capture contributions, enemy-only OnlyFlagScores, membership-based UseTeamSize and team limits/rotation; independent capture consumers and wire admission remain next.
[Network CTF](implementation/reports/m9_ctf_network.md) now opens dedicated/host admission with independent carried icons, absolute capture scores, localized notices and local grab/return sounds. Server-triggered capture/drop replication and real-process rotation have separate verification scopes.
[Client-routed CTF](implementation/reports/m9_ctf_capture.md) adds a Jam capture by an independently moving client, late admission while carried and capture-driven rotation. Full weapon ACK windows wait within the existing bound; skipped inputs are not banked. Other map routes and human LAN remain unqualified.
[Host/Join and LAN menus](implementation/reports/m9_multiplayer_menu.md) now provide retail Bloodbath/Team/CTF entry from Main/BLOODBATH, map/rule/rotation selection, automatic IPv4 discovery and direct-address fallback. Visual polish and human/cross-platform LAN qualification remain open.
[Network ballistic effects](implementation/reports/m9_ballistic_network.md) now deliver instant recipes to independent client casing/mark/smoke, fragment, liquid and tracer pools with local impact audio. Other FX families and persistent baselines remain pending.
[Network impact sprites, sparks and blood](implementation/reports/m9_impact_network.md) now share one bounded ordered recipe stream, independent child splats/audio and original immediate/deferred updates. Source blood probability and exact missing sound failures stay explicit.
[Network explosions and fragments](implementation/reports/m9_instant_network.md) now deliver six original components, flesh/destruction fragments and instant laser/lightning, with local collisions, child blood/audio and debris detail. Remaining presentation/UI stays pending.
[Network particle streams](implementation/reports/m9_stream_network.md) now carry immutable stream/system recipes with reliable owner membership and late-join baselines. Other owner-bound families are tracked below.
[Original multiplayer trail stopping](implementation/reports/m9_particle_stop.md) preserves the one-second stop/resume grace. [Network ObjectFX](implementation/reports/m9_object_network.md) now delivers multiple owner-bound trails/spells with finite-age late arrivals and retired-publication checks. PolyGrids, WarpGates and lights remain pending.
Communication portraits, music, remaining FX, measured pings/chat and multiplayer branches stay in progress.
**Remaining private presentation consumers/commands, full client separation, other CTF capture routes and complete network qualification remain pending.**

Original model: the authoritative server runs ObjectDLL objects; the client runs
CMoveMgr, rendering and effects over copied object state. The current local solo
path uses owned client replicas with immediate delivery. NetworkClient independently
owns admission, presentation, collision/environment, predicted movement, cameras,
HUD, view weapons and playback; it links without server gameplay objects. ENet
carries reliable membership/private events/commands/audio and sequenced State.
Dedicated and host share ServerSession, authoritative combat and map rotation.
The server-side GameSession still contains transitional local presentation/FX
producers; remaining remote effects and final simulation/library separation are
pending. The initial in-process-only assessment is superseded by these deliveries.

Original references are available. These include the multiplayer branches in
PlayerObj, BloodServerShell, the pickups and B2BaseClass, as well as FlagObjects,
TeamMgr, FragInfo, MessageMgr and NetStart. NetStart is a Win32 dialog wizard
that must be replaced. The later LithTech network layer, about 9k lines, is a
reference only. The source names Co-op, but only Bloodbath, CTF and Team maps
ship, so Co-op is assessed separately.

| # | Task | Size | Depends on |
| --- | --- | --- | --- |
| 9.0 | Accepted contracts and dependency audit; complete the per-reader/message inventory during replica extraction. ENet pin is implemented. | S | — |
| 9.1 | Client replica: presentation (original, PBR and post-processing paths), HUD, audio and inspector read a client world that server snapshots feed. Loopback solo must match current behavior, checked by the GPU readback tests. | XL | 9.0 |
| 9.2 | Wire protocol and transport: versioned, bounded encoding of every typed message; UDP with reliable and unreliable channels; fuzzed decoders | M | 9.0 |
| 9.3 | Sessions: listen and headless dedicated server, LAN discovery, handshake (protocol, retail edition and full-content fingerprint; mods refused), join/leave/timeout, map rotation | M | 9.2 |
| 9.4 | Multi-player server: N players and clients, the original multiplayer branches, respawn, slow death and humiliation, frags, item respawn, game-type filtering, start points, frag and time limits | L | 9.1 |
| 9.5 | Game modes: Bloodbath; CTF with FlagStand, FlagObjects and TeamMgr; Team. The CTF objects, capture rules, world-owned scores and team HUD already run in local inspection ([contracts](contracts/ctf_contracts.md)); network team management and the other modes remain. | M | 9.4 |
| 9.6 | Network movement and fire: the original client-side movement upload with server bounds, remote interpolation, server-side hits, loss and latency | M–L | 9.2, 9.4 |
| 9.7 | Multiplayer UI: host and join screens, name, character and team, FragInfo scoreboard, MessageMgr chat, gamepad navigation | M | 9.3, M7 |
| 9.8 | Validation: headless multi-client loopback on every bb_ and ctf_ map, simulated loss and latency, sanitizers, Windows/Linux/macOS interoperability, a LAN playtest | M | all |

Sizes: S ≈ 1 lot, M ≈ 2–3, L ≈ 4–5, XL ≈ 6–8, where a lot is one
continuation-sized change. Total: about 25–35 lots. Task 9.1 carries most of the
risk because it changes how every renderer path reads state. Direct IPv4 uses a
reachable UDP port; a master server, relay, NAT traversal and host migration are
out of scope. No desktop interoperability or human LAN acceptance is implied by
the Linux foundation tests.

## Later

| Area | Planned work |
| --- | --- |
| Mobile — M10 | Phones/tablets on Android and iOS: touch UI, audio focus, resume, thermal tests. This reuses the M8 Android build. |
| Mods — M11 | Versioned packages, load order/conflicts, replacements/custom levels and save compatibility. Existing REZ overlays are a first subset. |
| In-game editor — M12 | Hierarchy, asset browser, gizmos, undo/redo, edit/play modes, materials/probes and mod export. |
| Rendering extras | Render scaling and FSR1 (baked GI and probes moved to M10). |
| Desktop release | Signed/notarized installers, sustained-play soak tests and measured optimization, continuing from the M3 beta. |
| Shogo | Reassess after Blood II's campaign and engine contracts are established. |
| Dreamcast | Exploratory: a native PowerVR2 scene backend in this repository, after the campaign. Memory (16 MB) is the main risk; see the [study](implementation/dreamcast_study.md). |
| Homebrew consoles | Exploratory, no milestone: Steam Frame, R36S, Switch, PS Vita, PS3, PSP and Dreamcast ranked by cost, with shared lots (scene backend, GLES 3, Cg, big-endian CI); see the [homebrew roadmap](roadmap_homebrew.md). |
