# Blood 2 Reconstruction Roadmap

**M2 is closed with reservation M2-GPU-001**, authorized on 2026-10-18: [acceptance and closure record](../../implementation/reports/m2_acceptance.md). The selected functional session is accepted; the external NVIDIA/D-Bus GPU sanitizer failure remains an open platform qualification issue. M3 is next and is not yet accepted. The chronological entries below retain their original scope.

The [default launcher build](../../implementation/reports/launcher_default_build.md) produces
OpenBlood 2 Launcher beside the game, using an isolated SDL configuration.

The [SDL3 launcher plan](06_launcher.md) now records the user-requested compact
menu, Game Settings, local data-mod toggles and demo installation. The
[implemented continuation](../../implementation/reports/launcher_redesign.md) supersedes
the initial L0-L2 interface; graphical settings were cancelled and Nightmares
remains separate. This does not accept M3.

User-requested presentation continuation: [original statistics HUD](../../implementation/reports/original_hud.md), replacing the demonstration panel using selective original CNewStatusBar code and external artwork. Other original UI and M3 acceptance remain pending.

The [original smell-trail continuation](../../implementation/reports/m2_ai_smell.md) connects the original periodic player trail and shared AI SearchSmellTarget state, with checked hint lifetime and inspection. Original no-target/no-cursor exits and zero NPC smell ranges are preserved; no automatic trail acquisition is invented. Required remaining services and the actual M2 gate stay open.

The [burning death and dismemberment continuation](../../implementation/reports/m2_death_effects.md) connects original lethal fire/gib decisions, burning corpses, postmortem limb loss and visible client flesh fragments, with checked cleanup and in-game inspection. These paths no longer fail solely because fire/gib services were missing. Blood-spurt/splat effects, generic gib/save services and the actual M2 gate remain open. Earlier blanket fire/gib omissions are historical.

The [CityHub entry continuation](../../implementation/reports/m2_cityhub_entry.md) records `fallengrills` as an explicitly omitted object because its named BSP is absent. The source instance is rejected before initial update, matching the available LithTech loader behavior; no replacement geometry is invented. CityHub now has 453 created / 31 sounds / 541 editor-only / one omitted / zero unsupported / zero failed entries. Safe NEW_LEVEL preserves the supported player and enters at `01_egressa`; required SPR world materials now decode and animate through SDL_GPU. M2 remains incomplete; earlier CityHub refusal/count statements are historical.

The [flashlight continuation](../../implementation/reports/m2_flashlight.md) connects original FlashlightPU/CInvFlashlight/CFlashlightFX, L toggle, battery, local audio, inactive restoration and visible radial point illumination with inspection. CityHub now has 453 created / 31 transferred sounds / 541 editor-only / zero unsupported / one failed entry. Missing `fallengrills` geometry still rejects CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical.

The [remote-bomb continuation](../../implementation/reports/m2_remote_bombs.md) connects original RemotesPU, reserved slot11, charged throws, separate detonation and player-owned charge cleanup, with inspection. CityHub now has 452 created / 31 transferred sounds / 541 editor-only / one unsupported / one failed entry. FlashlightPU and missing `fallengrills` geometry still reject CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical.

The [proximity-bomb continuation](../../implementation/reports/m2_proximity.md) connects original ProximitiesPU, reserved inventory slot10, charged throws, sticking/detection/detonation, independent lifetime and inspection. CityHub now has 450 created / 31 transferred sounds / 541 editor-only / three unsupported / one failed entry. Remote bombs, flashlight and missing `fallengrills` geometry still block CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical.

The [original sky continuation](../../implementation/reports/m2_sky.md) connects DemoSkyWorldModel and deferred SkyPointer lookup to checked sky slots, parallax and SDL_GPU world-model background rendering. CityHub now has 448 created / 31 transferred sounds / 541 editor-only / five unsupported / one failed entry; four pointers remove themselves on their first update. Earlier counts below are historical. Missing pickups and `fallengrills` geometry still reject CityHub NEW_LEVEL; M2 remains incomplete.

The [authored particle-stream continuation](../../implementation/reports/m2_particle_streams.md) connects original server toggles and client ramps, current-owner emission and SDL_GPU particle rendering, with inspection. All eight CityHub streams create; the level now has 443 created / 31 transferred sounds / 541 editor-only / 10 unsupported / one failed entry. Original unread sound fields remain metadata. CityHub NEW_LEVEL still refuses; M2 remains incomplete.

The [safe NEW_LEVEL continuation](../../implementation/reports/m2_level_change.md) prepares an isolated destination, restores the supported player and client, then replaces ownership at a frame boundary. Failed preparation retains the current session. Real SubwayA transfers and NVIDIA rendering/audio are tested; CityHub now refuses with 10 unsupported and one failed entry after the particle-stream continuation. M2 remains incomplete; earlier blanket transfer omissions below are historical.

The [Metal shader continuation](../../implementation/reports/m2_metal.md) removes Vulkan-only world entry by bundling SDL_shadercross-generated MSL for world/model/FX rendering. Shader tools remain optional and offline. Linux NVIDIA validation is recorded; the user confirmed macOS execution on 17 October, with native automated coverage still pending. M2 stays incomplete.

The [conversation continuation](../../implementation/reports/m2_conversations.md) connects original ConversationTrigger sequencing, typed client delivery and CCommLink timing/real WAV playback, with inspection. CityHub has 434 created / 31 transferred sounds / 541 editor-only / 19 unsupported / one failed. All seven authored conversation cues in the base-world catalog remain unmapped and missing; separately labelled diagnostics verify actual audio without altering them. Original communication HUD/VoiceMgr arbitration, required services and safe NEW_LEVEL keep M2 incomplete.

The [M16 continuation](../../implementation/reports/m2_assault.md) connects original primary fire, alternate grenade charges, pickup/drop, independent grenade lifetime and inspection. CityHub has 433 created / 31 transferred sounds / 20 unsupported / one failed. Finite inverted ABC key bounds remain visible metadata warnings. The original Cultist lacks rifle animations; direct AI firing is tested separately from unavailable autonomous M16 combat. Replacement grenade physics, required services and safe NEW_LEVEL keep M2 open.

The [compound-shell correction](../../implementation/reports/m2_compound_shells.md) validates independently closed DAT components that share edges. CityHub now creates `startfences` and `Buildings`: 421 created / 31 transferred sounds / 31 unsupported / two failed. Collision gaps, hidden/removal inspection and GPU submission are checked; earlier counts below are historical. Missing geometry, FLAREGUN, required services and safe NEW_LEVEL keep M2 incomplete.

The [Mac-10 continuation](../../implementation/reports/m2_mac10.md) connects original automatic fire, dual inventory, pickup/NPC drop and autonomous Cultist use. Actual left-view assets exercise the restored original alternate-animation fallback. CityHub now has 419 created objects, 31 transferred sounds, 31 unsupported and four failed entries; earlier counts below are historical. Chrome, remaining required services and safe NEW_LEVEL keep M2 incomplete.

The [audio-stream continuation](../../implementation/reports/m2_audio_stream.md) connects real bounded PCM reads from REZ and retains observed VoiceType=0 metadata. CityHub now has 418 created objects, 31 transferred sounds, 32 unsupported and four failed entries; its five SoundFX create, four start initially, and 35 streamed voices share 384 KiB of buffers. Compressed streams, TypeAction schemas, required remaining services and safe NEW_LEVEL keep M2 incomplete.

The [SoundFX continuation](../../implementation/reports/m2_sound_fx.md) connects original nonstreaming ramp/ON/OFF rules to real audio and inspection. The private catalog exposes unresolved historical schemas on all 124 authored emitters (121 VoiceType / three TypeAction). CityHub remains 413 created; its five SoundFX entries are now explicit schema failures (32 unsupported / 40 failed), not successful playback. Streaming, safe NEW_LEVEL and M2 remain incomplete.

The [original destructible-model continuation](../../implementation/reports/m2_destructable_models.md) connects 122 CityHub objects to original damage, model replacement, fading and bounded box physics. CityHub now has 413 created, 37 unsupported and 35 failed entries; earlier counts below are historical. Contact-force sliding audio, required remaining services, safe NEW_LEVEL and M2 remain incomplete.

The [original built-in Model continuation](../../implementation/reports/m2_builtin_models.md) renders 47 authored CityHub lamps with real ABC/DTX resources and inspection. CityHub now has 291 created, 159 unsupported and 35 failed entries; the source session remains intact during rehearsal. Destructible models, safe NEW_LEVEL and M2 remain incomplete.

Continuation: [original fragment smoke trails](../../implementation/reports/m2_object_particles.md) now supplies
distance-based emission, source stop rules and actual DTX particle rendering.
Other required services and safe NEW_LEVEL keep M2 incomplete.

Current continuation: [original explosion metal fragments](../../implementation/reports/m2_explosion_fragments.md) adds
launch, collision/rest, fade, real model rendering and live inspection. Trails,
remaining FX/illumination and safe NEW_LEVEL keep M2 incomplete.

Current continuation: [original explosion particle rings](../../implementation/reports/m2_explosion_rings.md) adds
fire/smoke particle motion, real DTX alpha rendering and per-particle inspection.
Remaining composite FX, illumination and safe NEW_LEVEL keep M2 incomplete.

The [explosion point-light continuation](../../implementation/reports/m2_explosion_lights.md) evaluates original flash/residual light colors, radii, delays and lifetime, independently of emitter/resource lifetime.
Inspection and hidden/reload checks cover all nine SubwayA emitters. Surface
illumination, remaining composite components, safe level transfer and M2 remain open.

The [explosion model continuation](../../implementation/reports/m2_explosion_models.md) adds original ABC effects,
growth, rotation, alpha and independent lifetime to the existing fire/smoke sprites.
The nine SubwayA emitters and GPU model opacity are tested and inspectable. Required
particle/light/fragment services, safe automatic level transfer and M2 remain open.

The [explosion sprite continuation](../../implementation/reports/m2_explosion_sprites.md) connects original fire/smoke
recipes and lifecycle to real alpha-textured rendering and in-game inspection.
Nine SubwayA emitters are exercised through creation, removal and reload. Composite
models/particles/lights, safe automatic level transfer and M2 remain incomplete.

The [sprite presentation continuation](../../implementation/reports/m2_sprite_view.md) renders the 12 original SubwayA
blood splats with decoded alpha and in-game resource/mode diagnostics. Hidden UI,
world depth and unload/reload are tested. This is a prerequisite for client FX;
required FX emitters, safe automatic level transfer and M2 remain incomplete.

The [named keep-alive entry continuation](../../implementation/reports/m2_player_keepalive.md) preserves supported player stats/weapons into an explicitly loaded destination,
discards keys and executes original named start entry. Session exposes its mode,
selected marker and fallback. Automatic level transfer and the M2 gate remain open.

Current continuation: [player engine restoration and client association](../../implementation/reports/m2_player_restore.md)
connects the supported same-world living-player snapshot to original restored entry,
without fresh stats or start-trigger replay. Session inspection supports capture,
leave and restore. Earlier engine/association omissions below are historical;
full client/world state and safe NEW_LEVEL transfer remain unavailable. **M2 stays open.**

Latest restoration continuation: [character node visibility and selected player Load tail](../../implementation/reports/m2_character_nodes.md)
connects original named resets to real rendering/hit state and restores model/flags/
velocity without fresh player grants. Full engine/client restoration and safe level
transfer remain unavailable; **M2 stays open**.

Latest player-preservation continuation: [selected original player fields](../../implementation/reports/m2_player_state.md)
adds owned strings, raw attributes/air/movement values and relative clock rebasing,
plus the original restore-initialization early return. Full player/client/engine
restoration and safe destination entry remain unavailable; **M2 stays open**.

Latest preservation continuation: [original damage Save/Load](../../implementation/reports/m2_damage_state.md)
adds owned health/armor/death state and original death-delay/nigh-invulnerability
rules. Exit inspection now captures inventory and damage separately; full player
restoration, client association and safe destination entry remain pending.
**NEW_LEVEL is still refused and M2 remains incomplete.**

Latest inventory continuation: [original Save/Load subset](../../implementation/reports/m2_inventory_state.md).
Exit requests now capture owned inventory state; original keep-alive restoration
removes keys and recreates weapons. Full player preservation/client association
and safe destination entry remain unavailable, so NEW_LEVEL stays refused and
**M2 stays incomplete**. Earlier blanket inventory-save omissions are superseded
only for this aggregate subset.

Latest continuation: [original ExitTrigger and shell exit requests](../../implementation/reports/m2_exits.md).
SubwayA now has one unported entry (Gideon). NEW_LEVEL still requires unavailable
KeepAliveSave/Load and is explicitly refused while retaining the current player and
inventory. Earlier ExitTrigger omissions below are historical; **M2 remains open**.

Previous continuation: [ordinary NPC death and CCorpse](../../implementation/reports/m2_corpses.md)
connects independent animated corpses and real model-key sounds. Earlier blanket
corpse omissions below are historical. Fire/gibs, corpse damage, required FX,
Gideon and ExitTrigger still keep M2 incomplete.

[Inspector/input update](../../implementation/reports/m2_controls.md): the inspector folds/unfolds and resizes;
keyboard controls start enabled after player entry, with original cinematic gates.
M2 remains incomplete.

Previous death prerequisite: [NPC weapon drops](../../implementation/reports/m2_weapon_drops.md)
connects detached Beretta physics, original pickups and player collection.
The remaining death services and the actual M2 exit gate stay open.

The [NPC hit/recoil continuation](../../implementation/reports/m2_ai_hit.md) adds original hit-region damage and
nonlethal NPC reactions. Lethal corpse/gib behavior remains unavailable;
required services and the M2 exit criteria below are unchanged.

Current continuation: [production Cultist/Civilian lifecycle and AI_Update](../../implementation/reports/m2_ai_runtime.md)
creates SubwayA's ten CultistAI and five CivilianAI instances and connects their update loop.
Required remaining actions/death, FX, Gideon and ExitTrigger still keep M2 incomplete.
Earlier continuation counts/omissions below are historical snapshots.

Latest gameplay continuation: [original AI commands and hearing](../../implementation/reports/m2_ai_triggers.md) connects
trigger commands, two-faction broadcasts and player sound alerts, building on
[weapon actions](../../implementation/reports/m2_ai_fire.md) and [perception](../../implementation/reports/m2_ai_perception.md).
Full NPC initialization/AI_Update, required actions, client FX and ExitTrigger
still block the actual M2 gate.

Latest presentation: [animated models and first-person weapons](../../implementation/reports/m2_model_view.md), building on the textured 3D world. Original view offsets, skin resources, checked attachments and corrected ABC node rotations are now visible. Required NPCs, ExitTrigger, client FX and the unchanged M2 gate remain incomplete. Earlier evidence retains its historical scope.

**Phase 3 roadmap, status updated 18 October 2026. New project: [Blood2](../../../README.md). [M0](../../implementation/reports/m0.md) and [M1](../../implementation/reports/m1.md) are completed on Linux; [M2](../../implementation/reports/m2_acceptance.md) is closed with reservation M2-GPU-001; M3–M8 remain unaccepted.**

Continue the **new, independent Blood2 project**, created from scratch in M0. Integrate original gameplay into its small C++ engine. OpenBlood2, NoMadEngine and nomad are references and optional donors; none is the starting codebase. Selective imports are allowed after review, with provenance and relevant tests. Preserve `sources/blood2/` as the original-code baseline.

**Stack: C++20, SDL3 at the latest commit of `main`, SDL_GPU, spdlog, CMake, Ninja and FetchContent.** Record the resolved SDL commit for repeatable builds; use its OpenXR integration from the outset. Rust remains an option for justified isolated components. Use plain objects and containers: **no EnTT or ECS by default**. Include a small **in-game inspection editor**, using Dear ImGui, to verify the actual runtime state. See the [stack assessment](01_technical_stack.md).

## Basis and Scope

The [inventory](../workspace_readme.md#phase-1-inventory-baseline) identifies the available code and its provenance. The [audit](../Audit/readme.md) establishes that adapted OpenBlood2 game and engine code can compile and link on Linux after a dependency-path workaround. It also reproduces object-lifetime, scheduling, dispatch, and parser defects. It does **not** establish a working original-code gameplay session, retail-asset compatibility, performance, or other-platform support.

The missing complete original Blood 2 engine is the central constraint. Reusing the game source preserves rules, AI, weapons, and interactions, but requires implementing their engine contracts. The later LithTech tree can help explain selected algorithms; it is not a verified replacement engine. These conclusions come from the [contract audit](../Audit/02_original_code_and_contracts.md) and [format audit](../Audit/03_formats_and_animation.md).

The first product target is the **base game's single-player campaign on Windows, Linux, and macOS**. Android and iOS follow on the same architecture. SDL's `main` branch is the selected OpenXR path; full VR gameplay remains a later stage. Shogo, the Nightmare Levels expansion, multiplayer delivery, and legacy DLL/binary compatibility are separate later decisions. Keep original client/server responsibilities while running locally in one process.

## Local Reference Installation

The original game is available at [Blood II](</home/yann/.steam/steam/steamapps/common/Blood II>). A directory listing confirmed `BLOOD2.EXE`, `BLOOD2.REZ`, `BLOOD2P.REZ`, `BLOOD2P2.REZ`, `SOUND.REZ`, `BLOOD2NM.REZ`, `Music/` and cinematic files. This check did not run the game or parse the archives.

M0 configures this external path and verifies archive metadata/readability. Its readme announces version 2.1; original executable behavior remains untested. M2 now parses the four base-game archive indexes and the patched `01_SubwayA` DAT; see [M2 results and limits](../../implementation/reports/m2.md). See the [M0 reference notes](../../implementation/reports/m0.md#local-reference-content) for the initial resource-order policy. Expansion content being present does not expand the base-game scope.

## Reading Order

| Document | Decision or deliverable |
| --- | --- |
| [Technical stack](01_technical_stack.md) | C++ versus Rust, rendering, physics, audio, build tools, portability and dependency policy |
| [Runtime architecture](02_architecture.md) | Simple original-code integration, runtime services and in-game inspection editor |
| [Milestones](03_milestones.md) | Ordered work packages, dependencies, demonstrations and exit gates |
| [Validation and audit traceability](04_validation_and_traceability.md) | Fidelity scenarios, performance measurement, platform evidence and disposition of A01–A20 |

## Delivery Sequence

| Stage | Observable result | Dependency |
| --- | --- | --- |
| M0 — Completed on Linux | New repository/build, SDL main/GPU, inspection shell, content configuration; 13/13 tests in Debug and Release | [Evidence](../../implementation/reports/m0.md) |
| M1 — Completed on Linux | Three original game classes; typed boundary, lifetime/aggregate tests and live object inspection; 24/24 local tests, 22/22 sanitized headless tests | [Evidence](../../implementation/reports/m1.md) |
| M2 — Closed with reservation | Selected original SubwayA-to-CityHub session, strict readers, inspected services and lifetime evidence accepted; external NVIDIA/D-Bus sanitizer issue remains open | [Closure / M2-GPU-001](../../implementation/reports/m2_acceptance.md) |
| M3 — First original-code playable slice | Original movement/combat/interactions; inspector verifies their live state | M2 |
| M4 — Portable rendering and inspection | Same slice and inspection tools on Vulkan, D3D12 and Metal | SDL_GPU starts in M0; final gate requires M3 |
| M5 — Campaign and persistence | Base campaign can be completed, saved, restored and replayed with original game rules | M3; final validation also uses M4 |
| M6 — Desktop release and optimization | Measured, repeatable desktop release with fidelity and performance evidence | M4 + M5 |
| M7 — Mobile delivery | Android and iOS builds validated on named devices | M6; cross-compilation smoke checks start earlier |
| M8 — OpenXR delivery | Headset gameplay with explicit comfort and interaction adaptations | M6; standalone headsets also need Android work from M7 |

The gameplay critical path is **M0 → M1 → M2 → M3**. Start with SDL_GPU directly and grow only the rendering needed for each stage. Existing viewers may serve as external references; their OpenGL renderer and project structure are not imported. Grow inspection coverage alongside every new subsystem so missing or incorrect elements remain visible.

## Decisions and Reconsideration Points

| Recommendation | Why | Evidence that would justify revisiting it |
| --- | --- | --- |
| Start a new project; cherry-pick selectively | Avoid inheriting prototype architecture, workarounds and dependency debt | Evaluate each import on its own; do not turn selective reuse into a wholesale project import |
| Use C++ for engine and original game | It minimizes changes across an inheritance/callback-heavy C++ codebase | A measured, bounded Rust component solves a concrete problem with acceptable build and interface costs |
| Preserve source-level contracts through typed adapters | Static linking alone has left invalid tables, competing definitions and incorrect behavior | A simpler adapter preserves the same tested semantics with fewer game edits |
| Use SDL `main` and SDL_GPU from M0 | Use the selected OpenXR support and one rendering path | Normal integration and device tests guide fixes; no wait for a stable SDL release |
| Keep plain objects and a small in-game editor | Original classes already model gameplay; inspection makes integration errors visible | Add a dependency or abstraction only for a demonstrated need |
| Preserve LithTech movement semantics | Player feel, triggers and AI depend on collision behavior | Comparative tests demonstrate that a replacement solver satisfies the same contract |
| Optimize after measuring a real slice | Current costs are identified structurally, not benchmarked | A boundedness or lifetime defect already requires an immediate fix, regardless of benchmark results |

The new-project direction, selective reuse, SDL `main`, in-game editor and simplicity requirements are user decisions. M0/M1 are accepted for their recorded scope; M2 is closed with the explicit external GPU reservation. M3 will establish the reference-comparison and repeatable interactive-slice evidence; existing partial work is not milestone acceptance.

## Next Implementation Request

Continue **M2 in Blood2/** from the [implemented reader/inspector increment](../../implementation/reports/m2.md). Build on the [original player entry](../../implementation/reports/m2_player.md), [inventory/weapons](../../implementation/reports/m2_weapons.md), [MoveMgr movement](../../implementation/reports/m2_movement.md) and [locomotion](../../implementation/reports/m2_locomotion.md) continuations. The [containers](../../implementation/reports/m2_containers.md) and [specialized volumes](../../implementation/reports/m2_specialized_volumes.md) continuations now connect liquids/ladders and Conveyor/Damage/FreeFall. The [door continuation](../../implementation/reports/m2_doors.md) adds sliding Door and translating world-model pusher/rider collision. The [trigger/key continuation](../../implementation/reports/m2_triggers.md) connects original Trigger/BigTrigger/ToggleTrigger, SelfTrigger, key queries and KeyPickup. The [USE/rotation continuation](../../implementation/reports/m2_interaction.md) now connects original player USE and RotatingDoor with bounded atomic collision. The [brush continuation](../../implementation/reports/m2_brushes.md) connects RotatingBrush/Rotating, selected destructible brushes and damage/death triggers; SubwayA now has 141 created source entries and 67 unported entries. The [explosion continuation](../../implementation/reports/m2_explosions.md) adds original CDebris requests and explosive area damage, including deferred chains; client FX/audio remain pending. The [pushable continuation](../../implementation/reports/m2_pushables.md) connects original Pushable/Gravity flags, mass/friction, brush impulses and player priority with bounded box physics. The [pickup continuation](../../implementation/reports/m2_pickups.md) connects twelve original ammo/health/armor pickups and bounded destruction spawns. Next address inventory items/timed bonuses, remaining spawn classes, unsupported polygon dynamics and session services, then compare broader interactions against retail behavior. The [model/motion continuation](../../implementation/reports/m2_services.md) records the earlier engine subset. Preserve the [M1 contracts and limits](../../implementation/reports/m1.md); a successful world inspection must not be presented as successful game startup. See the [M2 gate](03_milestones.md#m2--connect-world-session-and-gameplay-services).

The [M2 service continuation](../../implementation/reports/m2_services.md) records the
models/motion/class increment. The latest [player entry evidence](../../implementation/reports/m2_player.md) records the connected lifecycle subset and its remaining gameplay blockers. Neither closes the M2 gate.

The latest [M2 weapon continuation](../../implementation/reports/m2_weapons.md) connects original inventory slots/ammo, Beretta/knife animation-key firing and typed ordinary damage, with an in-game Weapons inspector. This is service integration evidence, not completion of M2 or the M3 playable slice.

Earlier continuation: [original scripted cameras](../../implementation/reports/m2_cameras.md) adds CameraObj tracking, typed client view selection and player command gates. SubwayA has 144 created source entries, 7 owned SelfTriggers, 372 editor-only and 64 unported. Cinematic rendering/audio and the unchanged M2 exit gate remain open.

Earlier continuation: [original keyframed paths](../../implementation/reports/m2_keyframers.md) adds KeyFramer/KeyData timelines, camera motion and owned consumed keys. SubwayA has 148 created source entries, 7 SelfTriggers, 372 editor-only and 60 unported; nine Keys are consumed on first update. World/container paths, audio, CinematicTrigger and the unchanged M2 gate remain open.

The [world sound continuation](../../implementation/reports/m2_world_sounds.md) adds original built-in Sound loops that
outlive their consumed source objects, plus inspection and bounded playback checks.
At that increment, SubwayA had 166 live-created entries, 4 world-owned sound entries, 372 editor-only
and 38 unported entries; runtime counts were 173/176 before/after player. Streaming
and historical VoiceType schemas remain unsupported. M2 remains incomplete.

Latest M2 continuation: [original character animations and model nodes](../../implementation/reports/m2_character_animation.md)
connects the original animation table to locomotion and exposes current skeletal
transforms and original sphere measurements. AI/damage-localization consumers,
ExitTrigger, client FX and the actual M2 exit gate remain incomplete.

Latest M2 continuation: [original AI animation-key audio](../../implementation/reports/m2_ai_model_keys.md)
executes 140 sound markers from six real NPC models through the original dispatch
rules. Fire/extra actions and complete NPC behavior remain unavailable; the
17 unsupported SubwayA entries and actual M2 acceptance gate are unchanged.

Latest M2 continuation: [original AI path selection](../../implementation/reports/m2_path_manager.md)
connects PathMgr, owned route data, active enumeration and Session inspection.
SubwayA's seven points select in the original reverse enumeration order. AI
following/action delivery and the unchanged M2 exit gate remain incomplete.

Latest M2 continuation: [original AI visibility](../../implementation/reports/m2_ai_sight.md) connects body/head
ray checks, source volume/glass filtering and current-pose inspection. No NPC
state machine or target acquisition is claimed; the complete M2 gate remains open.

The [held-model/attachment continuation](../../implementation/reports/m2_attachments.md) adds original
CWeapon hand creation/selection/removal, typed current-pose transforms and inspection.
Default players acquire one owned Beretta child on their first inventory update;
source-entry accounting is unchanged. M2 remains incomplete with 17 unsupported
SubwayA entries and required services. Dropped weapons and client visibility SFX
are not implemented.

Latest M2 continuation: [original explicit AI targets and facing](../../implementation/reports/m2_ai_targets.md) adds
named target selection, checked links, stored-position/local-up orientation and
inspection. Target departure/re-entry is exercised without owning the target.
Autonomous perception, NPC states and the actual M2 acceptance gate remain open.

Current visible progress: the [diagnostic 3D viewport](../../implementation/reports/m2_world_view.md)
now renders actual level geometry with original movement and camera selection.
Textures/model presentation and missing original gameplay services remain the next
work; milestone gates are unchanged.

The [animated model continuation](../../implementation/reports/m2_model_view.md) adds real ABC/held/first-person weapon drawing, original view offsets and a corrected legacy node-rotation default. Hidden/missing objects remain inspectable. Required NPCs, transitions and client FX still block M2 completion.

The [AI command/hearing continuation](../../implementation/reports/m2_ai_triggers.md) adds original trigger commands,
two-faction broadcasts and player sound alerts. State selection is distinct from
executing missing NPC actions; production classes, required FX and ExitTrigger
still block the actual M2 gate.

The [original CMovement continuation](../../implementation/reports/m2_ai_navigation.md) adds checked transform caching and
source obstacle-path calculation with inspection. Partial routes remain failures;
no actor following or production NPC registration is implied. Required classes,
AI update/actions and services still keep the M2 gate open.

The [script movement continuation](../../implementation/reports/m2_ai_follow.md) connects original Script_Walk/Run,
FOLLOWPATH and MOVETOOBJECT helpers, owned arrival actions and velocity commands.
Read [contracts](../../contracts/ai_follow_contracts.md). Preserve cached facing timing, initial injury slowdown,
one-point arrival ordering, linked-target lifetime and residual velocity. Failed
partial plans and missing opportunity-fire branches must remain failures. Full
SCRIPT dispatch, production initialization/AI_Update/deactivation, required FX and
ExitTrigger keep M2 incomplete; source-class counts are unchanged.

The [AI script continuation](../../implementation/reports/m2_ai_script.md) connects original command parsing/preparation,
loop/nonloop sequencing, waits, animation, sound and movement state dispatch.
Read [contracts](../../contracts/ai_script_contracts.md). Preserve one-command cadence, source atoi-prefix rules,
PLAYANIMATIONLOOPING fallthrough, double DONE decisions and owned callback lifetime.
Production SCRIPT entry still requires its unported forced AI_Update; diagnostics
must not count as registered NPCs. Required initialization/update/death, client FX
and ExitTrigger/session services keep M2 incomplete. Source counts are unchanged.

The [NPC weapon-drop continuation](../../implementation/reports/m2_weapon_drops.md) connects the original Cabal drop,
independent falling Beretta, original pickup and player collection. Corpse/gib
services, player drop controls and the full M2 exit gate remain open.

The [original explosion flame continuation](../../implementation/reports/m2_explosion_flames.md) adds selected Tesla particle emission and inspection. Required illumination, safe NEW_LEVEL and the observable M2 gate remain open.

The [destination preparation continuation](../../implementation/reports/m2_destination.md) inspects destination DAT geometry, primary collision and property schemas without leaving the current session.
Original callbacks/resources/start triggers are still unverified; this prerequisite does not enable automatic NEW_LEVEL or close M2.

The [destination initialization continuation](../../implementation/reports/m2_destination_trial.md) now executes original destination callbacks and named player entry in a temporary session, destroys it and resumes the retained source.
Earlier callback-validation omissions are superseded for this explicit rehearsal only; destination promotion, required services and safe NEW_LEVEL still keep M2 open.

The [weapon fire-audio continuation](../../implementation/reports/m2_weapon_audio.md) connects original Beretta/knife WAV playback, four retained voices and automatic NPC hearing, with hidden-UI lifetime checks and inspection.
Earlier fire-audio omissions are superseded for these two weapons; reverb, safe NEW_LEVEL and the complete M2 gate remain open.

The [weapon impact continuation](../../implementation/reports/m2_weapon_impacts.md) connects original glass/FireThrough/liquid vector rules, material WAV playback and impact hearing, with ordered contact inspection.
Earlier impact-audio omissions are superseded for Beretta/knife; impact visuals, safe NEW_LEVEL and the actual M2 gate remain open.

Original wood/stone [impact flash/smoke](../../implementation/reports/m2_weapon_sprites.md) now has detached lifecycle and rendering. M2 stays incomplete; next work prioritizes reported mouse look/fire, non-attacking NPCs and train-window material issues over further FX.

The [keyboard/mouse fix](../../implementation/reports/m2_player_controls.md) now supplies interactive fire and relative look through the original paths. Next investigate NPC attacks and train glass; M2 remains incomplete.

[Interactive NPC entry correction](../../implementation/reports/m2_npc_entry.md) prevents pre-player suspension and verifies authored Cultist fire/damage in SubwayA. Train glass is the next playtest priority; required services and M2 remain incomplete.

[Train-window correction](../../implementation/reports/m2_world_alpha.md): the 31 transparent SubwayA faces now bind the original dirtyglass texture and blend its decoded alpha. Hidden-UI/reload/GPU checks cover the shared-depth pass; missing resources remain explicit. Lighting, global FX transparency ordering, required services and M2 remain incomplete.

The [Naga weapon continuation](../../implementation/reports/m2_naga_weapon.md) connects original eye-beam damage, AI fire-key aiming and the real red beam model as a prerequisite for Gideon. Its impact composite, Gideon itself and the actual M2 exit gate remain incomplete.

The [wind weapon dependency](../../implementation/reports/m2_wind_weapon.md) adds original full-radius shockwave damage and real model/audio presentation. Gideon initialization/states/shield and safe NEW_LEVEL remain required; M2 stays open.

The [shield/powerup dependency](../../implementation/reports/m2_shield_weapon.md) adds the original visual-only shield weapon, actual WAV and owner-bound sphere/ring particles with inspection. Armor remains the responsibility of the pending boss action. Gideon, required services and safe NEW_LEVEL still keep M2 incomplete.

The [Gideon fade/teleport continuation](../../implementation/reports/m2_gideon_teleport.md) adds original disappearance/reappearance, checked collision-aware teleport states, stored model color and rendered alpha and inspection. Production boss lifecycle/attack sequencing, required services and safe NEW_LEVEL remain open; M2 is incomplete.

The [Gideon attack continuation](../../implementation/reports/m2_gideon_attacks.md) connects original close/far sequences, one-second dodges, Bold taunt and immediate Special1 recoil, with inspection. Cached transforms, retained lateral velocity and checked failures are preserved. Production boss initialization/health/damage/shared-table ownership, required FX and safe NEW_LEVEL keep M2 incomplete.

The [production Gideon continuation](../../implementation/reports/m2_gideon_runtime.md) adds original initialization, AI_Update, typed boss-health delivery and inspection. The fifth shared table is isolated during destination rehearsals. It supersedes the earlier blanket production-Gideon omission; complete boss encounter/death parity, required remaining services and safe NEW_LEVEL still keep M2 incomplete.

The [NPC activation correction](../../implementation/reports/m2_activation_visibility.md) lets original visibility pings pass destructible glass and completes Gideon's authored SubwayA intro script, with atomic refresh and live query inspection. This replacement visibility policy does not implement retail PVS. Source accounting is unchanged; safe NEW_LEVEL, required services and the actual M2 gate remain open.

The [medkit continuation](../../implementation/reports/m2_medkit.md) adds the original pickup, direct H activation, local sound and preserved charges. It does not close required remaining services or safe NEW_LEVEL; M2 remains incomplete.

The [playtest control/combat correction](../../implementation/reports/m2_playtest_input.md) connects automatic mouse ownership on entry, original Cultist dodge/roll dispatch and bounded alternative-axis corpse placement. The authored first encounter is exercised through SDL input. Required services and safe NEW_LEVEL still keep M2 incomplete.

The [surface panning correction](../../implementation/reports/m2_surface_effects.md) renders SubwayA's authored tunnel/rail motion from session time, with retained metadata and inspection. Original object/collision motion is unchanged; required services and safe NEW_LEVEL keep M2 incomplete.

The [corpse doorway correction](../../implementation/reports/m2_corpse_clearance.md) addresses a reproduced frozen dead Cultist by adding checked source-center separation and swept living-body clearance to initial corpse placement. Original dimensions and bounded failures remain intact; in-game inspection records the extra sweep. This does not certify every later enemy or close M2.

The [continuous train/message-lifetime continuation](../../implementation/reports/m2_message_lifetime.md) replaces retained per-message pointer tokens with checked, non-reused value identities and bounded live payloads. Read [contracts](../../contracts/message_lifetime_contracts.md). Preserve borrowed callback readers, stale/foreign rejection, deferred lifetime and per-adapter inspection history. The spawn-to-CityHub route uses original input/combat/contact/exit paths with no teleport or stat edits; it does not certify retail parity or complete M2.
