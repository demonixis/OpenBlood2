# Implementation Milestones

Current M2 decision: **closed with user-authorized reservation M2-GPU-001 on 2026-10-18**; see the [closure record](../../implementation/reports/m2_acceptance.md). The original criteria and exit-gate text below remain unchanged. The sole accepted exception is the independently reproduced NVIDIA/D-Bus GPU LeakSanitizer failure, which remains an open platform qualification issue. It is not a passing sanitizer result. M3 is next; M3–M8 are not accepted. Historical progress notes below do not override this decision.

Current [door-audio and regression audit](../../implementation/reports/m2_door_audio.md) connects original busy/stop playback and records whole-suite validation separately from the unresolved GPU LeakSanitizer baseline. The continuous train reaches CityHub, but M2 remains incomplete; chronological omissions below are superseded by later evidence.

The [original smell-trail continuation](../../implementation/reports/m2_ai_smell.md) connects the original periodic player trail and shared AI SearchSmellTarget state, with checked hint lifetime and inspection. Original no-target/no-cursor exits and zero NPC smell ranges are preserved; no automatic trail acquisition is invented. Required remaining services and the actual M2 gate stay open.

The [burning death and dismemberment continuation](../../implementation/reports/m2_death_effects.md) connects original lethal fire/gib decisions, burning corpses, postmortem limb loss and visible client flesh fragments, with checked cleanup and in-game inspection. These paths no longer fail solely because fire/gib services were missing. Blood-spurt/splat effects, generic gib/save services and the actual M2 gate remain open. Earlier blanket fire/gib omissions are historical.

The [scripted opportunity-fire continuation](../../implementation/reports/m2_ai_script_fire.md) connects original standing/walking/running fire to FOLLOWPATH and MOVETOOBJECT, with inspection and checked lifetime. Real Cultist model diagnostics verify shots, damage and movement through reloads; these are not authored encounter or traversal proofs. M2 remains incomplete.

The [AI alignment correction](../../implementation/reports/m2_ai_alignment.md) restores the original parallel-axis fallback used by CMovement. Two reproduced CityHub Cultist update failures now become ordinary blocked-path decisions; inspection exposes the resolved axes. A 120-second relocation diagnostic reports no suspended NPCs after the correction, but is not a playthrough. M2 remains incomplete.

The [CityHub entry continuation](../../implementation/reports/m2_cityhub_entry.md) records `fallengrills` as an explicitly omitted object because its named BSP is absent. The source instance is rejected before initial update, matching the available LithTech loader behavior; no replacement geometry is invented. CityHub now has 453 created / 31 sounds / 541 editor-only / one omitted / zero unsupported / zero failed entries. Safe NEW_LEVEL preserves the supported player and enters at `01_egressa`; required SPR world materials now decode and animate through SDL_GPU. M2 remains incomplete; earlier CityHub refusal/count statements are historical.

The [flashlight continuation](../../implementation/reports/m2_flashlight.md) connects original FlashlightPU/CInvFlashlight/CFlashlightFX, L toggle, battery, local audio, inactive restoration and visible radial point illumination with inspection. CityHub now has 453 created / 31 transferred sounds / 541 editor-only / zero unsupported / one failed entry. Missing `fallengrills` geometry still rejects CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical.

The [remote-bomb continuation](../../implementation/reports/m2_remote_bombs.md) connects original RemotesPU, reserved slot11, charged throws, separate detonation and player-owned charge cleanup, with inspection. CityHub now has 452 created / 31 transferred sounds / 541 editor-only / one unsupported / one failed entry. FlashlightPU and missing `fallengrills` geometry still reject CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical.

The [proximity-bomb continuation](../../implementation/reports/m2_proximity.md) connects original ProximitiesPU, reserved inventory slot10, charged throws, sticking/detection/detonation, independent lifetime and inspection. CityHub now has 450 created / 31 transferred sounds / 541 editor-only / three unsupported / one failed entry. Remote bombs, flashlight and missing `fallengrills` geometry still block CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical.

The [original sky continuation](../../implementation/reports/m2_sky.md) connects DemoSkyWorldModel and deferred SkyPointer lookup to checked sky slots, parallax and SDL_GPU world-model background rendering. CityHub now has 448 created / 31 transferred sounds / 541 editor-only / five unsupported / one failed entry; four pointers remove themselves on their first update. Earlier counts below are historical. Missing pickups and `fallengrills` geometry still reject CityHub NEW_LEVEL; M2 remains incomplete.

The [authored particle-stream continuation](../../implementation/reports/m2_particle_streams.md) connects original server toggles and client ramps, current-owner emission and SDL_GPU particle rendering, with inspection. All eight CityHub streams create; the level now has 443 created / 31 transferred sounds / 541 editor-only / 10 unsupported / one failed entry. Original unread sound fields remain metadata. CityHub NEW_LEVEL still refuses; M2 remains incomplete.

The [safe NEW_LEVEL continuation](../../implementation/reports/m2_level_change.md) prepares an isolated destination, restores the supported player and client, then replaces ownership at a frame boundary. Failed preparation retains the current session. Real SubwayA transfers and NVIDIA rendering/audio are tested; CityHub now refuses with 10 unsupported and one failed entry after the particle-stream continuation. M2 remains incomplete; earlier blanket transfer omissions below are historical.

The [conversation continuation](../../implementation/reports/m2_conversations.md) connects original ConversationTrigger sequencing, typed client delivery and CCommLink timing/real WAV playback, with inspection. CityHub has 434 created / 31 transferred sounds / 541 editor-only / 19 unsupported / one failed. All seven authored conversation cues in the base-world catalog remain unmapped and missing; separately labelled diagnostics verify actual audio without altering them. Original communication HUD/VoiceMgr arbitration, required services and safe NEW_LEVEL keep M2 incomplete.

The [M16 continuation](../../implementation/reports/m2_assault.md) connects original primary fire, alternate grenade charges, pickup/drop, independent grenade lifetime and inspection. CityHub has 433 created / 31 transferred sounds / 20 unsupported / one failed. Finite inverted ABC key bounds remain visible metadata warnings. The original Cultist lacks rifle animations; direct AI firing is tested separately from unavailable autonomous M16 combat. Replacement grenade physics, required services and safe NEW_LEVEL keep M2 open.

The [world-model debris continuation](../../implementation/reports/m2_world_debris.md) connects original named-fragment activation, gravity, rotation and independent lifetime, with rendered geometry and inspection. CityHub now has 432 created / 31 transferred sounds / 21 unsupported / one failed entry. Conservative moving bounds are replacement physics; missing geometry, required services and safe NEW_LEVEL keep M2 incomplete.

The [Flare Gun continuation](../../implementation/reports/m2_flares.md) connects original primary sticking/burn damage, alternate fragments, both player hands, pickup/drop and autonomous Cultist fire. CityHub now has 422 created / 31 transferred sounds / 31 unsupported / one failed entry; earlier counts below are historical. Missing geometry, required services and safe NEW_LEVEL keep M2 incomplete.

The [compound-shell correction](../../implementation/reports/m2_compound_shells.md) validates independently closed DAT components that share edges. CityHub now creates `startfences` and `Buildings`: 421 created / 31 transferred sounds / 31 unsupported / two failed. Collision gaps, hidden/removal inspection and GPU submission are checked; earlier counts below are historical. Missing geometry, FLAREGUN, required services and safe NEW_LEVEL keep M2 incomplete.

The [Mac-10 continuation](../../implementation/reports/m2_mac10.md) connects original automatic fire, dual inventory, pickup/NPC drop and autonomous Cultist use. Actual left-view assets exercise the restored original alternate-animation fallback. CityHub now has 419 created objects, 31 transferred sounds, 31 unsupported and four failed entries; earlier counts below are historical. Chrome, remaining required services and safe NEW_LEVEL keep M2 incomplete.

The [original destructible-model continuation](../../implementation/reports/m2_destructable_models.md) connects 122 CityHub objects to original damage, model replacement, fading and bounded box physics. CityHub now has 413 created, 37 unsupported and 35 failed entries; earlier counts below are historical. Contact-force sliding audio, required remaining services, safe NEW_LEVEL and M2 remain incomplete.

The [original built-in Model continuation](../../implementation/reports/m2_builtin_models.md) renders 47 authored CityHub lamps with real ABC/DTX resources and inspection. CityHub now has 291 created, 159 unsupported and 35 failed entries; the source session remains intact during rehearsal. Destructible models, safe NEW_LEVEL and M2 remain incomplete.

Current continuation: [player engine restoration and client association](../../implementation/reports/m2_player_restore.md)
connects the supported same-world living-player snapshot to original restored entry,
without fresh stats or start-trigger replay. Session inspection supports capture,
leave and restore. Earlier engine/association omissions below are historical;
full client/world state and safe NEW_LEVEL transfer remain unavailable. **M2 stays open.**

Latest continuation: [ordinary NPC death and CCorpse](../../implementation/reports/m2_corpses.md)
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

**M0 and M1 are completed for their documented Linux scope; M2 is closed with reservation M2-GPU-001; M3–M8 remain unaccepted.** The audit supplies requirements, not completion credit. Each stage ends with a reproducible record, demonstrated behavior and explicit limitations.

Create a new project and repository. Keep the six existing repositories as references/donors and preserve original sources. Cherry-pick or import only components needed by the current milestone, after reviewing their behavior and dependencies. Use a few targets and plain objects; do not add EnTT or a general ECS.

## M0 — Create the New Project

**Status: completed, 15 October 2026.** [Blood2/](../../../README.md) is the independent implementation. Debug and inspector-free Release each pass 13 tests, including a real Vulkan window and the configured installation. Six configuration/content cases also pass Clang ASan/UBSan. See [results, capture and platform limits](../../implementation/reports/m0.md). The original game has not been run or loaded; those are later gates. The specification below is retained for traceability.

**Goal:** a clean, reproducible application foundation with SDL_GPU and the in-game inspection UI.

Work:

1. Create an independent directory/repository with minimal CMake/Ninja targets and spdlog. Use the [three-target layout](02_architecture.md#module-boundaries) as a starting point. Do not copy a prototype's build system or dependency tree.
2. Resolve the latest SDL `main` commit and record its SHA through FetchContent. Create an SDL_GPU window with a simple draw and clean shutdown. Keep VR optional; use SDL's main-branch OpenXR integration when wiring headset presentation.
3. Add Dear ImGui with the SDL3/SDL_GPU backends. Provide a toggleable inspection panel showing runtime/dependency status and an initially empty object/resource list. No ImGuizmo, ECS or separate editor application is needed.
4. Set up CTest and meaningful startup/configuration smoke checks. Use the [audit probes](../Audit/probes/readme.md) as specifications for M1/M2 regression cases, adapting them to new services when those services exist. Do not make building or repairing the old prototypes a prerequisite.
5. Configure the external content root to [the supplied Blood II installation](readme.md#local-reference-installation). Identify edition/patch and archive precedence, then select a small scenario with one enemy, weapon, trigger/door and pickup for M3. Record reference-game launch/observation results separately from file availability.
6. Start a concise import/adaptation ledger and platform evidence matrix. Record donor commits only for actual imports. Select representative desktop hardware or mark device-dependent checks pending.

**Deliverables:** new project, build instructions/presets, dependency SHAs, SDL_GPU application, inspector shell, smoke tests and reference scenario.

**Exit gate:** a fresh Linux build starts, draws, opens/closes the inspection UI and exits cleanly without a headset. Tests are discovered and pass for implemented behavior; missing content paths produce a clear error. The project has no build dependency on a previous prototype, no EnTT and no inherited OpenGL renderer. Add Windows/macOS build procedures or jobs, with unavailable runners marked pending.

**Depends on:** inventory/audit as reference. **Prevents or tracks:** A09–A11, A16, A17, A20. Old build defects stay documented in the audit; the new project does not inherit the obligation to repair them.

## M1 — Make the Original-Code Boundary Trustworthy

**Status: completed for the selected boundary on Linux, 15 October 2026.** Actual original `B2BaseClass`, `CameraSpot` and `Key` code is linked, with inherited/aggregate dispatch, typed services, safe lifetimes and live inspection. GCC Debug and inspector-free Release pass 24/24 local tests; Clang ASan/UBSan passes 22/22 headless tests. See [M1 results, source subset and platform limits](../../implementation/reports/m1.md). Original client/server shells and world loading remain M2. The specification below is retained for traceability.

**Goal:** original-style objects execute through a defined interface without invalid dispatch or ownership.

Work:

1. Import the original game code needed by this stage and introduce the [typed game/adapter boundary](02_architecture.md#one-typed-game-boundary), using compiler-checked definitions and implementations.
2. Keep one authoritative `BaseClass`/interface definition and explicit class registration. Resolve original client/server symbol collisions in source instead of importing manual tables or duplicate-symbol suppression.
3. Implement correct aggregate forwarding, precreate ordering, creation reasons, nested property contexts, runtime property strings, scheduling and deferred removal. Turn the audit's failure cases into focused tests of the new implementation.
4. Establish handle ownership, pending-destruction state, string/message lifetimes, and defined unsupported-service errors. Add foundational named-object/list/link behavior needed to exercise lifecycle contracts.
5. Review imported adaptations against `sources/blood2/` and record only necessary changes. Expose object identity, properties, lifecycle and schedules to the inspector.

**Deliverables:** a headless executable linking original game code through the typed boundary, service contracts and focused regression cases. Exercise at least one real original object/aggregate path as well as synthetic edge cases.

**Exit gate:** cases derived from A01–A05 pass in the new runtime; nested spawn, self/other removal, zero scheduling and inherited dispatch execute correctly. Sanitizers report no defect in the exercised path. Inspect a live test object's state and verify that destroying it clears the editor selection safely. Compile the boundary under GCC/Clang and MSVC/Apple Clang when runners are available; missing evidence stays explicit.

**Depends on:** M0. **Addresses:** A01–A05, A07 foundation, A09, A17. If this gate fails, keep work on the smallest failing boundary instead of bypassing original callbacks to unlock a visual demo.

## M2 — Connect World, Session and Gameplay Services

**Status: in progress.** [M2 implementation evidence](../../implementation/reports/m2.md) records REZ/DAT readers, original object creation and world inspection. The [service continuation](../../implementation/reports/m2_services.md) adds ABC animation/deformation, primary-world collision and original WorldProperties/SmellHint. The [player continuation](../../implementation/reports/m2_player.md) connects selected shell/CPlayerObj entry and lifetime branches. The [weapon continuation](../../implementation/reports/m2_weapons.md) adds inventory, Beretta/knife animation-driven fire and ordinary damage. The [movement continuation](../../implementation/reports/m2_movement.md) adds original MoveMgr dry-land walking/running/jumping with typed local movement and ground inspection. The [locomotion continuation](../../implementation/reports/m2_locomotion.md) adds original animation selection, crouch clearance and a bounded replacement stair solver. The [container continuation](../../implementation/reports/m2_containers.md) adds original VolumeBrush/liquid/ladder code, real volume queries, swimming/climbing and air/damage inspection. The [specialized volume continuation](../../implementation/reports/m2_specialized_volumes.md) adds Conveyor, Damage, FreeFall and initial volume-damage protection. The [door continuation](../../implementation/reports/m2_doors.md) adds original sliding Door, translating world-model collision and atomic pusher/rider movement. The [trigger/key continuation](../../implementation/reports/m2_triggers.md) adds Trigger/BigTrigger/ToggleTrigger, owned door SelfTriggers, key inventory/query and single-player KeyPickup. The [USE/rotation continuation](../../implementation/reports/m2_interaction.md) adds original player USE and RotatingDoor with bounded atomic collision and inspection. The [brush continuation](../../implementation/reports/m2_brushes.md) adds original RotatingBrush and selected CDestructableBrush damage/removal, with 141 created source entries and 67 unported entries in SubwayA. The [explosion continuation](../../implementation/reports/m2_explosions.md) adds original CDebris requests and explosive brush area damage. The [pushable continuation](../../implementation/reports/m2_pushables.md) adds BoxPhysics dynamics, original brush impulses and player priority. The [pickup continuation](../../implementation/reports/m2_pickups.md) adds eight ammo and four health/armor classes with original collection rules and destruction SpawnObject. The [objective continuation](../../implementation/reports/m2_objectives.md) adds original mission objectives, typed client delivery and external localized strings (142 created source entries, 66 unported in SubwayA). The [camera continuation](../../implementation/reports/m2_cameras.md) adds original CameraObj tracking/activation, typed client view selection and player command gates (144 created source entries, 64 unported in SubwayA); cinematic presentation/audio remain pending. The [keyframe continuation](../../implementation/reports/m2_keyframers.md) adds original KeyFramer/KeyData camera paths and consumed-key ownership (148 created source entries, 60 unported in SubwayA); world/container path motion and key audio were unavailable at that snapshot. The [dynamic light continuation](../../implementation/reports/m2_lights.md) adds original ClientLightFX/CLightFX waveform state and twelve keyframed GlowingLight targets (160 created source entries, 48 unported in SubwayA); world illumination/shadows/fog remain unavailable. The [train path continuation](../../implementation/reports/m2_worldpaths.md) now connects translating static BoxPhysics brushes with atomic primary-world/pusher/rider collision; angular paths and moving containers remain unavailable. The [cinematic continuation](../../implementation/reports/m2_cinematics.md) adds original CinematicTrigger, real bounded WAV voices and dry SDL playback (164 created / 44 unported in SubwayA); reverb and general sound emitters were unavailable at that snapshot. The [random sound continuation](../../implementation/reports/m2_random_sounds.md) adds original RandomSoundFX and typed fixed-position ambient/spatial voices (166 created / 42 unported in SubwayA); built-in Sound loops/streaming, reverb and other sound callers were unavailable at that snapshot. The [world sound continuation](../../implementation/reports/m2_world_sounds.md) adds four original Sound sources transferred to world-owned loops (166 live-created / 4 transferred / 38 unported in SubwayA); streaming and region loops remain unavailable. The [sprite continuation](../../implementation/reports/m2_sprites.md) adds original DetailSprite placement/repeats and SPR frame clocks (178 live-created / 4 transferred / 26 unported in SubwayA); DTX decoding and world sprite rendering remain unavailable. The [scene explosion continuation](../../implementation/reports/m2_scene_explosions.md) adds original CClientExplosionSFX server activation/removal, preserves its null-damager suppression and connects detached WAV voices (187 live-created / 4 transferred / 17 unported in SubwayA). Client explosion FX remain unavailable. Complete player gameplay, other movers, polygon dynamics/stacks, remaining spawn classes and pickups, WorldModelDebris/client FX/audio, crane controls/minefields and other required classes remain missing. The exit gate below is unchanged.

The [explosion sprite continuation](../../implementation/reports/m2_explosion_sprites.md)
connects selected original fire/smoke components and real opacity rendering. This
supersedes the blanket scene-explosion FX omission only for those sprites; remaining
composite effects and safe automatic NEW_LEVEL still block the unchanged M2 gate.

The [explosion model continuation](../../implementation/reports/m2_explosion_models.md)
adds original model growth/rotation/fade and real alpha rendering. Model components
supersede the previous omission only for the selected high-detail recipes; missing
composite services and safe NEW_LEVEL still block the same M2 acceptance criteria.

The [explosion point-light continuation](../../implementation/reports/m2_explosion_lights.md)
adds original transient light evaluation and inspection. Surface illumination and
remaining composite services stay explicit; the M2 gate is unchanged.

The [explosion ring continuation](../../implementation/reports/m2_explosion_rings.md)
adds original fire/smoke ring rules, bounded particle motion and real alpha billboards.
This replaces the earlier ring omission only for the selected recipes; other composite
FX, illumination and safe NEW_LEVEL keep the M2 gate unchanged.

The [explosion fragment continuation](../../implementation/reports/m2_explosion_fragments.md)
adds original grenade fragment launch, bounce/rest, fade and actual metal geometry.
Trails and other required services remain unavailable; the M2 exit gate is unchanged.

The [fragment smoke-trail continuation](../../implementation/reports/m2_object_particles.md)
adds original distance-based smoke emission and checked child lifetime. Earlier trail
omissions are historical for this selected recipe; the M2 exit gate is unchanged.

The [weapon fire-audio continuation](../../implementation/reports/m2_weapon_audio.md) connects
original Beretta/knife playback, retained voice cleanup and automatic gunfire hearing.
This closes one selected service gap; required world services and safe NEW_LEVEL
still keep the M2 exit gate open.

The [weapon impact continuation](../../implementation/reports/m2_weapon_impacts.md) adds original
vector material rules, selected impact audio and hearing. Required remaining services
and safe NEW_LEVEL still keep the M2 acceptance criteria unmet.

The [weapon impact sprite continuation](../../implementation/reports/m2_weapon_sprites.md) adds
original wood/stone flashes and smoke, with detached lifetime and shared-depth
presentation. Other FX and the actual M2 exit gate remain incomplete.

**Playtest priority update (2026-10-17):** [keyboard fire and mouse controls](../../implementation/reports/m2_player_controls.md)
now feed the original weapon/camera path. The [NPC entry correction](../../implementation/reports/m2_npc_entry.md)
prevents demonstrated pre-player suspension and verifies autonomous fire/damage in
an authored SubwayA encounter. The [train-window correction](../../implementation/reports/m2_world_alpha.md)
now replaces the pink placeholder with actual DTX alpha on all 31 transparent faces.
Broader interactive combat, global translucent ordering and required services remain
unverified/incomplete; these checks do not relax M2's gate.

The [Naga weapon dependency](../../implementation/reports/m2_naga_weapon.md) now connects original eye-beam damage and the red beam model. Gideon, its remaining weapons and eye-impact composite are still unavailable; this does not change the M2 exit gate.

The [energy projectile dependency](../../implementation/reports/m2_energy_projectiles.md) now connects the original weapon, non-Zealot projectile callbacks and delayed damage. The [wind dependency](../../implementation/reports/m2_wind_weapon.md) additionally supplies original shockwave damage and model/audio presentation. The [shield/powerup dependency](../../implementation/reports/m2_shield_weapon.md) supplies the original visual-only weapon and owner-bound particles. The [combat-method continuation](../../implementation/reports/m2_gideon_combat.md) now connects his decisions, armor and model-key firing. The [fade/teleport continuation](../../implementation/reports/m2_gideon_teleport.md) adds the selected original disappearance/movement states and rendered model alpha. The [attack/dodge continuation](../../implementation/reports/m2_gideon_attacks.md) connects the original close/far sequences and recoil. Production Gideon initialization/health/damage/shared-table ownership, required FX and safe NEW_LEVEL still keep the unchanged M2 gate open.

The [SoundFX continuation](../../implementation/reports/m2_sound_fx.md) supplies original nonstreaming controller rules with real audio. All 124 authored controllers have unresolved historical schemas (121 VoiceType / three TypeAction); CityHub's five remain failed entries. FileStream/TIMESYNC and safe world replacement remain explicit services to resolve, not completed acceptance.

The [PCM stream continuation](../../implementation/reports/m2_audio_stream.md) now plays CityHub's 31 transferred sounds and four initially active SoundFX with bounded archive reads. Zero-valued VoiceType metadata is retained without invented semantics; TypeAction and compressed streams remain unsupported. Safe world replacement and the M2 exit gate are unchanged.

**Goal:** the selected content can enter a real local original-code session and use engine services.

Work:

1. Harden readers before broad content loading: bounded reads, cycle detection, count checks, recursion limits, explicit format variants and strict failure. Add malformed synthetic fixtures and differential checks against donor readers where useful.
2. Load world geometry, object properties and models through the shared resource system. Verify units, axes, scale and world-model transforms with controlled geometry plus the selected private content.
3. Connect server world entry/update/teardown and client startup/player association. Route commands/messages through the original paths and instantiate the selected level's required classes.
4. Implement the slice's collision/movement queries, property lookup, named targeting, lists, links and attachments. Verify the service path actually reaches the implementation rather than a remaining facade stub.
5. Connect animation index/state/playback and model-string events to original callers, independent of render visibility. Define a shared time policy and precise event ordering.
6. Produce a trace of every engine service reached by the slice. Separate implemented, explicitly unsupported and unexercised entries. Required calls cannot be satisfied by success-shaped placeholders.
7. Extend the editor to world/object selection, collision/trigger overlays, model/skeleton/attachment state and resource references. Show unresolved or rejected source entries alongside successfully loaded elements.

**Deliverables:** a local session harness, strict loaders, supported-class/service matrix and inspectable world. Implement only the SDL_GPU drawing needed to see and diagnose these systems.

**Exit gate:** the selected world enters/exits through original shells; required objects have correct properties; contacts/animation callbacks reach original code. Malformed resources fail within bounded time/memory. Sanitizers pass tested session paths. The editor can account for loaded and missing elements and show placement/collision/attachment state. Required unsupported calls keep this gate open.

The [inventory preservation prerequisite](../../implementation/reports/m2_inventory_state.md)
now captures/restores supported original inventory data with source keep-alive key
removal. This does not satisfy the shell exit gate: player-owned fields, restored
client association and failure-safe destination entry still block NEW_LEVEL.

The [damage preservation prerequisite](../../implementation/reports/m2_damage_state.md) adds
original health/armor/death aggregate state. Full player/timer/engine-object state,
client association and safe destination entry still block the shell exit gate.

The [player field prerequisite](../../implementation/reports/m2_player_state.md) preserves
selected original values and restore initialization without fresh-game grants.
Remaining player/client/engine state and safe destination entry still block transfer.

The [model restoration prerequisite](../../implementation/reports/m2_character_nodes.md) adds
original node resets, real hidden geometry/hit state and selected Load model/flags/
velocity setup. It does not supply engine checkpoints, client association or safe
world replacement; the observable M2 exit gate remains unchanged.

**Depends on:** M1. **Addresses:** A06, A07, A12–A14, A17, A19; applies the A20 generation boundary.

Latest prerequisite evidence: [character animation/node continuation](../../implementation/reports/m2_character_animation.md)
connects original CAnim_Sound tables, current node queries and hit-sphere measurements.
This does not register the missing AI or supply damage localization/attachments;
M2 remains incomplete with unchanged source-entry counts and exit criteria.

Latest prerequisite: [character motion keys](../../implementation/reports/m2_character_motion.md)
adds the original base-character motion branch and bounded parameters. The private
character corpus contains no motion markers; synthetic validation is labelled.
No AI class is added and the M2 acceptance criteria remain unmet.

Latest prerequisite: [character audio and footsteps](../../implementation/reports/m2_character_audio.md)
connect original voice/player key paths and surface/liquid selection. AI hearing,
remaining source classes and required services keep the M2 exit gate unchanged.

The [AI model-key continuation](../../implementation/reports/m2_ai_model_keys.md) integrates
selected AI_Mgr sound dispatch, source camera/script/random rules and shared voices.
Actual sound-key playback and diagnostic probes do not satisfy missing AI classes.
M2 remains incomplete with 17 unsupported SubwayA entries and required services.

The [AI path-manager continuation](../../implementation/reports/m2_path_manager.md) connects
original snapshot/selection rules and active enumeration. Owned copies and GPU
inspection pass independently of AI following, which remains unported. The same
17 missing source instances and required services keep M2 incomplete.

The [AI visibility continuation](../../implementation/reports/m2_ai_sight.md) connects original
CanSee body/head rays and source object filtering. Real model poses and hidden/reload
queries are inspected; target acquisition and NPC states remain unavailable.
M2 counts and acceptance criteria remain unchanged.

The [held-model/attachment continuation](../../implementation/reports/m2_attachments.md) adds
original selected weapon children and typed visual parent/node transforms. This
supplies observable attachment placement, ownership and hidden/reload inspection;
physical/nested attachments, dropped weapons and client visibility SFX remain open.
The unchanged source-class counts and required services still block M2 completion.

The [explicit AI target/facing continuation](../../implementation/reports/m2_ai_targets.md)
connects original named selection, checked target links, deferred loss callbacks
and local-up facing. These commands do not implement autonomous target acquisition
or NPC states. The 17 unsupported source entries and M2 exit gate remain unchanged.

The [diagnostic 3D continuation](../../implementation/reports/m2_world_view.md) now provides
the M2 drawing path for real level volumes, moving world models and the original
player/cameras. It enables navigation and placement inspection without waiting for
the complete M4 renderer. The [DTX continuation](../../implementation/reports/m2_textures.md) supplies opaque base textures.
The [animated model continuation](../../implementation/reports/m2_model_view.md) also renders existing ABC/held/view models and corrects legacy node rotations. Required NPCs, FX and transitions still keep M2
open. The next M2 increments should return to required original NPC acquisition/states,
client FX and exit/session services; richer lighting/material work belongs to M4.
A textured navigable level does not satisfy M3's combat/scenario acceptance.

The [AI perception continuation](../../implementation/reports/m2_ai_perception.md) supplies
original candidate/faction lists and stimulus/target selection, with real player
membership and ExitHint. It does not register partial NPCs as successful world
objects. Next: original AI update/state/actions and their necessary services, then
required client FX and exit/session behavior. The same M2 acceptance gate applies.

The [AI state/action continuation](../../implementation/reports/m2_ai_states.md) now supplies
original transition and decision rules plus selected stationary actions. Production
NPCs stay unsupported until initialization, the complete update loop and required
movement/combat/death paths execute. Diagnostic dispatch does not satisfy that gate.

The [AI weapon continuation](../../implementation/reports/m2_ai_fire.md) connects supported
weapon selection, original standing fire, aiming/clearance and marker-driven shots.
Required production initialization/AI_Update, attack-state sequencing, navigation,
death, client FX and exit/session behavior still keep this gate open.

The [AI command/hearing continuation](../../implementation/reports/m2_ai_triggers.md) adds original trigger commands,
two-faction broadcasts and player sound alerts. State selection is distinct from
executing missing NPC actions; production classes, required FX and ExitTrigger
still block the actual M2 gate.

The [script movement continuation](../../implementation/reports/m2_ai_follow.md) connects
original WALK/RUN, FOLLOWPATH and MOVETOOBJECT helpers with collision integration,
owned arrival actions and inspection. Full script dispatch and production NPC
initialization/update/deactivation/death remain unavailable; required FX and
ExitTrigger still block M2. Diagnostic helper success does not satisfy the gate.

The [script-state continuation](../../implementation/reports/m2_ai_script.md) connects the
original instruction list, loop/wait/animation/sound sequencing and movement helpers.
The production SCRIPT trigger remains gated on its full forced AI_Update. Next
integration must supply real NPC initialization/update/state execution, plus the
required FX and exit/session services; script diagnostics do not close M2.

The [autonomous locomotion continuation](../../implementation/reports/m2_ai_locomotion.md)
connects original movement/obstruction/ledge rules and Cultist close/far attacks.
Shade-dependent recovery, remaining navigation/death states, full initialization
and AI_Update/deactivation are still required; diagnostic attacks do not close M2.

The [AI recovery/navigation continuation](../../implementation/reports/m2_ai_recovery.md)
connects original jump/crouch/crawl and selected destination/search states. Full
NPC initialization/update, shade-dependent detours, remaining escape/recoil/death,
required FX and exit/session services still block M2. No acceptance criteria change.

The [AI escape/guard continuation](../../implementation/reports/m2_ai_escape.md) adds original
Cultist/Civilian flee/hide and guard/beg behavior with checked tracking. Full NPC
initialization/update, remaining states, required FX and exit/session services still
block M2. Diagnostic scenarios do not satisfy authored-game acceptance criteria.

The [NPC activation correction](../../implementation/reports/m2_activation_visibility.md) lets original visibility pings pass destructible glass and completes Gideon's authored SubwayA intro script, with atomic refresh and live query inspection. This replacement visibility policy does not implement retail PVS. Source accounting is unchanged; safe NEW_LEVEL, required services and the actual M2 gate remain open.

The [original keyframe-audio continuation](../../implementation/reports/m2_keyframe_audio.md) connects sound-bearing keys to retained attached playback with original replacement/lifetime rules. Train Station diagnostic activation exercises authored timing; three unresolved catalog references, remaining required services and consolidated acceptance keep M2 incomplete.

The [object snapshot optimization](../../implementation/reports/m2_object_snapshots.md) reduces repeated occupied-slot scans while retaining owned copies, callback membership, generations and slot order. The original AI clearance fixture and its sanitizer deadline remain unchanged; M2 stays incomplete.

The [music instrument routing continuation](../../implementation/reports/m2_music_routing.md) connects selected-band MIDI snapshots to checked DLS patch/key lookup and controller-aware diagnostic synthesis. Original download/residency, live scheduling and playlists remain unfinished; MUSIC still keeps M2 open.

The [connected grid-note continuation](../../implementation/reports/m2_music_grid_notes.md) joins admitted notes to normal-mode harmonic conversion and selected-band event preparation, with original executable comparisons. Paired events are not live transport; active-note scheduling, curves and playlists still keep MUSIC and M2 incomplete.

The [active-note continuation](../../implementation/reports/m2_music_active_notes.md) connects
original chord-dependent deadlines, serial exemptions and due/pending order, with
executable comparison and inspection. Terminal tails remain pending; live clocks,
section events/curves, controller delivery and playlists still keep MUSIC and the
actual M2 gate incomplete.

The [style-curve continuation](../../implementation/reports/m2_music_curves.md) decodes the original installed curve records with executable-reader comparison and independent inspection. Curve evaluation, controller delivery and transport remain unfinished; MUSIC and the actual M2 gate stay open.

The [style-curve event continuation](../../implementation/reports/m2_music_curve_events.md) prepares original point events with explicit owner/tick inputs and executable comparison. Admission, merged note order, controller delivery and live MUSIC still keep M2 incomplete.

The [ordered grid dispatch continuation](../../implementation/reports/m2_music_grid_dispatch.md) connects original curve admission and note/curve preparation order with executable comparison. Live controller delivery and transport remain required; MUSIC and M2 stay incomplete.

The [live voice control continuation](../../implementation/reports/m2_music_live_controls.md) applies supported controllers to sounding DLS voices without restarting their clocks. Timed channel dispatch, original playlists and MUSIC remain required; M2 stays incomplete.

The [immediate MIDI receiver](../../implementation/reports/m2_music_midi_receiver.md) connects real channel controls and note lifetime; timed delivery/playlists and required MUSIC acceptance remain open.

The [timed MIDI continuation](../../implementation/reports/m2_music_timing.md) connects explicit sample deadlines to actual audio and world cleanup. IMA tick mapping, running sections/playlists and required MUSIC acceptance still keep M2 incomplete.

The [IMA clock continuation](../../implementation/reports/m2_music_clock.md) maps explicit musical ticks to the audio queue with original runtime tempo/rounding. Running sections, playlists and required MUSIC acceptance still keep M2 incomplete.

## M3 — Demonstrate the First Original-Code Playable Slice

**Goal:** prove that the preservation approach produces interactive gameplay before expanding systems or content.

Work:

1. Drive the original player/client movement from input. Test walking, looking, jumping, grounding and collision in the selected segment.
2. Execute one original weapon path, including its animation, ammo/reload behavior where applicable, damage and effects. Add a projectile path if the first weapon is hitscan, or a hitscan path if it is projectile-based, before declaring both service families supported.
3. Spawn an original enemy class, exercise perception/movement/attack/death and confirm that its animation events affect gameplay.
4. Exercise a named trigger, a moving door and a pickup through original objects/messages. Ensure player and enemy contacts with the door are meaningful.
5. Connect real sound playback, required effects and enough original HUD information to assess the scenario. Use the editor to inspect player/enemy/weapon state, sound emitters, animation events and trigger links; add pause, single-step and a separate inspection camera.
6. Compare the same scenario against the original executable where available and against the source contracts. Run at different frame rates and with the enemy offscreen.

**Deliverables:** a repeatable scenario script/checklist, local trace/capture references, a runnable build and a discrepancy register. Record actual asset edition and hashes privately as appropriate; do not commit retail content to the workspace.

**Exit gate:** the original player can complete and repeat the sequence without lifecycle failures; outcomes match the reference within declared tolerances. The editor exposes the corresponding live state and remains safe across spawn/destruction/reload. Opening or closing it must not alter gameplay or misroute input. No required service silently succeeds without work.

**Depends on:** M2. **Addresses:** A06, A07, A09, A14, A15, A19. Saving and full campaign coverage are deliberately separate M5 gates; this slice is not a complete game.

## M4 — Complete Portable Rendering and Inspection

Open qualification issue carried from M2: [reservation M2-GPU-001](../../implementation/reports/m2_acceptance.md#reservation-m2-gpu-001), the independently reproduced NVIDIA/D-Bus GPU LeakSanitizer failure. M2 closure does not qualify this backend as sanitizer-clean. Follow the recorded clearance conditions after a relevant graphics/platform change.

**Goal:** complete and validate the SDL_GPU renderer and inspection tools across desktop backends.

SDL_GPU starts in M0 and grows with M2/M3. This stage completes rendering coverage and backend validation; there is no migration of an old OpenGL renderer. Use the [selected SDL main/OpenXR path](01_technical_stack.md#sdl-main-and-openxr) and keep headset support optional while checking the new project's integration.

Work:

1. Keep CPU format/model/world data independent of GPU resources and use one scene representation for rendering and inspection.
2. Implement required world/lightmap, model/skin, sprite, transparency, sky and UI pipelines. Add content-driven effects as M5 discovers them.
3. Validate coordinate transforms, depth, color space, alpha behavior, animation pose and attachment placement against the same inputs. Profile uploads and draw submission.
4. Cross-compile minimal Android/iOS platform shells and test real devices as soon as available, detecting assumptions before completing the renderer. Keep VR optional and support a list of views.
5. Validate viewport picking, overlays, fonts and inspector panels on each backend. Exercise available XR session/swapchain integration without turning full VR gameplay into this stage's gate.

**Deliverables:** portable renderer and inspector, offline shader pipeline and backend/integration evidence.

**Exit gate:** M3 and its inspection tools run through Vulkan/Linux, D3D12/Windows and Metal/macOS with correct visuals and no known GPU validation errors in exercised paths. Compile-only evidence does not close a backend. Full XR gameplay remains M8.

**Depends on:** M0 for initial rendering; M3 for the complete gate. **Addresses:** A17, A18 and presentation aspects of A14. Keep backend fixes local and preserve original gameplay.

## M5 — Complete Base-Game Behavior and Persistence

**Goal:** expand from a representative slice to a completable single-player game.

Work:

1. Build a campaign coverage matrix: levels/transitions, playable characters, weapons and secondary modes, ammunition, pickups/inventory, enemies/bosses, triggers, movers, damage types and difficulty settings. Include save-sensitive and scripted cases. Derive the list from original code and content rather than guessed counts.
2. Implement remaining reachable services and remove behavior-changing placeholders. Preserve source-based AI, projectile logic and rules; do not substitute NoMadEngine/nomad implementations for missing integration.
3. Restore persistence: original save/load callbacks, engine object state, stable-ID reference repair, schedules, animation and session state. Support process-restart restore and test missing/corrupt save failures. Document whether retail saves are unsupported.
4. Complete original menus/HUD/text, controls, audio/music, cinematic handling and relevant effects. Resolve codec/content gaps recorded in M0/M2. Keep accessibility or optional presentation improvements separate from the fidelity baseline.
5. Exercise level transitions, death/restart, character and difficulty variants, and repeated load/unload. Finish reviewing original-code adaptations that affect delivered content.
6. Complete inspection coverage for every supported element category, including hidden/inactive objects, lights, materials, effects, audio, links and failed resource references. Compare source-world entries with runtime state to expose omissions.

**Deliverables:** a campaign support matrix, versioned save format, discrepancy/known-limitations report and full playthrough evidence.

**Exit gate:** the base campaign can be completed with no missing required class/service; the coverage matrix is tested; saves restore equivalent state after restart; repeated play does not accumulate stale resources. Required music/cinematic/UI behavior and editor inspection coverage are complete. Final acceptance uses the M4 renderer.

**Depends on:** M3; implementation can overlap M4, but the final gate requires M4. **Addresses:** A06–A09, A14, A15, A17, A19. Nightmare Levels, multiplayer and original-save compatibility remain explicit follow-up scope.

## M6 — Optimize and Release on Desktop

**Goal:** a reproducible Windows/Linux/macOS build with measured performance and preservation evidence.

Work:

1. Measure the [validation scenarios](04_validation_and_traceability.md#performance-measurement) on named machines. Fix measured costs in world queries, resource residency, allocations, skinning and draw submission while retaining comparison tests.
2. Add bounded archive access/caching and spatial acceleration where needed. Use allocation reuse, batching and GPU skinning based on evidence. Keep simulation ordering stable while considering asynchronous decoding; avoid a speculative multithreaded rewrite.
3. Validate optimized builds with sanitizers in supported configurations and run extended gameplay/load/unload tests. Do not enable fast-math or CPU-specific instructions globally without fidelity and device checks.
   Measure editor overhead separately and verify that disabling inspection removes its UI/debug work without changing simulation results.
4. Package runtime dependencies, shaders and configuration correctly on all three desktop platforms. Test paths outside the source tree, user content selection, case differences and clean-machine startup.
5. Record source/dependency notices and distribution prerequisites before publishing. The audit did not establish redistribution permissions for every source/asset; source availability alone is not that evidence. This does not block local technical validation.

**Deliverables:** repeatable packages, per-platform evidence, performance captures, installation instructions and honest known limitations.

**Exit gate:** required gameplay, save/load and presentation checks pass on all three named desktop configurations. Meet the recorded frame-time/memory budgets and show no unbounded growth under the defined soak test. No claim of optimization is made without before/after measurements and unchanged fidelity results.

**Depends on:** M4 + M5. **Addresses:** A16–A19; closes release-related portability gaps. Performance instrumentation starts earlier; this stage is the optimization/release gate, not the first time performance is observed.

## M7 — Deliver Android and iOS

**Goal:** run the same game/core on supported mobile hardware.

Complete ARM64 builds, content import/storage, touch and controller mappings, lifecycle pause/resume, audio focus, orientation/UI scaling and memory-pressure behavior. Validate offline shader packaging and avoid desktop filesystem or CPU-feature assumptions. Test save compatibility between supported desktop/mobile builds. Select device tiers and sustained frame-rate budgets from measurements; account for thermal throttling and battery use.

**Exit gate:** the reference slice and representative campaign/save/transition cases pass on named Android and iOS devices, including background/resume and prolonged play. Publish minimum supported device/OS capabilities from these results. Cross-compilation alone does not close the stage.

**Depends on:** M6; platform smoke work starts during M4. **Addresses:** A17–A19 on mobile.

## M8 — Deliver OpenXR, Then Reassess Shogo

**Goal:** add headset presentation and interaction while retaining the verified original simulation.

Use the recorded SDL `main` revision and OpenXR integration, updating to the latest `main` commit when refreshing the dependency. Implement stereo views, predicted poses, actions, aiming, menus and session-loss recovery. Define world scale and comfort/interaction adaptations explicitly while keeping original simulation. Test a desktop runtime, then a standalone Android headset if desired. Reuse M7's Android work without waiting for the iOS port.

**Exit gate:** named headset/runtime combinations pass input, stereo, pause/resume and sustained frame-timing checks at the selected refresh rate. The ordinary non-VR build remains independent of XR. Any platform with no tested suitable runtime remains unsupported.

**Depends on:** M6 and SDL/OpenXR integration in the new runtime; standalone delivery also needs Android platform work. **Addresses:** the XR portion of A17 and new headset-specific behavior.

After Blood 2's base campaign and runtime contracts are established, perform a separate Shogo compatibility assessment using `sources/Shogo_Aegis/`, explicitly distinguishing Aegis changes. Reuse proven engine services; do not expand this initial roadmap into a generic multi-game engine or claim Shogo is supported because one executable compiled.

## Managing Dependencies and Uncertainty

| Risk/input | Earliest check | Consequence if unresolved |
| --- | --- | --- |
| Interface definitions and original callback semantics | M1 | Blocks meaningful original-code execution; fix boundary before expanding gameplay |
| Reference edition/patch and executable behavior | Supplied installation in M0; M2/M3 comparison | Files are present; identify the exact baseline and run observations before claiming fidelity |
| Unknown format variants and coordinates | M2 | Isolate failing content and produce bounded diagnostics; no silent recovery in gameplay |
| Full reachable engine-service surface | M2/M3 traces, M5 matrix | Reestimate after each scenario; lexical counts are not implementation estimates |
| SDL_GPU backend/device coverage | M0 startup through M4 validation | Fix backend-specific issues locally; record untested devices |
| SDL main updates and project OpenXR integration | Record SHA in M0; integration checks through M8 | Use the selected main-branch support and retain repeatable revisions/tests |
| Apple/mobile/headset runners | M0 register, M4 smoke work | Keep their gates pending rather than presenting cross-platform design as validation |
| Imported behavior/dependency debt | Review each cherry-pick/import, M0–M5 | Keep the useful part, adapt it to the small new runtime and record necessary divergences |

After M1, estimate the next service batch using actual implementation/test effort. After M3, estimate campaign coverage using the observed class/API gaps. Do not extrapolate from source-line counts or assign a completion percentage from the number of registered classes.

### Lighting investigation note (2026-10-15)

For the lighting milestone, investigate Blood II `DirLight` as a spotlight
(project-owner clarification), despite its name. Recover the implicit main
directional light from original preprocessing/world data; compare Shogo's
explicit property before deciding how to represent it. Verify spotlight cone,
range, direction and baked contribution separately from the main light. Do not
map every `DirLight` to a global directional source.

The [original CMovement continuation](../../implementation/reports/m2_ai_navigation.md) adds checked transform caching and
source obstacle-path calculation with inspection. Partial routes remain failures;
no actor following or production NPC registration is implied. Required classes,
AI update/actions and services still keep the M2 gate open.

The [NPC weapon-drop continuation](../../implementation/reports/m2_weapon_drops.md) connects the original Cabal drop,
independent falling Beretta, original pickup and player collection. Corpse/gib
services, player drop controls and the full M2 exit gate remain open.

The [level-exit continuation](../../implementation/reports/m2_exits.md) registers original ExitTrigger and
connects typed shell exit/load requests. Read [contracts](../../contracts/exit_contracts.md). Preserve
EndEpisode/EndOfEpisode spelling, start-name copy-before-broadcast, delayed virtual
slot rules and dead-player early return. NEW_LEVEL explicitly refuses missing
KeepAliveSave/Load before teardown; do not substitute fresh-game entry or claim a
completed transfer. SubwayA has 203 created, four transferred sounds, 372 editor-only,
one unsupported (Gideon), no failures. Required transfer/FX/services keep M2 open.

The [named keep-alive entry continuation](../../implementation/reports/m2_player_keepalive.md)
now enters an already loaded destination using original inventory type 1 and named
start behavior. Private SubwayA-to-CityHub harness entry preserves supported player
state, but automatic exit transfer still requires safe destination preparation.
Staged player cleanup does not rewind world callbacks; M2's observable gate is unchanged.

The [sprite presentation continuation](../../implementation/reports/m2_sprite_view.md) supplies
bounded alpha-textured original sprites and inspection needed by later FX. It does
not implement required FX emitters, Gideon or safe automatic world transfer, and
does not change the M2 exit gate or mark M3/M4 complete.

The [original explosion flame continuation](../../implementation/reports/m2_explosion_flames.md) adds selected Tesla particle emission and inspection. Required illumination, safe NEW_LEVEL and the observable M2 gate remain open.

The [destination preparation continuation](../../implementation/reports/m2_destination.md) inspects destination DAT geometry, primary collision and property schemas without leaving the current session.
Original callbacks/resources/start triggers are still unverified; this prerequisite does not enable automatic NEW_LEVEL or close M2.

The [destination initialization continuation](../../implementation/reports/m2_destination_trial.md) now executes original destination callbacks and named player entry in a temporary session, destroys it and resumes the retained source.
Earlier callback-validation omissions are superseded for this explicit rehearsal only; destination promotion, required services and safe NEW_LEVEL still keep M2 open.

The [production Gideon continuation](../../implementation/reports/m2_gideon_runtime.md) adds original initialization, AI_Update, typed boss-health delivery and inspection. The fifth shared table is isolated during destination rehearsals. It supersedes the earlier blanket production-Gideon omission; complete boss encounter/death parity, required remaining services and safe NEW_LEVEL still keep M2 incomplete.

The [initial corpse placement correction](../../implementation/reports/m2_corpse_placement.md) now clears the wider original death-animation body in the SubwayA keyboard-fire encounter, with bounded atomic placement and inspection. Difficult fits still fail explicitly; M2 remains incomplete.

The [medkit continuation](../../implementation/reports/m2_medkit.md) adds the original pickup, direct H activation, local sound and preserved charges. It does not close required remaining services or safe NEW_LEVEL; M2 remains incomplete.

The [playtest control/combat correction](../../implementation/reports/m2_playtest_input.md) connects automatic mouse ownership on entry, original Cultist dodge/roll dispatch and bounded alternative-axis corpse placement. The authored first encounter is exercised through SDL input. Required services and safe NEW_LEVEL still keep M2 incomplete.

The [Metal shader continuation](../../implementation/reports/m2_metal.md) packages offline SDL_shadercross output for the existing world/model/FX pipelines. The runtime no longer restricts world entry to Vulkan. The user confirms macOS execution; native automated tests remain pending; this platform correction does not change the M2 exit gate.

The [corpse doorway correction](../../implementation/reports/m2_corpse_clearance.md) addresses a reproduced frozen dead Cultist by adding checked source-center separation and swept living-body clearance to initial corpse placement. Original dimensions and bounded failures remain intact; in-game inspection records the extra sweep. This does not certify every later enemy or close M2.


Continuation: [original corpse blood pools](../../implementation/reports/m2_blood_splats.md) supersedes the pool omission for corpse ground hits. Blood-spurt particles, gib impact callers, shared SFX eviction and other required services remain incomplete. The M2 exit gate is unchanged.

Continuation: [original blood spurts and debris impacts](../../implementation/reports/m2_blood_spurts.md) supersedes the corresponding blood-spurt/gib-impact omissions for the selected flesh recipes. Separate flesh sparks, outside-world classification, shared SFX eviction, full save/restore and other required services remain incomplete. The M2 exit criteria are unchanged.

The [original impact-spark continuation](../../implementation/reports/m2_sparks.md) adds flesh impact bursts and metal sparks with checked detached lifetime and inspection. M2 remains incomplete; this supersedes earlier blanket spark omissions only for these recipes.

The [compact HUD and numeric weapon continuation](../../implementation/reports/m2_hud.md) adds actual health/armor/current ammunition and original owned-slot selection with 1..9/0, including inspector-free rendering. Original PCX artwork and remaining required services are still pending; M2 remains incomplete. The evidence also records the next user-reported two-level playtest checks without treating hypotheses as diagnoses.

The [original pickup-audio continuation](../../implementation/reports/m2_pickup_audio.md) connects accepted collection to detached spatial WAV playback, including keys, with durable inspection and source-independent lifetime. M2 remains incomplete. The evidence distinguishes successful diagnostic post-transfer firing from unresolved user reports and records severe post-transition stutter as the next performance investigation.

The [collision traversal continuation](../../implementation/reports/m2_collision_traversal.md) reduces measured post-transition activation-query work without changing original AI cadence or triangle contacts. Services exposes primary-world traversal counters. Sampled before/after equivalence and NVIDIA/lifetime checks are recorded; CPU skinning and other frame costs remain explicit, and M2 stays incomplete.

The [bound-container index](../../implementation/reports/m2_container_index.md) reduces repeated post-transition object scans while retaining fresh checked snapshots, original volume order and deferred destruction. Read [contracts](../../contracts/container_index_contracts.md); never mutate a bound shape outside Objects. Container inspection includes the index count. CPU model preparation and other playtest reports remain; M2 stays incomplete.

The [model preparation optimization](../../implementation/reports/m2_model_preparation.md) reuses exact local geometry for immutable shared ABC assets and removes temporary heap pose scratch. Read [contracts](../../contracts/model_preparation_contracts.md); preserve weak ownership, current visibility/materials, packed-deformation order and world-revision clearing. Inspection reports actual reuse and retained vertex bytes. Skinning remains on the CPU; remaining frame costs and the M2 exit gate stay open.

The [NPC animation-clearance correction](../../implementation/reports/m2_ai_animation_clearance.md) fixes a reproduced permanent AI suspension after an ordinary collision refusal. Required remaining services and the M2 exit gate remain unchanged.

The [original PolyGrid continuation](../../implementation/reports/m2_polygrids.md) connects VolumeBrush surface creation, original high-detail plasma/scale rules and actual transparent SDL_GPU rendering. Read [contracts](../../contracts/polygrid_contracts.md). Preserve independent publisher lifetime, Current.y pan, hidden-clock pause, shared per-world phases, checked resource failures and revision-owned caches. Visual publishers are excluded from replacement body physics; static containers, explicit always-update timing and separate alpha sorting remain bounded policies. CityHub water now renders; underwater fog, moving volumes and the M2 gate stay open.

The [NPC ammunition continuation](../../implementation/reports/m2_ai_ammo.md) connects original FindAmmo and MID_ADDAMMO instead of suspending an empty-ammo actor. Read [contracts](../../contracts/ai_ammo_contracts.md). Preserve original class selection, ray-hit visibility, empty-route escape, single integration and capped collection. Specific+AvoidTarget skips unused undefined arithmetic; removed pickup trackers retain only their owned route. Private contact/hidden-inspector tests do not prove autonomous retail pickup pursuit or resolve every idle-NPC report. M2 remains incomplete.

The [service coverage continuation](../../implementation/reports/m2_service_coverage.md) adds 171 bounded interface entries and three supporting trace boundaries, with independent implementation/observation status and restored-player/teardown scope. The limited SubwayA → CityHub diagnostic reaches 108 boundaries in CityHub through teardown, leaving 66 unexercised. Required-path coverage and the unchanged M2 acceptance review remain open; no percentage or milestone is certified by these counters.

The [authored exit-fade continuation](../../implementation/reports/m2_screen_fade.md) connects original player fade commands and validates the actual train exit relay through contact, combat and NEW_LEVEL. Its final explosions exposed a measured smoke-pool limit, now bounded to support the original emission. Unsupported player commands remain visible; diagnostic relocation is not a full playthrough and M2 stays incomplete.

The [original trigger-audio continuation](../../implementation/reports/m2_trigger_audio.md) connects activation and lock/explicit-unlock one-shots with checked speaker lifetime and inspection. Key-response unlock remains silent as in the source. Private CDM and labelled NVIDIA train fixtures verify real decoded audio; music, remaining required services and the unchanged M2 acceptance gate remain open.


The [original player VoiceMgr continuation](../../implementation/reports/m2_player_voices.md) connects weapon-pickup speech, SOUND and conversation interruption, with actual streamed/local WAV playback and inspected failure/history. Unique replies survive safe NEW_LEVEL by value; two absent installed references stay failures. Other original callers, interactive IMA music and required-service acceptance remain open; M2 is incomplete.


The [combat voice continuation](../../implementation/reports/m2_combat_voices.md) connects original player pain/burning and NPC/corpse kill/gib replies. Read [contracts](../../contracts/combat_voice_contracts.md); preserve integer net-health loss, chance-draw order, distinct killer filters, checked local-client lookup and cosmetic failure independence. Queued replies are not successful playback; inspect the actual VoiceMgr status. Remaining original callers, interactive music and the M2 gate stay open.

The [original single-player death continuation](../../implementation/reports/m2_player_death.md) connects once-only death, inventory cleanup, real corpses/fragments and the client orbit. Live-checkpoint re-association resets the view; whole-world/dead-player saves and the actual M2 acceptance gate remain incomplete.

The [original movement-audio continuation](../../implementation/reports/m2_movement_audio.md) connects grounded Jump voices and sampled-height material landing sounds, with snapshot preservation and in-game inspection. Fall damage, other required services and the actual M2 gate remain open.

The [original falling-damage continuation](../../implementation/reports/m2_fall_damage.md) connects accepted stopping
forces, pre-stop callback velocity, original player thresholds and restored mass.
Real Caleb diagnostic falls exercise original death/corpse/camera at 30/60/120 Hz;
this is not retail physics parity. Dedicated pusher/body/projectile forces, IMA
music, whole-world saves and remaining required services keep M2 incomplete.

The [original player damage-impulse continuation](../../implementation/reports/m2_player_impulses.md) connects accepted damage to the shared player body with the source single-player self-hit exclusion. The existing original drag/caps and restoration reset remain; production NPC impulses, required services and the actual M2 gate stay open.

The [original music asset prerequisite](../../implementation/reports/m2_music_assets.md) accounts for the original playlist requests and external AASE/AASY/DLS containers. Structural inspection is not synthesis: the reached MUSIC command remains unsupported, and M2 stays incomplete.


The [DLS data continuation](../../implementation/reports/m2_music_dls.md) decodes original external instrument-bank PCM, cue/region mappings and raw articulation. Read [contracts](../../contracts/music_dls_contracts.md); preserve cue indirection, signed values, region overrides, strict effective loop bounds and immutable world ownership. Cabal's unused invalid wave default remains visible; Crudux/Track1 invalid effective loops refuse decoding without invented repair. No synthesis or successful MUSIC service is claimed; M2 remains incomplete.

The [DLS voice continuation](../../implementation/reports/m2_music_sampler.md) adds bounded sample/envelope auditions and shared audio-clock/output verification. It does not implement original sequence/style/personality playback or close the required MUSIC service; M2 remains incomplete.

The [legacy DLS pan continuation](../../implementation/reports/m2_music_legacy_pan.md) supplies evidenced integer/fixed-point pan compatibility. All 38 Cabal region auditions synthesize; original IMA sequencing and the actual M2 gate remain open.


The [IMA pattern-note continuation](../../implementation/reports/m2_music_notes.md) decodes the installed AASY pattern/grid/note profile with original-reader equivalence and read-only inspection. Read [contracts](../../contracts/music_note_contracts.md); preserve record order, signed fields, opaque curves/header tails, bounded atomic failure and immutable world ownership. Decoded notes are not final MIDI events or successful MUSIC playback; sequencing, required services and the actual M2 gate remain open.

The [IMA band continuation](../../implementation/reports/m2_music_bands.md) decodes original style/section channel records and prepares verified original-order controller/program messages. Read [contracts](../../contracts/music_band_contracts.md); preserve packed flags, legacy fallback, signed transpose, direct scope, raw unknown fields and immutable ownership. Setup messages are not sent; effective band selection, sequencing and the actual M2 gate remain open.

The [IMA section continuation](../../implementation/reports/m2_music_sections.md) adds original-reader-checked header timing and command/mute/chord data. Authored meter is separate from style reconciliation; effective selection, sequencing and MUSIC remain unavailable. The actual M2 gate remains open.

The [IMA timeline continuation](../../implementation/reports/m2_music_timeline.md) reconciles style meter, command/chord times and beat-sampled mutes with original executable checks. Read [contracts](../../contracts/music_timeline_contracts.md); preserve signed/file-order behavior, the initial future mute, negative chord beats, explicit binding failures and immutable bounded ownership. Pattern choice, pitch harmonization, actual MUSIC playback and the M2 gate remain open.

The [IMA variation continuation](../../implementation/reports/m2_music_variations.md) decodes complete pattern channels and reproduces original defaults and bounded variation decisions. Read [contracts](../../contracts/music_variation_contracts.md); preserve immutable authored fields, per-row default bits, random precedence/draw order and unfiltered cycle/hold behavior. Explicit diagnostic choices are not live scheduling or MUSIC playback; pitch conversion, remaining required services and the M2 gate stay open.

The [IMA note-event continuation](../../implementation/reports/m2_music_events.md) reproduces the original post-harmonization event arithmetic with explicit pitch/timing/map/draw inputs and diagnostic inspection. Read [contracts](../../contracts/music_event_contracts.md); preserve zero-pitch suppression, signed range/draw order, octave folding and equal-velocity note-off. Reject unsafe indexes/tick overflow atomically. Prepared messages are not harmonic conversion, live scheduling or MUSIC playback; the actual M2 gate stays open.

The [IMA harmonic-pitch continuation](../../implementation/reports/m2_music_pitch.md) reproduces original mode/chord/scale/octave/range arithmetic with explicit runtime harmony and in-game event inspection. Read [contracts](../../contracts/music_pitch_contracts.md); preserve raw bypass, signed/unsigned nibble differences, ordered range adjustment and zero suppression. Bound nonterminating mode-4 scale walks with explicit failure; retain finite long-walk results. No personality reconciliation, current-chord selection or MUSIC playback is claimed. M2 remains incomplete.


The [IMA prepared-harmony continuation](../../implementation/reports/m2_music_harmony.md) connects bounded external REPs metadata and original retained-chord pitch/variation flags. Read [contracts](../../contracts/music_harmony_contracts.md); preserve signed remainders, original mask/flag order, next-retained-chord classification and explicit missing bindings. Keep opaque personality graphs and the limited zero-ID filename profile visible. Prepared harmony does not select or activate a live chord, schedule music or satisfy MUSIC; M2 remains incomplete.

The [IMA section/pattern continuation](../../implementation/reports/m2_music_selection.md) adds executable-verified normal section decisions and explicit tick inspection. Live MUSIC scheduling, remaining required services and the M2 exit gate are still incomplete.


The [IMA grid-cursor continuation](../../implementation/reports/m2_music_cursor.md) connects original normal-mode grid progression, exact-command restarts, variation cadence and note admission. Read [contracts](../../contracts/music_cursor_contracts.md); preserve the saved old ordinal, mask/root-only change test, explicit draws, silent missing grids and atomic world-owned lifetime. This is a manual single-section diagnostic, not audio playback. Clocks, bands/curves, active notes and playlists remain; MUSIC and M2 stay incomplete.


The [IMA band-selection continuation](../../implementation/reports/m2_music_band_selection.md) connects original last-section-band priority and reverse-order marked style fallback. Read [contracts](../../contracts/music_band_selection_contracts.md); preserve header-before-band guards, any-nonzero byte104, absent-default failure and immutable source ownership. Selection and controller-setup errors remain distinct. No live MIDI/soundtrack is claimed; remaining music services and M2 stay incomplete.


The [finite IMA section continuation](../../implementation/reports/m2_music_program.md) plays complete authored sections on the existing audio clock. Read [contracts](../../contracts/music_program_contracts.md); preserve original terminal-note order/deadlines, explicit audition seed/preroll, bounded atomic preparation and cancellation without clock reset. This is finite inspection playback; game playlists/transitions and required MUSIC keep the unchanged M2 gate open.

The [finite playlist continuation](../../implementation/reports/m2_music_playlists.md) connects original level-request selection to one-pass transition/list audio. Read [contracts](../../contracts/music_playlist_contracts.md); preserve authored duplicates/order, musical-duration boundaries, shared tempo/preroll, atomic admission and aggregate work limits. Requested loops and live switches remain unexecuted; game MUSIC and the unchanged M2 gate stay incomplete.

The [repeating music continuation](../../implementation/reports/m2_music_transport.md) connects continuous prepared playlists to the audio clock, including fractional tempo carry, early notes, tails and bounded cancellation. Original live MUSIC/switching and variation state remain incomplete; the M2 exit gate is unchanged.

The [live MUSIC continuation](../../implementation/reports/m2_music_messages.md) connects original player/client requests to immediate and next-song playback. Read [contracts](../../contracts/music_message_contracts.md); preserve owned messages, logical mutation before failure, source motif/break no-ops, bounded pending replacement and world-owned cancellation. Boundary/tail policy and repeated prepared variations remain explicit adaptations; retail activation fidelity and M2 remain incomplete.

The [session-cost audit](../../implementation/reports/m2_session_cost.md) removes temporary trace-name allocations and zero-depth audio modulation work. Read [contracts](../../contracts/session_cost_contracts.md); preserve owned counters, exact scalar PCM/cursor progression and all existing session/test deadlines. The full M2 acceptance review and runtime-start guard remain open.

The [checked startup continuation](../../implementation/reports/m2_session_start.md) supersedes the historical unconditional RunWorld refusal. Direct 3D entry and the finite headless harness now validate source dispositions, original player association, decoded resources and retained failures. This is an entry checkpoint; the unchanged consolidated M2 acceptance gate remains open.

The [GPU lifetime investigation](../../implementation/reports/m2_gpu_lifetime.md) reproduces the graphics shutdown leak without the game, including a standalone Vulkan control. Fresh NVIDIA module attribution is complete; the failing GPU sanitizer result remains visible. Final M2 acceptance remains open under the unchanged gate.
