# Blood 2 Reconstruction Documentation

Current status: **M2 closed with reservation M2-GPU-001 on 2026-10-18**. The [acceptance and closure record](../implementation/reports/m2_acceptance.md) separates the accepted SubwayA-to-CityHub session from the unresolved external NVIDIA/D-Bus GPU sanitizer failure. M3 is next; later milestones remain unaccepted. Progress entries below are historical.

Document filenames are lowercase. Repository entrypoints remain `README.md` and
`AGENTS.md`. Historical evidence logs, patches and hash ledgers retain the paths
from their recorded commits; they are immutable snapshots, not current path indexes.

The [original blood-spurt continuation](../implementation/reports/m2_blood_spurts.md) adds weapon entry/exit jets, flesh-fragment launch jets and impact marks, and deferred limb impact requests, with real game textures and inspection. Cosmetic random draws remain isolated from gameplay. Separate flesh sparks, generic debris, full save/restore and the actual M2 gate remain incomplete. Earlier blanket blood-spurt/gib-impact omissions are historical.

The [original smell-trail continuation](../implementation/reports/m2_ai_smell.md) connects the original periodic player trail and shared AI SearchSmellTarget state, with checked hint lifetime and inspection. Original no-target/no-cursor exits and zero NPC smell ranges are preserved; no automatic trail acquisition is invented. Required remaining services and the actual M2 gate stay open.

The [burning death and dismemberment continuation](../implementation/reports/m2_death_effects.md) connects original lethal fire/gib decisions, burning corpses, postmortem limb loss and visible client flesh fragments, with checked cleanup and in-game inspection. These paths no longer fail solely because fire/gib services were missing. Blood-spurt/splat effects, generic gib/save services and the actual M2 gate remain open. Earlier blanket fire/gib omissions are historical.

The [scripted opportunity-fire continuation](../implementation/reports/m2_ai_script_fire.md) connects original standing/walking/running fire to FOLLOWPATH and MOVETOOBJECT, with inspection and checked lifetime. Real Cultist model diagnostics verify shots, damage and movement through reloads; these are not authored encounter or traversal proofs. M2 remains incomplete.

The [AI alignment correction](../implementation/reports/m2_ai_alignment.md) restores the original parallel-axis fallback used by CMovement. Two reproduced CityHub Cultist update failures now become ordinary blocked-path decisions; inspection exposes the resolved axes. A 120-second relocation diagnostic reports no suspended NPCs after the correction, but is not a playthrough. M2 remains incomplete.

The [CityHub entry continuation](../implementation/reports/m2_cityhub_entry.md) records `fallengrills` as an explicitly omitted object because its named BSP is absent. The source instance is rejected before initial update, matching the available LithTech loader behavior; no replacement geometry is invented. CityHub now has 453 created / 31 sounds / 541 editor-only / one omitted / zero unsupported / zero failed entries. Safe NEW_LEVEL preserves the supported player and enters at `01_egressa`; required SPR world materials now decode and animate through SDL_GPU. M2 remains incomplete; earlier CityHub refusal/count statements are historical.

The [M16 continuation](../implementation/reports/m2_assault.md) connects original primary fire, alternate grenade charges, pickup/drop, independent grenade lifetime and inspection. CityHub has 433 created / 31 transferred sounds / 20 unsupported / one failed. Finite inverted ABC key bounds remain visible metadata warnings. The original Cultist lacks rifle animations; direct AI firing is tested separately from unavailable autonomous M16 combat. Replacement grenade physics, required services and safe NEW_LEVEL keep M2 open.

The [world-model debris continuation](../implementation/reports/m2_world_debris.md) connects original named-fragment activation, gravity, rotation and independent lifetime, with rendered geometry and inspection. CityHub now has 432 created / 31 transferred sounds / 21 unsupported / one failed entry. Conservative moving bounds are replacement physics; missing geometry, required services and safe NEW_LEVEL keep M2 incomplete.

The [Flare Gun continuation](../implementation/reports/m2_flares.md) connects original primary sticking/burn damage, alternate fragments, both player hands, pickup/drop and autonomous Cultist fire. CityHub now has 422 created / 31 transferred sounds / 31 unsupported / one failed entry; earlier counts below are historical. Missing geometry, required services and safe NEW_LEVEL keep M2 incomplete.

The [compound-shell correction](../implementation/reports/m2_compound_shells.md) validates independently closed DAT components that share edges. CityHub now creates `startfences` and `Buildings`: 421 created / 31 transferred sounds / 31 unsupported / two failed. Collision gaps, hidden/removal inspection and GPU submission are checked; earlier counts below are historical. Missing geometry, FLAREGUN, required services and safe NEW_LEVEL keep M2 incomplete.

The [Mac-10 continuation](../implementation/reports/m2_mac10.md) connects original automatic fire, dual inventory, pickup/NPC drop and autonomous Cultist use. Actual left-view assets exercise the restored original alternate-animation fallback. CityHub now has 419 created objects, 31 transferred sounds, 31 unsupported and four failed entries; earlier counts below are historical. Chrome, remaining required services and safe NEW_LEVEL keep M2 incomplete.

The [audio-stream continuation](../implementation/reports/m2_audio_stream.md) connects real bounded PCM reads from REZ and retains observed VoiceType=0 metadata. CityHub now has 418 created objects, 31 transferred sounds, 32 unsupported and four failed entries; its five SoundFX create, four start initially, and 35 streamed voices share 384 KiB of buffers. Compressed streams, TypeAction schemas, required remaining services and safe NEW_LEVEL keep M2 incomplete.

The [SoundFX continuation](../implementation/reports/m2_sound_fx.md) connects original nonstreaming ramp/ON/OFF rules to real audio and inspection. The private catalog exposes unresolved historical schemas on all 124 authored emitters (121 VoiceType / three TypeAction). CityHub remains 413 created; its five SoundFX entries are now explicit schema failures (32 unsupported / 40 failed), not successful playback. Streaming, safe NEW_LEVEL and M2 remain incomplete.

The [original destructible-model continuation](../implementation/reports/m2_destructable_models.md) connects 122 CityHub objects to original damage, model replacement, fading and bounded box physics. CityHub now has 413 created, 37 unsupported and 35 failed entries; earlier counts below are historical. Contact-force sliding audio, required remaining services, safe NEW_LEVEL and M2 remain incomplete.

The [original built-in Model continuation](../implementation/reports/m2_builtin_models.md) renders 47 authored CityHub lamps with real ABC/DTX resources and inspection. CityHub now has 291 created, 159 unsupported and 35 failed entries; the source session remains intact during rehearsal. Destructible models, safe NEW_LEVEL and M2 remain incomplete.

Continuation: [original fragment smoke trails](../implementation/reports/m2_object_particles.md) now supplies
distance-based emission, source stop rules and actual DTX particle rendering.
Other required services and safe NEW_LEVEL keep M2 incomplete.

Current continuation: [original explosion metal fragments](../implementation/reports/m2_explosion_fragments.md) adds
launch, collision/rest, fade, real model rendering and live inspection. Trails,
remaining FX/illumination and safe NEW_LEVEL keep M2 incomplete.

Current continuation: [original explosion particle rings](../implementation/reports/m2_explosion_rings.md) adds
fire/smoke particle motion, real DTX alpha rendering and per-particle inspection.
Remaining composite FX, illumination and safe NEW_LEVEL keep M2 incomplete.

The [explosion point-light continuation](../implementation/reports/m2_explosion_lights.md) evaluates original flash/residual light colors, radii, delays and lifetime, independently of emitter/resource lifetime.
Inspection and hidden/reload checks cover all nine SubwayA emitters. Surface
illumination, remaining composite components, safe level transfer and M2 remain open.

The [explosion model continuation](../implementation/reports/m2_explosion_models.md) adds original ABC effects,
growth, rotation, alpha and independent lifetime to the existing fire/smoke sprites.
The nine SubwayA emitters and GPU model opacity are tested and inspectable. Required
particle/light/fragment services, safe automatic level transfer and M2 remain open.

The [explosion sprite continuation](../implementation/reports/m2_explosion_sprites.md) connects original fire/smoke
recipes and lifecycle to real alpha-textured rendering and in-game inspection.
Nine SubwayA emitters are exercised through creation, removal and reload. Composite
models/particles/lights, safe automatic level transfer and M2 remain incomplete.

The [sprite presentation continuation](../implementation/reports/m2_sprite_view.md) renders the 12 original SubwayA
blood splats with decoded alpha and in-game resource/mode diagnostics. Hidden UI,
world depth and unload/reload are tested. This is a prerequisite for client FX;
required FX emitters, safe automatic level transfer and M2 remain incomplete.

The [named keep-alive entry continuation](../implementation/reports/m2_player_keepalive.md) preserves supported player stats/weapons into an explicitly loaded destination,
discards keys and executes original named start entry. Session exposes its mode,
selected marker and fallback. Automatic level transfer and the M2 gate remain open.

Latest gameplay continuation: [original AI commands and hearing](../implementation/reports/m2_ai_triggers.md) connects
trigger commands, two-faction broadcasts and player sound alerts, building on
[weapon actions](../implementation/reports/m2_ai_fire.md) and [perception](../implementation/reports/m2_ai_perception.md).
Full NPC initialization/AI_Update, required actions, client FX and ExitTrigger
still block the actual M2 gate.

Latest presentation: [animated models and first-person weapons](../implementation/reports/m2_model_view.md), building on the textured 3D world. Original view offsets, skin resources, checked attachments and corrected ABC node rotations are now visible. Required NPCs, ExitTrigger, client FX and the unchanged M2 gate remain incomplete. Earlier evidence retains its historical scope.

The documentation contains **phase 1, inventory**, **phase 2, technical audit**, **phase 3, reconstruction roadmap**, and implementation evidence. **[M0](../implementation/reports/m0.md) and [M1](../implementation/reports/m1.md) are completed on Linux** in the new [Blood2 project](../../README.md); **[M2 is closed with reservation M2-GPU-001](../implementation/reports/m2_acceptance.md)** for the selected SubwayA-to-CityHub session. M3 is next; M3–M8 remain unaccepted. Start with the roadmap for direction and the closure record for verified behavior and the open external GPU issue.

The revised direction is a **new project from scratch**, with selective cherry-picks, SDL at the latest `main` commit, a small in-game inspection editor, and no EnTT/ECS by default. The [local Blood II installation](Roadmap/readme.md#local-reference-installation) has been used for the M2 private world tests; original-executable comparisons remain future work. Inventory and audit records retain their historical scope.

Earlier increment: [original pickups and destruction spawns](../implementation/reports/m2_pickups.md). M2 remains incomplete.

## Phase 3: Reconstruction Roadmap

| Document | Contents |
| --- | --- |
| [Roadmap overview](Roadmap/readme.md) | Recommended direction, scope, dependencies and first implementation task |
| [Technical stack](Roadmap/01_technical_stack.md) | C++ and Rust assessment, SDL main/GPU/OpenXR, Dear ImGui and minimal dependencies |
| [Runtime architecture](Roadmap/02_architecture.md) | Simple original-code integration, engine services and in-game inspection |
| [Implementation milestones](Roadmap/03_milestones.md) | New project foundation through original gameplay, campaign, desktop, mobile and VR |
| [Validation and audit traceability](Roadmap/04_validation_and_traceability.md) | Fidelity checks, performance budgets and planned responses to findings A01–A20 |

## Implementation Results

- [Original character motion keys](../implementation/reports/m2_character_motion.md): bounded Sparam, source motion order, checked lifetime and explicit synthetic/private evidence. M2 remains incomplete.
- [Original character audio and footsteps](../implementation/reports/m2_character_audio.md): voice lifetime, model keys, baked materials and liquid selection. M2 remains incomplete.

Earlier: [original pickups and destruction spawns](../implementation/reports/m2_pickups.md),
with source rules, typed messages, ingame inspection and validation.


| Milestone | Evidence |
| --- | --- |
| [M2 continuation — Keyframed train](../implementation/reports/m2_worldpaths.md) | Static brush translation, atomic collision/riders and path inspection |
| [M2 continuation — Dynamic lights](../implementation/reports/m2_lights.md) | Original light rules, evaluated client waveforms and placement |
| [M2 continuation — Keyframes](../implementation/reports/m2_keyframers.md) | Original camera paths and consumed-key ownership |
| [M2 continuation — Cameras](../implementation/reports/m2_cameras.md) | Original tracking/view selection and player gates |
| [M2 continuation — Pickups and destruction spawns](../implementation/reports/m2_pickups.md) | Twelve original pickup classes, capped player collection, SpawnObject and inspection |
| [M2 continuation — Pushable brushes](../implementation/reports/m2_pushables.md) | Original body properties/impulses and bounded box physics |
| [M2 continuation — Explosive brushes](../implementation/reports/m2_explosions.md) | Original area damage and debris requests |
| [M2 continuation — Rotating/destructible brushes](../implementation/reports/m2_brushes.md) | Original rotators, damage/death triggers and destruction |
| [M2 continuation — USE and rotating doors](../implementation/reports/m2_interaction.md) | Original USE, rotating geometry and atomic collision |
| [M2 continuation — Triggers and keys](../implementation/reports/m2_triggers.md) | Original triggers, owned SelfTrigger, key pickups/queries, inspector and validation |
| [M2 continuation — Sliding doors](../implementation/reports/m2_doors.md) | Original Door, translating geometry, atomic pushing/riders, inspector and validation |
| [M2 continuation — Specialized volumes](../implementation/reports/m2_specialized_volumes.md) | Conveyor/Damage/FreeFall, original damage protection and inspection |
| [M2 continuation — Original MoveMgr](../implementation/reports/m2_movement.md) | Walking/running, jump/gravity, collisions/support, typed movement and in-game inspection |
| [M2 continuation — Player locomotion](../implementation/reports/m2_locomotion.md) | Stairs, crouch clearance, original body animation/fallback, live pose inspection and namespace/style migration |
| [M2 continuation — Containers, liquids and ladders](../implementation/reports/m2_containers.md) | Original volumes, real shell queries, water/ladder movement, air/damage, inspection and validation |
| [M2 continuation — Inventory and weapons](../implementation/reports/m2_weapons.md) | Original slots/ammo, Beretta/knife, animation keys, typed shots/damage, Weapons inspection and verification |
| [M2 continuation — Model/motion services](../implementation/reports/m2_services.md) | Original WorldProperties/SmellHint, ABC animation/deformation, collision and inspector |
| [M2 continuation — Original player entry](../implementation/reports/m2_player.md) | selected shell/CPlayerObj lifecycle, typed clients, Session inspection, tests and open gameplay gate |
| [M2 — World integration, in progress](../implementation/reports/m2.md) | Bounded REZ/DAT readers, actual original world objects, inspector, tests and unresolved session gate |
| [M1 — Original-code boundary](../implementation/reports/m1.md) | Typed original objects, lifetime/service contracts, inspector capture, GCC/Clang and sanitizer evidence |
| [M0 — New project foundation](../implementation/reports/m0.md) | Build/run commands, Debug and Release tests, GPU inspector capture, dependency/source snapshot and platform limits |

## Phase 2: Technical Audit

| Document | Contents |
| --- | --- |
| [Audit overview](Audit/readme.md) | Main conclusions and verified outcomes |
| [Builds and executed checks](Audit/01_build_verification.md) | Compilation, test execution, diagnostic overrides and limitations |
| [Original code and engine contracts](Audit/02_original_code_and_contracts.md) | Original-game integration, lifecycle, interface and service findings |
| [Formats and animation](Audit/03_formats_and_animation.md) | Readers, coordinates, robustness and animation/gameplay connections |
| [Prototype reuse and portability](Audit/04_prototypes_and_portability.md) | Reusable work, platform constraints and unmeasured performance costs |
| [Findings and coverage](Audit/05_findings_and_coverage.md) | Severity register, evidence boundaries and unresolved questions |
| [Execution evidence](Audit/evidence/readme.md) | Logs, commands, dependency commits and source-integrity verification |

## Phase 1: Inventory Baseline

Snapshot date: **15 October 2026**. The phase-1 baseline maps the available projects, systems, dependencies, tools, and reference material for rebuilding Blood 2 through reuse of its original code. Its counts and original scope are retained below. Technical suitability and behavioral findings are documented in the phase-2 audit; the proposed architecture and implementation milestones are in the phase-3 roadmap.

The phase-1 snapshot contains **six independent Git repositories, 11,455 tracked files, and 10,588 files classified as code**. Those code files contain **3,343,227 physical lines**, including comments, blank lines, third-party code, and duplicate copies. These are inventory counts, not a measure of implementation completeness. All six repositories had no tracked changes or nonignored untracked files at the snapshot.

## Read the Inventory

| Document | Contents |
| --- | --- |
| [Workspace and source trees](01_workspace.md) | Repository identities, source provenance, directory responsibilities, and overlapping copies |
| [Systems and entry points](02_systems.md) | Where the original game behavior and corresponding prototype systems live |
| [Builds and dependencies](03_builds_and_dependencies.md) | Declared targets, dependency revisions, build options, and platform branches |
| [Tools, tests, and existing documentation](04_tools_tests_and_docs.md) | Inspection tools, test entry points, documentation map, and historical plans |
| [Coverage and audit handoff](05_coverage.md) | What was enumerated, searched, and read; exclusions and questions for phase 2 |
| [Repository and module counts](Inventory/counts.md) | Generated counts for every indexed module |
| [Existing documentation index](Inventory/documentation.md) | Generated catalog of 172 documentation files |

## Machine Readable Records

- [workspace.json](Inventory/workspace.json): exact Git commits, repository status, classification rules, extension counts, and totals.
- [files.jsonl](Inventory/files.jsonl): one record for each of **10,923 indexed text files**, with workspace-relative path, repository, module, category, byte size, physical line count, and SHA-256 hash. Use this to locate files or detect later changes; it is not a claim of manual code review.
- [source_copies.json](Inventory/source_copies.json): path-based comparisons of the Blood 2 source copies and the two C++ tool directories, including line-ending normalization.

Original game archives and their contents were not opened or copied. Binary assets are represented only in aggregate counts. The index also omits asset configuration data, editor metadata, ignored files, and symlink contents. The two root documents and this `Documentation/` directory are outside the six repositories and outside these counts.

## Main Structural Findings

- **OpenBlood2 already integrates adapted original game code statically.** Its client and server compatibility interfaces remain part of the implementation.
- **NoMadEngine and nomad contain separate gameplay implementations.** Their format readers, runtime systems, and tools are cataloged independently from original source reuse.
- **The two reference Blood 2 code trees have identical text after line-ending normalization.** All 606 matching code paths in `sources/blood2/` and `sources/lithtech/Blood2/` match after replacing CRLF with LF.
- **The larger LithTech tree includes other engine and game generations.** Its NOLF, NOLF2, FEAR, Shogo, tools, and libraries are included in the inventory, without assuming that their APIs are interchangeable with Blood 2's.
- **Build declarations and test code were not executed during phase 1.** Phase 2 subsequently performed the [builds and checks documented here](Audit/01_build_verification.md). No prototype is marked as a complete game on the strength of either source inventory or successful compilation alone.

## Refresh the Generated Records

From the workspace root:

```sh
python3 Documentation/tools/inventory_workspace.py --date 2026-10-15
```

Use the date of the new inventory, or omit `--date` to use the current UTC date. The script uses Python's standard library and Git, reads tracked files, and rewrites only the generated files under `Documentation/Inventory/`. It does not fetch dependencies, configure projects, run game code, or update the authored documents. After refreshing, update the dated prose and counts in this overview and the other authored documents as needed.

The current project direction remains defined by the root [README.md](../../../README.md) and [AGENTS.md](../../../AGENTS.md).

The [animated model continuation](../implementation/reports/m2_model_view.md) adds real ABC/held/first-person weapon drawing, original view offsets and a corrected legacy node-rotation default. Hidden/missing objects remain inspectable. Required NPCs, transitions and client FX still block M2 completion.

The [AI command/hearing continuation](../implementation/reports/m2_ai_triggers.md) adds original trigger commands,
two-faction broadcasts and player sound alerts. State selection is distinct from
executing missing NPC actions; production classes, required FX and ExitTrigger
still block the actual M2 gate.

The [original CMovement continuation](../implementation/reports/m2_ai_navigation.md) adds checked transform caching and
source obstacle-path calculation with inspection. Partial routes remain failures;
no actor following or production NPC registration is implied. Required classes,
AI update/actions and services still keep the M2 gate open.

The [script movement continuation](../implementation/reports/m2_ai_follow.md) connects original Script_Walk/Run,
FOLLOWPATH and MOVETOOBJECT helpers, owned arrival actions and velocity commands.
Read [contracts](../contracts/ai_follow_contracts.md). Preserve cached facing timing, initial injury slowdown,
one-point arrival ordering, linked-target lifetime and residual velocity. Failed
partial plans and missing opportunity-fire branches must remain failures. Full
SCRIPT dispatch, production initialization/AI_Update/deactivation, required FX and
ExitTrigger keep M2 incomplete; source-class counts are unchanged.

The [AI script continuation](../implementation/reports/m2_ai_script.md) connects original command parsing/preparation,
loop/nonloop sequencing, waits, animation, sound and movement state dispatch.
Read [contracts](../contracts/ai_script_contracts.md). Preserve one-command cadence, source atoi-prefix rules,
PLAYANIMATIONLOOPING fallthrough, double DONE decisions and owned callback lifetime.
Production SCRIPT entry still requires its unported forced AI_Update; diagnostics
must not count as registered NPCs. Required initialization/update/death, client FX
and ExitTrigger/session services keep M2 incomplete. Source counts are unchanged.

The [energy projectile continuation](../implementation/reports/m2_energy_projectiles.md) adds original moving flare projectiles, deferred explosion damage, checked shooter attribution and real sprite/WAV presentation. This is a Gideon prerequisite; its remaining weapons/state lifecycle, missing FX and safe NEW_LEVEL keep M2 incomplete.

The [wind weapon continuation](../implementation/reports/m2_wind_weapon.md) connects the original one-shot shockwave, full-radius damage and actual model/WAV presentation. The repaired ground query and checked callback lifetime are explicit. Gideon state/shield integration, required services and safe NEW_LEVEL keep M2 incomplete.

The [shield/powerup dependency](../implementation/reports/m2_shield_weapon.md) adds the original visual-only shield weapon, actual WAV and owner-bound sphere/ring particles with inspection. Armor remains the responsibility of the pending boss action. Gideon, required services and safe NEW_LEVEL still keep M2 incomplete.

The [Gideon combat continuation](../implementation/reports/m2_gideon_combat.md) connects original decision thresholds, shield armor, animation-driven fire and extra-key/node aiming. It also fixes sparse NPC weapon-ID initialization. Production boss lifecycle/fade/teleport, required services and safe NEW_LEVEL remain open; M2 is incomplete.

The [Gideon attack continuation](../implementation/reports/m2_gideon_attacks.md) connects original close/far sequences, one-second dodges, Bold taunt and immediate Special1 recoil, with inspection. Cached transforms, retained lateral velocity and checked failures are preserved. Production boss initialization/health/damage/shared-table ownership, required FX and safe NEW_LEVEL keep M2 incomplete.

The [original impact-spark continuation](../implementation/reports/m2_sparks.md) adds flesh impact bursts and metal sparks with checked detached lifetime and inspection. M2 remains incomplete; this supersedes earlier blanket spark omissions only for these recipes.

The [compact HUD and numeric weapon continuation](../implementation/reports/m2_hud.md) adds actual health/armor/current ammunition and original owned-slot selection with 1..9/0, including inspector-free rendering. Original PCX artwork and remaining required services are still pending; M2 remains incomplete. The evidence also records the next user-reported two-level playtest checks without treating hypotheses as diagnoses.

The [original pickup-audio continuation](../implementation/reports/m2_pickup_audio.md) connects accepted collection to detached spatial WAV playback, including keys, with durable inspection and source-independent lifetime. M2 remains incomplete. The evidence distinguishes successful diagnostic post-transfer firing from unresolved user reports and records severe post-transition stutter as the next performance investigation.

The [collision traversal continuation](../implementation/reports/m2_collision_traversal.md) reduces measured post-transition activation-query work without changing original AI cadence or triangle contacts. Services exposes primary-world traversal counters. Sampled before/after equivalence and NVIDIA/lifetime checks are recorded; CPU skinning and other frame costs remain explicit, and M2 stays incomplete.

The [bound-container index](../implementation/reports/m2_container_index.md) reduces repeated post-transition object scans while retaining fresh checked snapshots, original volume order and deferred destruction. Read [contracts](../contracts/container_index_contracts.md); never mutate a bound shape outside Objects. Container inspection includes the index count. CPU model preparation and other playtest reports remain; M2 stays incomplete.

The [model preparation optimization](../implementation/reports/m2_model_preparation.md) reuses exact local geometry for immutable shared ABC assets and removes temporary heap pose scratch. Read [contracts](../contracts/model_preparation_contracts.md); preserve weak ownership, current visibility/materials, packed-deformation order and world-revision clearing. Inspection reports actual reuse and retained vertex bytes. Skinning remains on the CPU; remaining frame costs and the M2 exit gate stay open.

The [NPC animation-clearance correction](../implementation/reports/m2_ai_animation_clearance.md) restores original clip continuation after ordinary collision refusal, retains safe physical dimensions in the AI cache and exposes requested/actual extents. A reproduced SubwayA suspension and subsequent ordinary death are checked; broader idle-NPC reports and M2 remain open.

The [original PolyGrid continuation](../implementation/reports/m2_polygrids.md) connects VolumeBrush surface creation, original high-detail plasma/scale rules and actual transparent SDL_GPU rendering. Read [contracts](../contracts/polygrid_contracts.md). Preserve independent publisher lifetime, Current.y pan, hidden-clock pause, shared per-world phases, checked resource failures and revision-owned caches. Visual publishers are excluded from replacement body physics; static containers, explicit always-update timing and separate alpha sorting remain bounded policies. CityHub water now renders; underwater fog, moving volumes and the M2 gate stay open.

The [NPC ammunition continuation](../implementation/reports/m2_ai_ammo.md) connects original FindAmmo and MID_ADDAMMO instead of suspending an empty-ammo actor. Read [contracts](../contracts/ai_ammo_contracts.md). Preserve original class selection, ray-hit visibility, empty-route escape, single integration and capped collection. Specific+AvoidTarget skips unused undefined arithmetic; removed pickup trackers retain only their owned route. Private contact/hidden-inspector tests do not prove autonomous retail pickup pursuit or resolve every idle-NPC report. M2 remains incomplete.

The [service coverage continuation](../implementation/reports/m2_service_coverage.md) adds a reviewed bounded adapter catalogue, restored-player/exit/command tracing and retained teardown failures. Read [contracts](../contracts/service_coverage_contracts.md). Implementation status and observed attempts remain separate; unexercised services and the actual M2 gate stay open.

The [authored exit-fade continuation](../implementation/reports/m2_screen_fade.md) connects original player fade commands and validates the actual train exit relay through contact, combat and NEW_LEVEL. Its final explosions exposed a measured smoke-pool limit, now bounded to support the original emission. Unsupported player commands remain visible; diagnostic relocation is not a full playthrough and M2 stays incomplete.

The [continuous train/message-lifetime evidence](../implementation/reports/m2_message_lifetime.md) records spawn-to-CityHub traversal at 30/60 Hz, bounded live messages and the pinned SDL stale-property shutdown correction. M2 remains incomplete.

The [original keyframe-audio continuation](../implementation/reports/m2_keyframe_audio.md) connects retained sounds on controlled objects, including authored Train Station playback. The catalog keeps three missing sound references explicit; M2 remains incomplete.

The [object snapshot optimization](../implementation/reports/m2_object_snapshots.md) reduces repeated occupied-slot scans while retaining owned copies, callback membership, generations and slot order. The original AI clearance fixture and its sanitizer deadline remain unchanged; M2 stays incomplete.
