# Blood2 Project Instructions

Implementation notes below are chronological snapshots: later linked evidence
supersedes older missing-service statements. The current continuous SubwayA route
reaches CityHub; consolidated M2 acceptance is still incomplete.

Follow the workspace [AGENTS.md](../AGENTS.md) and the [roadmap](../Documentation/Roadmap/readme.md).

Documentation starts at [docs/readme.md](docs/readme.md). Keep general developer
guides at the docs root, subsystem references in `docs/contracts/`, and import
history/source ledgers in `docs/implementation/`. Progress reports and measured
results stay in the dedicated workspace `Documentation/Implementation/` and
`evidence/` directories; the implementation index links them. Preserve historical
evidence bytes and update maintained links when moving documentation.

- This is the new implementation repository. Earlier projects are optional donors, never build dependencies.
- Keep the three project targets small: `engine`, `blood2_game`, `blood2_app`. M1 links original `B2BaseClass`, `CameraSpot` and `Key`; M2 adds `GameStartPoint`, `PathPoint` and selected trigger/load-command code. The service continuation adds original WorldProperties/SmellHint, ABC playback/deformation and primary-world swept collision. M2 is in progress, not a working original game session. Read [docs/contracts/object_contracts.md](docs/contracts/object_contracts.md) before adding classes/services; extend the typed subset and tests together.
- Use plain objects/functions and explicit return values. No EnTT/ECS or manual interface/vtable reconstruction.
- Dependency revisions and archive hashes live in `cmake/Dependencies.cmake`. Resolve the latest SDL `main` when updating SDL and retain the exact SHA for repeatability.
- Read [docs/contracts/world_contracts.md](docs/contracts/world_contracts.md) before extending readers or session loading. Preserve exact-path mount precedence, bounded failures and source-entry accounting. Keep opaque format sections and missing required services explicit; the M2 exit gate remains open.
- Read [docs/contracts/model_and_collision_contracts.md](docs/contracts/model_and_collision_contracts.md) before extending animation/physics. The Caleb inspection preview is not CPlayerObj; do not mark full session startup complete on that basis.
- Keep commercial resources external. Local paths belong in ignored `blood2.local.ini`/`CMakeUserPresets.json` or command-line options.
- Build/test with the presets in README. GPU tests need a real display and must be explicitly enabled. Report headless, private-data and graphics results separately.
- Test the inspector-free Release build when changing application/platform lifetime or UI integration. Keep inspection data truthful; detected archives are not loaded resources.
- Record selective imports in `docs/implementation/provenance.md`; preserve original reference trees.
- Keep the original object and aggregate callback chain intact. Never replace `SetNextUpdate` with callback-return timing or destroy an object inside its callback. Run the object contract suite and Clang sanitizers for lifetime/dispatch changes.

- The [player continuation](../Documentation/Implementation/m2_player.md) connects selected original shell/CPlayerObj entry branches. Read [docs/contracts/player_contracts.md](docs/contracts/player_contracts.md) before extending them. The classes are partial source ports: preserve the documented omissions and do not describe entry inspection as complete player gameplay. Keep client association handle-checked, messages owned and bounded, and removal/rollback independent of the UI.

- Read [docs/contracts/weapon_contracts.md](docs/contracts/weapon_contracts.md) before extending inventory, client weapon input/animation, ray fire or damage. Beretta/knife are selective original ports; preserve every FIRE_KEY event (including the two primary knife markers), owned weapon/client models and deferred hit-callback removal. Other weapons, pickups, full combat, audio/FX and muzzle/hand placement are still open. Keep typed shot/damage messages separate from legacy packet ABI; portable seeded randomness is not claimed retail-bit-identical.

- Read [docs/contracts/movement_contracts.md](docs/contracts/movement_contracts.md) before extending original MoveMgr, player input or support queries. Client-associated players use one shared body and skip generic physics; typed movement must resolve collision exactly once. Keep source rules separate from the documented replacement integrator. Read [docs/contracts/locomotion_contracts.md](docs/contracts/locomotion_contracts.md) for current stairs, crouch/animation dimensions, source clip fallbacks and clearance. Selected containers are covered by the contracts below; translating platforms are covered by the door contracts below; bounded rotating transport is covered by the interaction contracts below.

- Follow `.clang-format` and `.editorconfig`. Use `OpenBlood2` for project-owned C++ namespaces; preserve source-global SDK/game classes. Commit code and English documentation after each completed run, in their respective independent repositories.
- Locomotion shape changes are atomic and foot-anchored. A blocked standing attempt preserves the short body and camera; this also applies to alive entry resets. Preserve original `CheckAnims` fallback and expose resolved clips. Keep the new stair solver explicitly separate from verified retail behavior.

- Read [docs/contracts/container_contracts.md](docs/contracts/container_contracts.md) before extending
  container creation, geometry, liquids, ladders or air/damage. Preserve closed
  shape validation, DAT base-ring metadata, head/body separation and four body
  records. Static containers reject unsupported transforms; removal invalidates
  queries immediately. Player motion must not receive generic volume integration
  a second time. Keep pending effects, AI and specialized volumes visible; keys are extended by the trigger contracts below.

- Read [docs/contracts/specialized_volume_contracts.md](docs/contracts/specialized_volume_contracts.md)
  for Conveyor/Damage/FreeFall and level-entry volume damage protection. Preserve
  source client/server conveyor differences, one player integration, authored
  numeric damage types, and the level clock across delayed entry/reload. Keep
  ambiguous/missing model bindings explicit; never select an arbitrary model.

- Read [docs/contracts/door_contracts.md](docs/contracts/door_contracts.md) before extending movers,
  door triggers or collision. Translating Door is a selective original port;
  its atomic pusher/rider solver is replacement engine code. Preserve checked
  named geometry, actual bounds centers, atomic blocking, one shared player
  integration and deferred touch removal. Rotation is extended by the interaction contracts below; crushing, audio/portals and the complete M2 gate remain explicitly pending.

- Read [docs/contracts/trigger_contracts.md](docs/contracts/trigger_contracts.md) before extending
  Trigger/BigTrigger/ToggleTrigger, door SelfTriggers or key inventory/pickups.
  Preserve the original active-command guard, player-only lock query, consuming
  key uses, named-filter order, delayed-toggle behavior and typed message ownership.
  SelfTriggers are owned, stationary children; pending parents cannot activate them.
  Player USE ray/input is extended below; generic pickups, audio/localized feedback
  and save/restore remain pending. Keep source-entry counts distinct from generated runtime children.

- Read [docs/contracts/interaction_contracts.md](docs/contracts/interaction_contracts.md) before extending
  player USE, rotating doors or collision. Preserve the 82-unit eye ray and
  direct-Door exclusion, press-edge input, source double angular increment and
  StartOpen behavior. Keep the authored shell immutable, transforms/body moves
  atomic, player push velocity consumed once and contact removal deferred.
  Conservative bounds/work limits must remain visible; this is not verified
  retail physics. Other movers, crushing and complete gameplay remain pending.

The latest [brush evidence](../Documentation/Implementation/m2_brushes.md) connects original RotatingBrush/Rotating
and selected CDestructableBrush damage/removal. Read [brush contracts](docs/contracts/brush_contracts.md) before extending
rotators, shared CDestructable, brush destruction or debris. Preserve source
zero-based initial angles, elapsed-time restart, per-axis ramps and distinct
CanDamage/Destructable gates. Keep atomic motion, reentrant damage guards and
deferred removal. CDebris/WorldModelDebris remain unported; diagnostic requests
are not FX. Pushable/gravity, explosive and SpawnObject brushes reject creation.
SubwayA now has 141 created source entries, 7 owned SelfTriggers, 372 editor-only
and 67 unported entries. Precreate rejection is a failure, not difficulty
filtering. Other movers and complete gameplay remain pending; M2 is open.

The latest [explosion evidence](../Documentation/Implementation/m2_explosions.md) connects original CDebris
properties/requests and explosive brush area damage. Read
[explosion contracts](docs/contracts/explosion_contracts.md) before extending debris, explosions or
radius damage. Preserve source squared-distance falloff, no visibility filter,
fixed effective 100 damage / 200 radius, multi-material velocity sign alternation,
checked snapshot order and deferred chain/removal. Reports count deliveries,
not health changes. FX/audio records are not rendered or played. WorldModelDebris,
pushable/gravity brushes, SpawnObject and full gameplay remain pending. The earlier
brush paragraph's Exploding rejection/CDebris omission is superseded; M2 stays open.

The latest [pushable evidence](../Documentation/Implementation/m2_pushables.md) connects selected original
CDestructableBrush Pushable/Gravity, mass/friction, brush damage impulses and
CPlayerObj ground/air priorities. Read [pushable contracts](docs/contracts/pushable_contracts.md) before
extending dynamic world bodies, pushing or damage physics. Preserve actual bounds
centers, one player integration, original DONTFOLLOWSTANDING, atomic rider/pusher
commit and deferred callbacks. BoxPhysics is required; polygon dynamics, stacks,
SpawnObject, player impulses and complete gameplay remain pending. Earlier blanket
pushable/gravity omissions are superseded for this bounded subset; M2 stays open.

The latest [pickup evidence](../Documentation/Implementation/m2_pickups.md) connects
eight original ammo pickups, four health/armor pickups and destruction SpawnObject.
Read [pickup contracts](docs/contracts/pickup_contracts.md). Preserve checked typed acknowledgements,
normal/bonus caps, dead-player guards, independent child lifetime, death-trigger
ordering and deferred removal. Earlier blanket SpawnObject/pickup omissions are
superseded for this subset. Remaining inventory items, timed bonuses, old TypeAction
schemas, audio/FX and full M2 gameplay remain pending.

The latest [objective evidence](../Documentation/Implementation/m2_objectives.md) connects original ObjectivesTrigger,
typed client delivery and external cres.dll strings. Read [objective contracts](docs/contracts/objectives_contracts.md)
before extending objectives, localization or client broadcasts. Preserve the
nonempty authored-string guard, resource-ID precedence, original game-mode filter,
owned bounded queue and explicit unresolved state. The PE reader executes no code;
ambiguous languages and malformed resources fail. SubwayA now has 142 created
source entries, 7 owned SelfTriggers, 372 editor-only and 66 unported. HUD/audio,
transitions and complete M2 acceptance remain pending.

The latest [camera evidence](../Documentation/Implementation/m2_cameras.md) connects original CameraObj activation,
tracking and typed client camera selection, including movement/USE/fire gates.
Read [camera contracts](docs/contracts/camera_contracts.md) before extending camera modes, cinematics or
player restrictions. Preserve source/client listener differences, strict timeout,
unlink scheduling and already-started FIRE_KEY handling. Presentation/listener
requests are not cinematic rendering/audio. SubwayA now has 144 created source
entries, 7 owned SelfTriggers, 372 editor-only and 64 unported entries. KeyFramer,
CinematicTrigger, transitions and the actual M2 gate remain open.

The latest [keyframe evidence](../Documentation/Implementation/m2_keyframers.md) connects original KeyFramer/KeyData timelines,
camera/ordinary-object motion and consumed-key ownership. Read [keyframe contracts](docs/contracts/keyframer_contracts.md)
before extending scripted paths, linked-camera rotation or controller callbacks.
Preserve first-update initialization, ignored early ON, component rotation offsets,
loop overshoot discard and the source's missing final snap. Keep target offsets paired
through removal and stop reentrant stale updates. World-model/container motion, key
audio and CinematicTrigger remain explicitly unavailable. SubwayA has 148 created
source entries, 7 SelfTriggers, 372 editor-only and 60 unported; nine Keys are consumed
on the first update (155/158 initial instances before/after player, 146/149 afterwards).
M2 remains open; these counts do not imply completed cinematic/gameplay services.

The latest [dynamic light evidence](../Documentation/Implementation/m2_lights.md) connects original ClientLightFX/GlowingLight
and client CLightFX waveform evaluation. Read [light contracts](docs/contracts/light_contracts.md) before
extending lights, SFX publication or their damage/lifetime. Preserve source command
prefixes, scheduling/NONE-wave quirks, color /511, absolute phase and deferred
source-bound lifetime. Client records and inspector overlays are real evaluated
state; world illumination/shadows/fog and audio remain unavailable. DirLight spots
and the implicit main light are separate. SubwayA now has 160 created source entries,
7 SelfTriggers, 372 editor-only and 48 unported (167/170 initial runtime instances,
158/161 after consuming nine Keys). The earlier missing GlowingLight statement is
superseded for this subset; M2 remains open.

The latest [train path evidence](../Documentation/Implementation/m2_worldpaths.md) extends original KeyFramer
with translating BoxPhysics world models. Read [path contracts](docs/contracts/worldpath_contracts.md).
Keep unchanged-rotation validation before mutation, primary-world checks without
GOTHRUWORLD, atomic pusher/rider commit, deferred contacts and single player integration.
SubwayA's diagnostic train run records 297 moves/two primary-world blocks per cycle;
this is replacement physics, not verified retail parity. Angular paths, dynamic
world bodies and containers remain unavailable. Class counts stay 160 created,
372 editor-only and 48 unported. Earlier blanket world-path omissions are superseded
only for this translation subset; M2 stays open.

The latest [cinematic evidence](../Documentation/Implementation/m2_cinematics.md) connects original CinematicTrigger with
real bounded WAV decoding, typed voice completion and dry SDL playback. Read
[cinematic contracts](docs/contracts/cinematic_contracts.md). Preserve any-payload rearming without reset,
message-before-sound order, missing-file retry, checked speaker/owner lifetime and
sample-count clocks independent of UI. Terminal odd-data padding exceptions are
bounded; three missing catalog references remain failures. Reverb is requested but
not rendered. General sound emitters/music and complete M2 acceptance remain open.
SubwayA now has 164 created source entries, 7 SelfTriggers, 372 editor-only and
44 unported (171/174 runtime before/after player, 162/165 after consuming nine Keys).
Earlier CinematicTrigger/audio omissions are superseded only for this subset.

The latest [random sound evidence](../Documentation/Implementation/m2_random_sounds.md) connects original RandomSoundFX and typed fixed-position ambient/spatial voices.
Read [random sound contracts](docs/contracts/random_sound_contracts.md). Preserve case-sensitive prefix commands,
OFF voice tails, first-empty list termination, interval-based replacement, sequence
progression after failed starts, and one release per message read. Ambient attenuates
without panning; sampled voices do not follow later emitter motion. Built-in Sound
loops/streaming and other audio callers remain missing. SubwayA now has 166 created
source entries, 7 SelfTriggers, 372 editor-only and 42 unported (173/176 runtime
before/after player, 164/167 after consuming nine Keys). M2 remains incomplete.

The latest [world sound evidence](../Documentation/Implementation/m2_world_sounds.md) integrates the original built-in Sound
with typed world-owned whole-clip loops. Read [world sound contracts](docs/contracts/world_sound_contracts.md).
Preserve immediate deferred source removal, durable success/failure accounting,
source-independent playback and unload ownership. FileStream/TIMESYNC and region
metadata reject playback; priority arbitration and historical VoiceType schemas
remain unavailable. SubwayA has 166 live-created source entries, 4 transferred
world sounds, 7 SelfTriggers, 372 editor-only and 38 unported entries. Runtime
counts remain 173/176 before/after player, 164/167 after nine Keys are consumed.
Earlier blanket Sound-loop omissions are superseded only for this subset; M2 stays open.

The latest [sprite evidence](../Documentation/Implementation/m2_sprites.md) integrates original DetailSprite with bounded
SPR manifests, frame clocks, flush placement and independent runtime repeats.
Read [sprite contracts](docs/contracts/sprite_contracts.md). Preserve the source's unchanged rotation,
flush-only repeat branch, column order and post-repeat difficulty filtering.
DTX resources are indexed, not decoded or rendered; the overlay is diagnostic.
SubwayA has 178 live-created entries, 4 transferred world sounds, 7 SelfTriggers,
372 editor-only and 26 unported entries, with no failures. Runtime counts are
185/188 before/after player entry, then 176/179 after nine Keys are consumed.
M2 remains incomplete; earlier DetailSprite omissions are superseded only for
this bounded subset.

The latest [scene explosion evidence](../Documentation/Implementation/m2_scene_explosions.md) integrates original CClientExplosionSFX
activation/removal and detached one-shot WAV playback. Read [scene explosion contracts](docs/contracts/scene_explosion_contracts.md).
Preserve the original null-damager guard (zero deliveries), cached initial position,
retrigger-before-update behavior and source-independent voice tails. Reports explicitly
mark client FX unavailable; they are not successful FX services. SubwayA now has 187
live-created entries, 4 transferred sounds, 372 editor-only and 17 unported, with no
failures; runtime counts are 194/197 before/after player, then 185/188 after nine Keys.
Required client FX, AI, ExitTrigger and other services keep M2 incomplete.

The latest [character animation evidence](../Documentation/Implementation/m2_character_animation.md) connects the full original
CAnim_Sound animation table to player locomotion, current world-space model node
queries and original hit-sphere measurements. Read [character contracts](docs/contracts/character_animation_contracts.md).
Preserve the >999 previous-slot fallback, untouched slot zero, strafe aliases,
humiliation spelling/order and intermediate-zero ForceAnimation behavior.
Missing head nodes fail explicitly; later-node fallback stays visible. Spheres
are measurements, not active damage/collision services. AI, ExitTrigger, client
FX and complete M2 remain open; SubwayA counts are unchanged.

The latest [character audio evidence](../Documentation/Implementation/m2_character_audio.md) connects original CAnim_Sound
voice helpers, player model-key audio and surface/liquid footsteps to real WAV
playback. Read [audio contracts](docs/contracts/character_audio_contracts.md). Preserve busy-voice refusal,
player interrupt-before-failure, source 3D speed threshold and left/right alternation,
raw first-container liquid precedence and distinct baked material flags. Typed
movement keeps stale surface messages rejected. AI sound alerts remain explicitly
unavailable; no new AI class or complete M2 gate is claimed.

The latest [character motion evidence](../Documentation/Implementation/m2_character_motion.md) connects original BaseCharacter model-key motion
and bounded Sparam lookup. Read [motion-key contracts](docs/contracts/character_motion_contracts.md). Preserve local-up yaw,
source overwrite/order, world-space AddXYZ, centered raw dimensions and the player's
base-handler exclusion. Validate complete numeric groups before mutation; stop on
service failure and preserve deferred touch removal. The seven examined character
assets contain no motion keys; synthetic probes are explicitly labelled, not AI.
AI states/hearing/fire, ExitTrigger, client FX and the actual M2 gate remain open.

The latest [AI model-key evidence](../Documentation/Implementation/m2_ai_model_keys.md) connects selected AI_Mgr animation-key
sound dispatch, actual decoded voices and inspection. Read [contracts](docs/contracts/ai_model_key_contracts.md).
Preserve source branch priority, server-camera/script early returns, B2_LITE's
1..3 variants, chance draw ordering and class/gender shared voice lifetime.
Fire/show_weapon/extra remain unavailable; diagnostic probes are not registered
world AI. CivilianAI, CultistAI, Gideon, ExitTrigger and the actual M2 gate stay open.

The latest [AI path-manager evidence](../Documentation/Implementation/m2_path_manager.md) connects original PathMgr selection
and owned snapshots to shell lifetime and in-game inspection. Read [contracts](docs/contracts/path_manager_contracts.md).
Preserve active-only GetNextObject, included SMELL points, unconsumed source objects,
prefix-plus-first-digit matching, reverse enumeration and preflight/no-match output
semantics. Returned paths own their data; source handles are inspection-only.
AI following/actions, remaining classes/services and the M2 gate stay open.

The latest [AI visibility evidence](../Documentation/Implementation/m2_ai_sight.md) connects original AI_Mgr::CanSee
and its VolumeBrush/destructible-glass filter. Read [contracts](docs/contracts/ai_sight_contracts.md). Preserve
body-first rays, head retry only after an obstruction, target-hit requirements and
checked lifetime. Current head poses and hidden/reload inspection are exercised;
range/FOV, target acquisition, states, attacks and production NPCs remain unported.
The 17 unsupported SubwayA entries and actual M2 gate remain unchanged.

The latest [held-model/attachment evidence](../Documentation/Implementation/m2_attachments.md) connects original CWeapon held models,
selected inventory visibility and typed parent/node transforms. Read [contracts](docs/contracts/attachment_contracts.md).
Preserve original gun-node/origin fallback, handheld clip, independent child removal,
non-reused IDs and deferred ownership cleanup. Visual children only: chains, physical
children, dropped weapons and client visibility SFX remain unavailable. Hidden/reload
inspection runs outside UI; required AI/ExitTrigger/FX keep M2 incomplete.

The latest [explicit AI target evidence](../Documentation/Implementation/m2_ai_targets.md) connects original named/link selection,
stored-position facing and target MID_LINKBROKEN. Read [contracts](docs/contracts/ai_target_contracts.md).
Preserve same-target no-op, SetTarget's net-zero Metacmd change, local-up yaw,
deferred non-owning target lifetime and checked pose failures. Explicit commands
are not autonomous acquisition or an AI state machine. Source counts and the
actual M2 gate remain unchanged; NPCs, ExitTrigger and required FX remain open.

The [diagnostic 3D world view](../Documentation/Implementation/m2_world_view.md) now renders actual DAT geometry and
current world-model transforms through SDL_GPU. Read [view contracts](docs/contracts/world_view_contracts.md)
before extending rendering, camera presentation or GPU lifetime. Preserve original
coordinates, base rings, current checked visibility, world revisions and the single
player integrator. `--view-world` is navigable inspection, not the M3 playable slice;
texture/model/FX rendering and required NPC/ExitTrigger services remain unavailable. Current
world shaders now bundle Vulkan/SPIR-V and Metal/MSL (macOS execution user-confirmed; native automated tests pending); M2 stays incomplete.

The [DTX world-texture continuation](../Documentation/Implementation/m2_textures.md) adds bounded version -2 palette decoding, exact-path material caching and opaque SDL_GPU sampling. Read [texture contracts](docs/contracts/texture_contracts.md) before extending images/materials. Preserve the -2/-5 generation boundary, strict lengths, pre-pivot UVs, world-revision ownership and explicit failed resources. Alpha/sky markers, unlit mip-zero sampling and unsupported version-0 assets remain visible limitations. Models, required original services and the M2 gate remain open.

The [animated model continuation](../Documentation/Implementation/m2_model_view.md) renders checked existing ABC/held/view models with original view offsets and shared world depth. Read [contracts](docs/contracts/model_view_contracts.md) before extending presentation or poses. Preserve read-only clocks, deferred lifetime, hidden/missing inspection, cache revisions and explicit close-view limits. Absent ABC TransformInfo now conjugates normalized node rotations; explicit zero remains direct. Rendering/head queries/attachments share that correction and the file -Z to engine +Z basis conversion; all spatial model fields are normalized once. Lighting/FX, NPCs, ExitTrigger and the actual M2 gate remain open.

The [AI perception continuation](../Documentation/Implementation/m2_ai_perception.md) connects original faction lists, FindObjectInRadius, ComputeStimuli, opportunity selection and ExitHint. Read [contracts](docs/contracts/ai_perception_contracts.md) before extending NPC acquisition/state updates. Preserve player membership in both factions, source list/priority/boundary rules, dead-target behavior and checked deferred lifetime. Required query failure is -1, never a usable stimulus. Attacker memory is a checked non-owning identity; timed-power storage does not implement pickups/timers. No production NPC is registered; states, attacks, required FX and ExitTrigger keep M2 open.

The [AI state/action continuation](../Documentation/Implementation/m2_ai_states.md) adds original transitions, Cultist/Civilian decisions and stationary idle/talk/special commands. Read [contracts](docs/contracts/ai_state_contracts.md). Preserve strict idle deadline, last-state exclusions, idle/talk command sharing, special-action random draws and failed-perception guards. Animation resize uses the documented atomic foot-anchored replacement. Diagnostic dispatch is not AI_Update; production NPCs, combat/navigation/death, required FX and ExitTrigger remain unavailable. M2 stays open.

The [AI weapon-action continuation](../Documentation/Implementation/m2_ai_fire.md) connects original weapon choice, supported aiming/clearance, standing fire and fire_key delivery. Read [contracts](docs/contracts/ai_fire_contracts.md). Preserve raw-ammo selection, strict cadence/range, initial-miss versus head-retry behavior, cached-head writes, dry acceptance and independent completion increments. Player eye alignment must remain player-only. Missing switch columns/services fail; diagnostic firing does not register production NPCs or implement AI_Update. Navigation/death, required FX and ExitTrigger still keep M2 open.

The [AI command/hearing continuation](../Documentation/Implementation/m2_ai_triggers.md) connects selected original HandleTrigger,
PlayAnimation, SendSoundTrigger and player sound alerts. Read [contracts](docs/contracts/ai_trigger_contracts.md). Preserve
source command early returns, exact hearing boundaries, reversed gunfire class test,
impact guards, two-faction order/duplicates and alert-after-playback-failure. Snapshot
checked identities across callbacks; delivery is not reaction. Scripts and full NPC
initialization/AI_Update/deactivation remain unavailable; M2 stays open.

The [CMovement continuation](../Documentation/Implementation/m2_ai_navigation.md) adds the original cached transform aggregate
and obstacle-path calculation. Read [contracts](docs/contracts/ai_navigation_contracts.md). Preserve two side rays,
character-only filtering, zero lateral range on misses, source offsets/partial paths
and accumulated point count. Planning never integrates motion; players do not initialize
this cache. Bounded searches and value ownership are adaptations. Full NPC initialization,
following/actions, shade queries and the actual M2 gate remain open.

The [script movement continuation](../Documentation/Implementation/m2_ai_follow.md) connects original Script_Walk/Run,
FOLLOWPATH and MOVETOOBJECT helpers, owned arrival actions and velocity commands.
Read [contracts](docs/contracts/ai_follow_contracts.md). Preserve cached facing timing, initial injury slowdown,
one-point arrival ordering, linked-target lifetime and residual velocity. Failed
partial plans and missing opportunity-fire branches must remain failures. Full
SCRIPT dispatch, production initialization/AI_Update/deactivation, required FX and
ExitTrigger keep M2 incomplete; source-class counts are unchanged.

The [AI script continuation](../Documentation/Implementation/m2_ai_script.md) connects original command parsing/preparation,
loop/nonloop sequencing, waits, animation, sound and movement state dispatch.
Read [contracts](docs/contracts/ai_script_contracts.md). Preserve one-command cadence, source atoi-prefix rules,
PLAYANIMATIONLOOPING fallthrough, double DONE decisions and owned callback lifetime.
Production SCRIPT entry still requires its unported forced AI_Update; diagnostics
must not count as registered NPCs. Required initialization/update/death, client FX
and ExitTrigger/session services keep M2 incomplete. Source counts are unchanged.

The [autonomous locomotion continuation](../Documentation/Implementation/m2_ai_locomotion.md) adds original obstruction/ledge probes,
walk/run/moving-fire commands and Cultist close/far attack sequencing. Read
[contracts](docs/contracts/ai_locomotion_contracts.md). Preserve first-hit self exclusion, false-on-throttled ledge
queries, injury/support timing, independent fire/completion increments and cached
attack thresholds. Missing shade recovery and crawl/jump execution remain explicit
failures. Diagnostic drivers are not production AI_Update; NPC initialization,
remaining states, required FX and ExitTrigger keep M2 incomplete. Counts are unchanged.

The [AI recovery/navigation continuation](../Documentation/Implementation/m2_ai_recovery.md) connects original jump/crouch/crawl,
firing crawl, JumpOver/CrawlUnder, ToPos/Around and visual-search states. Read
[contracts](docs/contracts/ai_recovery_contracts.md). Preserve landing-based completion, cached movement, crawl's
recovery overwrite, arrival/ledge short circuits and independent fire markers.
Failed plans cannot be followed until replanning succeeds. Earlier crawl/jump-body
omissions are superseded; shade recovery, smell/escape/recoil/death, full NPC
initialization/AI_Update, required FX and ExitTrigger keep M2 incomplete.

The [AI escape/guard continuation](../Documentation/Implementation/m2_ai_escape.md) connects original Cultist/Civilian
escape/hide, GuardLocation and the lateral beg command. Read [contracts](docs/contracts/ai_escape_contracts.md).
Preserve class-specific hint filters, preceding cached facing, current civilian
dimensions, guard ledge order and residual velocity. Tracking is a checked non-owning
identity, not a new engine link. Diagnostic ray-hit hints do not change authored
marker flags. Earlier escape/guard-body omissions are superseded; smell/recoil/death,
full NPC initialization/AI_Update, required FX and ExitTrigger keep M2 incomplete.

The [production NPC lifecycle continuation](../Documentation/Implementation/m2_ai_runtime.md) registers CultistAI/CivilianAI and connects original initialization/AI_Update.
Read [contracts](docs/contracts/ai_runtime_contracts.md). Preserve class-specific property/table quirks,
source update ordering, deferred child lifetime and independent clocks. Runtime
failures suspend visibly; replacement center-ray activation is not retail PVS.
SubwayA has 202 created source entries, 4 transferred sounds, 372 editor-only,
2 unsupported and no creation failures. Remaining actions/death, Gideon, required
FX and ExitTrigger keep M2 open. User-reported pink train glass and direct movement
failure after intro are retained in the evidence for later investigation.

The [NPC hit/recoil continuation](../Documentation/Implementation/m2_ai_hit.md) connects original visible-node hit localization,
region damage and nonlethal HandleDamage/MC_Recoil through production AI_Update.
Read [contracts](docs/contracts/ai_hit_contracts.md). Preserve NORMAL=0 random fallback including raw node 18,
unwrapped side-angle difference, Cabal head kill before armor, strict limp threshold,
remaining-HP/accumulated-damage recoil threshold and Civilian EscapeRun override.
Missing later nodes may reuse the prior transform; invalid poses must fail before
health mutation. Refused/nested packets cannot replay stale reactions. Lethal
corpse/drop/gib services remain explicit failures; NPC impulses, hidden limbs,
Gideon, ExitTrigger and required FX keep M2 incomplete.

The [NPC weapon-drop continuation](../Documentation/Implementation/m2_weapon_drops.md) connects original Cabal
DropCurrentWeapon, detached hand physics, BerettaPU and player collection.
Read [contracts](docs/contracts/weapon_drop_contracts.md). Preserve left-first selection, melee/busy refusal,
checked independent lifetime, one-second initial support check and ammo-dependent
pickup acceptance. Failed transfers stay inspectable. Player drop controls,
corpses/gibs, required FX, Gideon and ExitTrigger remain unavailable; M2 stays open.

The [inspector/input continuation](../Documentation/Implementation/m2_controls.md) enables keyboard controls by default
and native inspector folding/resizing. Preserve text/focus suppression, F2's
explicit preference, original cinematic gates and single MoveMgr integration.
Folding or hiding UI must not stop the session; M2 remains incomplete.

The [ordinary death continuation](../Documentation/Implementation/m2_corpses.md) connects original Dying/MC_Dead/CCorpse.
Read [contracts](docs/contracts/corpse_contracts.md). Preserve two-call transfer, cached pose, raw clip
dimensions versus visual scale, source damage fractions, independent child lifetime
and no-GETHANDLE sound overlap. Failed setup retains the visible actor. Fire/gibs,
corpse damage/limbs and blood-pool FX remain unavailable; requests are not FX.
SubwayA counts are unchanged and M2 remains incomplete.

The [level-exit continuation](../Documentation/Implementation/m2_exits.md) registers original ExitTrigger and
connects typed shell exit/load requests. Read [contracts](docs/contracts/exit_contracts.md). Preserve
EndEpisode/EndOfEpisode spelling, start-name copy-before-broadcast, delayed virtual
slot rules and dead-player early return. NEW_LEVEL explicitly refuses missing
KeepAliveSave/Load before teardown; do not substitute fresh-game entry or claim a
completed transfer. SubwayA has 203 created, four transferred sounds, 372 editor-only,
one unsupported (Gideon), no failures. Required transfer/FX/services keep M2 open.

The [inventory preservation continuation](../Documentation/Implementation/m2_inventory_state.md) ports selected original
CInventoryMgr Save/Load and owned exit-time capture. Read [contracts](docs/contracts/inventory_state_contracts.md).
Preserve raw ammo after weapon recreation, selected-slot order, keep-alive key
removal, transient reset and checked staged ownership. Load refuses associated
clients; snapshots are inventory only. Player/damage fields, restored client
association and safe destination entry remain pending; NEW_LEVEL and M2 stay open.

The [damage preservation continuation](../Documentation/Implementation/m2_damage_state.md) connects original CDestructable
Save/Load values and death-health/delay/nigh-invulnerability rules. Read [contracts](docs/contracts/damage_state_contracts.md).
Preserve CanDamage's million-HP sentinel, separate signed death health, remaining
duration/overshoot, character-only modifier and destination/list ownership. The
legacy location byte is initialized to zero, not inferred from hit nodes. Captures
are aggregate-only; full player/client/destination transfer and M2 remain open.

The [player field/restore-initialization continuation](../Documentation/Implementation/m2_player_state.md) preserves selected original
CPlayerObj values and the savegame early return. Read [contracts](docs/contracts/player_state_contracts.md). Preserve relative
smell clocks, nullable strings, raw attributes, separate player death and zeroed
stat caches. Restore initialization grants no fresh stats/scent/scheduling. Active
powers refuse missing timer preservation. These are partial fields, not full player
or client/engine restoration; NEW_LEVEL and M2 remain open.

The [node visibility/Load-tail continuation](../Documentation/Implementation/m2_character_nodes.md) connects original named resets,
per-instance rendering/hit masks and selected player model/flags/velocity restoration.
Read [contracts](docs/contracts/character_node_contracts.md). Preserve contiguous extras, missing-name retained status,
sparse node IDs, source random fallback and Load's absent SHADOW flag. Tail failures
may leave partial setup: discard staged owners, never claim active-player rollback.
Engine/client restoration, safe destination entry, required FX and M2 remain open.

The [player restoration continuation](../Documentation/Implementation/m2_player_restore.md) connects supported engine snapshots and original saved-client association.
Read [contracts](docs/contracts/player_restore_contracts.md). Preserve staged cleanup, checked new identities, original Load overrides,
relative scheduling, raw attributes/caps and single MoveMgr integration. First update
must derive speeds without ValidateAttributes. No live-client replacement or start
trigger replay; current-world clearance may reject. Whole-world/client saves,
dead/powered states and safe NEW_LEVEL transfer remain unavailable; M2 stays open.

The [named keep-alive entry continuation](../Documentation/Implementation/m2_player_keepalive.md) connects supported captured players to already loaded destinations. Read [contracts](docs/contracts/player_keepalive_contracts.md).
Preserve key removal, raw stats, case-insensitive solo marker selection/last fallback,
trigger-before-teleport ordering, checked callback lifetime and one MoveMgr integrator.
Failure cleans staged ownership, not destination trigger effects or an unloaded world.
Automatic NEW_LEVEL still refuses before teardown; safe transfer and M2 remain open.

The [sprite presentation continuation](../Documentation/Implementation/m2_sprite_view.md) draws original DetailSprite frames
using actual DTX alpha, current authored/camera rotations and shared world depth.
Read [contracts](docs/contracts/sprite_view_contracts.md). Preserve read-only clocks, disabled chromakey, explicit
unsupported modes, stable depth order and world-revision cache ownership. Missing
textures emit diagnostics, not guessed quads. Required FX and safe NEW_LEVEL still
keep M2 incomplete; source-class counts are unchanged.

The [explosion sprite continuation](../Documentation/Implementation/m2_explosion_sprites.md) connects selected original CExplosionSpriteFX methods and high-detail CExplosionFX sprite recipes.
Read [contracts](docs/contracts/explosion_sprite_contracts.md). Preserve normal offsets, source fade/scale formulas,
1 ms effect cadence, detached lifetime, sibling failures and null-damager behavior.
Per-instance alpha only extends the sprite pass. Reports explicitly retain missing
composite components; these sprites do not complete the FX service or M2 gate.
Safe NEW_LEVEL and other required services remain open; source counts are unchanged.

The [explosion model continuation](../Documentation/Implementation/m2_explosion_models.md) connects original CExplosionModelFX and selected high-detail model recipes.
Read [contracts](docs/contracts/explosion_model_contracts.md). Preserve cached-axis/per-update rotation, wave/fade
formulas, explicit RNG ordering, final-update removal and independent model/sprite
failure/lifetime. Fullbright alpha rendering is bounded diagnostic presentation;
separate model/sprite sorting is not retail parity. Required remaining composite
FX, safe NEW_LEVEL and M2 stay open; world-source counts do not change.

The [explosion point-light continuation](../Documentation/Implementation/m2_explosion_lights.md) connects original CExplosionLightFX and selected high-detail light recipes.
Read [contracts](docs/contracts/explosion_light_contracts.md). Preserve delay overshoot discard, activation return,
linear/final updates, reused zero Radius2 and independent sibling/source lifetime.
These are evaluated lights, not surface illumination or scorch decals. Keep missing
composite/lighting services explicit; safe NEW_LEVEL and M2 remain incomplete.

The [explosion particle-ring continuation](../Documentation/Implementation/m2_explosion_rings.md) connects original ring lifecycle/recipes and actual textured particles.
Read [contracts](docs/contracts/explosion_ring_contracts.md). Preserve cached-right emission, delay return/overshoot, source per-update rotation,
ordered independent RNG and detached ownership. Particle stepping/quad sizing follow
explicit later-engine reference policies, not verified retail parity. Keep missing
fragments/flames/illumination and safe NEW_LEVEL explicit; M2 remains incomplete.

The [explosion fragment continuation](../Documentation/Implementation/m2_explosion_fragments.md) connects original metal launch, bounce/rest and fade with real model rendering.
Read [contracts](docs/contracts/explosion_fragment_contracts.md). Preserve per-update motion, strict rest/fade gates,
1.1-velocity rays, impact angular damping and per-condition random count draws.
Use checked detached lifetime; unavailable collision must fail. Trails/child explosions
are not implemented, and replacement collision is not retail parity. M2 stays open.

The [fragment smoke-trail continuation](../Documentation/Implementation/m2_object_particles.md) connects selected original CObjectParticleFX and real textured particles.
Read [contracts](docs/contracts/object_particle_contracts.md). Preserve first-update return, absolute emission timing, distance-derived count,
unused authored alpha/count, fixed origin, strict stopped-tail lifetime and parent removal.
Zero-count division is skipped without emission/RNG draws. Keep bounded failures and
replacement-engine limits explicit; other ObjectFX modes and M2 remain open.

The [explosion flame continuation](../Documentation/Implementation/m2_explosion_flames.md) connects original three-zone particle emission and the selected Tesla recipe.
Read [contracts](docs/contracts/explosion_flame_contracts.md). Preserve immediate bursts, one overdue emission per callback, additive deadlines,
initial alpha 1 for fades, final emission and unused normal/ramp fields. Bulk sampling
is a documented later-engine policy, not retail RNG parity. Keep bounded failures,
detached lifetime and read-only rendering; illumination, safe NEW_LEVEL and M2 remain open.

The [destination preparation continuation](../Documentation/Implementation/m2_destination.md) adds CPU-only DAT/collision/schema inspection while retaining the active world/player.
Read [contracts](docs/contracts/destination_contracts.md). Preserve separate syntax/execution policy, const schema checks, one final-request scan per pump,
owned report lifetime and callback refusal. Schema success never implies callback/resource readiness or authorizes teardown.
Automatic NEW_LEVEL, required remaining services and M2 remain incomplete.

The [destination initialization continuation](../Documentation/Implementation/m2_destination_trial.md) runs temporary original callbacks and named KeepAlive entry while retaining the source session.
Read [contracts](docs/contracts/destination_trial_contracts.md). Preserve fixed sentinel addresses, all seven list/count pairs, active camera, four shared AI table owners and adapter binding.
Destroy the temporary session under its own adapter before resuming source globals; every new mutable game global needs auditing.
No destination simulation tick, runtime promotion or transfer is implemented. Monotonic audio IDs are not rewound; M2 remains incomplete.

The [weapon fire-audio continuation](../Documentation/Implementation/m2_weapon_audio.md) connects original retained fire sounds and automatic gunfire hearing.
Read [contracts](docs/contracts/weapon_audio_contracts.md). Preserve four slots, first-empty/earliest-end ordering, playback-before-alert even on missing WAV,
all-inventory cleanup and dry-fire silence. Refresh rays after eligible hearing callbacks, defer removal and expose interrupted shots.
Reverb/priority arbitration and other required services remain unavailable; safe NEW_LEVEL and M2 stay open.

The [weapon impact continuation](../Documentation/Implementation/m2_weapon_impacts.md) connects original vector continuation, selected material WAVs and impact hearing.
Read [contracts](docs/contracts/weapon_impact_contracts.md). Preserve source branch order, raw versus stopping material, fixed endpoint, one-unit advance and B2_LITE variants.
Keep the 64-contact bound, explicit partial failures, checked callback lifetime and detached voices. Impact visuals/decals, safe NEW_LEVEL and M2 remain open.

The [weapon impact sprite continuation](../Documentation/Implementation/m2_weapon_sprites.md) connects original wood/stone flashes and smoke with tint, detached lifetime and in-game inspection.
Read [contracts](docs/contracts/weapon_sprite_contracts.md). Preserve first-update/delay quirks, per-callback motion, first-container visibility and independent sibling failures.
Other impact components remain unavailable. User playtest fixes (mouse look/fire, non-attacking NPCs, train glass) take priority over more FX; M2 stays open.

The [keyboard/mouse control fix](../Documentation/Implementation/m2_player_controls.md) connects F/G and captured mouse firing plus relative mouse look to existing original gameplay.
Read [contracts](docs/contracts/player_control_contracts.md). Preserve UI-only uncaptured clicks, one-time delta consumption, focus/player-change resets and original FIRE_KEY/camera gates.
F3 captures, Escape releases; --view-world requests capture initially. Next priorities are non-attacking NPCs and train glass, before further cosmetic FX. M2 remains open.

The [NPC entry correction](../Documentation/Implementation/m2_npc_entry.md) holds interactive simulation until original player entry. Read [contracts](docs/contracts/session_playback_contracts.md). Preserve explicit diagnostic stepping, deferred removal while waiting, original intro/AI rules and visible runtime failures. Tests prove authored Cultist fire/damage after delayed application entry without forced AI targets. Broader combat remains unverified; train glass is next before more FX. M2 stays open.

The [train-window correction](../Documentation/Implementation/m2_world_alpha.md) replaces the intentional pink marker with actual DTX alpha on authored transparent world faces. Read [contracts](docs/contracts/world_alpha_contracts.md) before extending world materials/passes. Preserve opaque failure markers, sky precedence, per-face posed sorting, shared GPU ownership, no alpha depth writes and separate submission inspection. Global translucent FX ordering and retail renderer parity remain unverified; required services, safe NEW_LEVEL and M2 stay open.

The [Naga weapon continuation](../Documentation/Implementation/m2_naga_weapon.md) adds the original eye weapon and red-small model beam. Read [contracts](docs/contracts/naga_weapon_contracts.md). Preserve different visual/damage filters, no-ammo semantics, silent alt, head_gun failure and null held models. Beam lifetime begins on first update and survives source removal; publication failures stay visible. The eye-impact composite and Gideon remain unavailable; M2 stays open.

The [energy projectile continuation](../Documentation/Implementation/m2_energy_projectiles.md) connects original CEnergyBlast and non-Zealot projectile flight, contacts and delayed damage. Read [contracts](docs/contracts/energy_projectile_contracts.md). Preserve separate projectile sender/shooter attribution, null responsibility after unlink, strict expiry, direct-direction mutation and original contact exclusions. Point flight is replacement physics; trail/Tesla-primary composite and Gideon remain unavailable. Keep missing services explicit and M2 open.

The [wind weapon continuation](../Documentation/Implementation/m2_wind_weapon.md) connects original Gideon Wind and CBehemothShockwaveProjectile. Read [contracts](docs/contracts/wind_weapon_contracts.md). Preserve the one-shot override, current-shooter ground query, center-distance450/full damage, owner exclusion/self inclusion, raw damage vectors/target positions and no AREAEFFECT flag. The uninitialized source ray endpoint is repaired explicitly; entry-face collision is replacement policy. Keep callback snapshots/reentrancy guards, independent FX/audio and truthful terminal history. Gideon/shield/powerup, safe NEW_LEVEL and M2 remain open.

The [shield/powerup continuation](../Documentation/Implementation/m2_shield_weapon.md) connects original CGideonShield and owner-bound ObjectFX19/36. Read [contracts](docs/contracts/shield_weapon_contracts.md). Preserve visual-only weapon semantics (armor belongs to Gideon), cached-right emission, local-up offset, per-callback rotations, absolute emission clocks and powerup alpha1 until its final ramp. Checked owner removal ends all particles; keep bounded failures, retained inspection and destination isolation. Gideon initialization/actions/extra-key caller, required services, safe NEW_LEVEL and M2 remain open.

The [Gideon combat continuation](../Documentation/Implementation/m2_gideon_combat.md) connects selected original decisions, shield armor, node firing and extra-key powerup. Read [contracts](docs/contracts/gideon_combat_contracts.md). Preserve strict shield/sight gates, reversed panic inequality, MAGIC column8, absent Soul Hook completion, cached-forward fallback and no timed/generic-clearance fire. Extra keys retain branch priority and checked deferred lifetime. Sparse NPC IDs must never index a player-sized array. No production boss registration/new shared table is claimed; fade/teleport, initialization, required services and safe NEW_LEVEL keep M2 open.

The [Gideon fade/teleport continuation](../Documentation/Implementation/m2_gideon_teleport.md) connects original per-call fade, cached/target-centered rays and collision-aware moves. Read [contracts](docs/contracts/gideon_teleport_contracts.md). Preserve unnormalized directions, unused miss offsets, source SOLID timing, no-GETHANDLE sound semantics and checked contact lifetime. Model alpha is rendered; additive RGB remains lighting metadata and survives supported player restoration. Production boss initialization/health/damage/attack sequencing, required FX and safe NEW_LEVEL keep M2 open.

The [Gideon attack continuation](../Documentation/Implementation/m2_gideon_attacks.md) connects original close/far sequences, timed dodges, Bold taunt and Special1 recoil. Read [contracts](docs/contracts/gideon_attack_contracts.md). Preserve cached distance/basis, entry-only taunt velocity, repeated dodge velocity, elapsed>=1 completion independent of playback, retained lateral motion and checked target lifetime. No production boss registration or new shared global is claimed; initialization/health/damage/table ownership, required FX and safe NEW_LEVEL keep M2 open.

The [Gideon lifecycle continuation](../Documentation/Implementation/m2_gideon_runtime.md) registers original initialization/AI_Update and typed boss-health delivery. Read [contracts](docs/contracts/gideon_runtime_contracts.md). Preserve pre-base health capture, repeated nonzero-ratio publication, gravity removal after base initialization, one accepted typed damage application and subclass-before-base reaction order. All five shared table owners participate in destination rehearsal isolation. Original HUD, complete encounter/death parity, required FX and safe NEW_LEVEL remain incomplete; M2 stays open.

The [NPC activation correction](../Documentation/Implementation/m2_activation_visibility.md) reuses the original AI glass/volume filter for replacement visibility pings and completes Gideon's authored intro script. Read [contracts](docs/contracts/activation_visibility_contracts.md). Preserve manual inactivity, strict original ping cadence, atomic refresh and checked historical inspection. Center rays are not retail PVS; complete encounters, safe NEW_LEVEL and M2 remain open.

The [corpse placement correction](../Documentation/Implementation/m2_corpse_placement.md) preserves original raw death dimensions and fits the new body locally against primary-world geometry. Read [contracts](docs/contracts/corpse_placement_contracts.md). Preserve the clear cached anchor, same-side corrections, bounded distance/work, final box/center-path validation and deferred failure cleanup. Reports retain actual attempted/committed state without advancing simulation. This is replacement triangle collision, not retail depenetration; safe NEW_LEVEL, required services and M2 remain open.

The [built-in Model continuation](../Documentation/Implementation/m2_builtin_models.md) integrates the original engine Model callback and 47 actual CityHub instances. Read [contracts](docs/contracts/builtin_model_contracts.md). Preserve forced OT_MODEL, BaseClass defaults/property order, empty/missing resource distinctions, shared immutable assets and deferred lifetime. Do not conflate Model with CModelObject/CDestructableModel or infer lighting/physical bodies. CityHub remains 159 unsupported / 35 failed; safe NEW_LEVEL and M2 remain open.

The [destructible-model continuation](../Documentation/Implementation/m2_destructable_models.md) connects original CDestructableModel rules and 122 CityHub objects. Read [contracts](docs/contracts/destructable_model_contracts.md). Preserve authored versus automatic dimensions, replacement non-rotation, cascade/base callback order, exact destroyed flags and deferred destruction. Models reuse the bounded atomic box solver; no contact forces or depenetration are invented. Unnamed ABC clips remain unnamed. CheckClearShot's destructible filter is separate from CanSee. Chrome/save/partial destroy pairs remain explicit failures. CityHub is 413 created / 37 unsupported / 35 failed; safe NEW_LEVEL and M2 stay open.

The [SoundFX continuation](../Documentation/Implementation/m2_sound_fx.md) connects original nonstreaming state/command rules and retained audio. Read [contracts](docs/contracts/sound_fx_contracts.md). Preserve startup deadline cancellation, ambient-only looping, whole-string commands, ramp interruption/restart and stale deadlines. Owned strings fix duplicate release; source filtering remains after startup. Historical VoiceType schemas and FileStream/TIMESYNC remain failures, including all five CityHub entries. No class-count increase certifies playback. Safe NEW_LEVEL and M2 remain open.

The [PCM streaming continuation](../Documentation/Implementation/m2_audio_stream.md) supplies real disk-backed Sound/SoundFX playback. Read [contracts](docs/contracts/audio_stream_contracts.md). Preserve REZ slice bounds, two fixed sample windows, local sample clocks, failed-read diagnostics and world/source voice ownership. VoiceType=0 is explicit unused historical metadata only; nonzero values and TypeAction remain rejected. Compressed streams, reverb/priority arbitration and late network synchronization remain unavailable. CityHub has 418 created / 31 transferred sounds / 32 unsupported / four failed; safe NEW_LEVEL and M2 remain open.

The [Mac-10 continuation](../Documentation/Implementation/m2_mac10.md) connects original weapon ID 2, both-hand firing, pickups, NPC drops and autonomous Cultist use. Read [contracts](docs/contracts/mac10_contracts.md). Preserve marker-driven cadence, complete primary-sequence fallback when alt_fire is absent, typed pickup acknowledgements and checked detached-hand lifetime. Chrome/multiplayer boost and view-key audio remain unavailable. CityHub has 419 created / 31 transferred sounds / 31 unsupported / four failed; safe NEW_LEVEL and M2 remain open.

The [compound-shell correction](../Documentation/Implementation/m2_compound_shells.md) admits independently closed DAT components sharing edges. Read [contracts](docs/contracts/compound_shell_contracts.md). Preserve per-component opposite-edge closure, base rings/welding/work limits, actual query geometry and authored BoxPhysics. Balanced winding alone is insufficient; unresolved non-manifold sheets still fail. Inspect topology even when hidden and retain checked removal/reload. CityHub is 421 created / 31 transferred sounds / 31 unsupported / two failed; safe NEW_LEVEL and M2 stay open.

The [Flare Gun continuation](../Documentation/Implementation/m2_flares.md) connects original ID 3, tracking FIRE ticks, eight alternate fragments, pickup/drop and autonomous Cultist use. Read [contracts](docs/contracts/flare_contracts.md). Preserve strict clocks, world-space offsets, loop restart, FRAG-before-BURST ordering, one point integration and checked independent children. Sprite tint/alpha is rendered; source trails/lights/composites and fire/gib deaths remain unavailable. CityHub is 422 created / 31 transferred sounds / 31 unsupported / one failed; safe NEW_LEVEL and M2 remain open.

The [world-model debris continuation](../Documentation/Implementation/m2_world_debris.md) connects original named-fragment activation, gravity, rotation and independent lifetime, with rendered geometry and inspection. CityHub now has 432 created / 31 transferred sounds / 21 unsupported / one failed entry. Conservative moving bounds are replacement physics; missing geometry, required services and safe NEW_LEVEL keep M2 incomplete. Read [contracts](docs/contracts/world_debris_contracts.md). Preserve contiguous name traversal, flag replacement, subclass/aggregate ordering, retained acceleration and deferred independent lifetime.

The [M16 continuation](../Documentation/Implementation/m2_assault.md) connects original primary fire, alternate grenade charges, pickup/drop, independent grenade lifetime and inspection. CityHub has 433 created / 31 transferred sounds / 20 unsupported / one failed. Finite inverted ABC key bounds remain visible metadata warnings. The original Cultist lacks rifle animations; direct AI firing is tested separately from unavailable autonomous M16 combat. Replacement grenade physics, required services and safe NEW_LEVEL keep M2 open. Read [contracts](docs/contracts/assault_contracts.md). Preserve distinct ammo pools, source sequential explosion passes, deferred lifetime, one model-projectile integrator and explicit missing clips.

The [conversation continuation](../Documentation/Implementation/m2_conversations.md) connects original ConversationTrigger sequencing, typed client delivery and CCommLink timing/real WAV playback, with inspection. CityHub has 434 created / 31 transferred sounds / 541 editor-only / 19 unsupported / one failed. All seven authored conversation cues in the base-world catalog remain unmapped and missing; separately labelled diagnostics verify actual audio without altering them. Original communication HUD/VoiceMgr arbitration, required services and safe NEW_LEVEL keep M2 incomplete. Read [contracts](docs/contracts/conversation_contracts.md). Preserve first-empty waves, backslash-only mapping, repeat precedence, busy drops, text-derived strict deadlines and independent queued/voice lifetime. Never fabricate dialogue for an unmapped authored reference.

The [medkit continuation](../Documentation/Implementation/m2_medkit.md) connects original MedKitPU/CInvMedkit,
H direct-item activation, local WAV playback and owned charge restoration.
Read the [contracts](docs/contracts/medkit_contracts.md) before extending inventory items: preserve requested
versus actual healing, integer charge/return quirks, run/death/camera gates,
checked action delivery and keep-alive retention. Other items/HUD and safe
NEW_LEVEL remain unavailable; M2 is still incomplete.

The [playtest input/combat correction](../Documentation/Implementation/m2_playtest_input.md) makes player entry capture the mouse, retains startup capture until focus, and connects original Cultist dodge/roll actions. It also adds bounded alternative-axis corpse placement for train seats. Read [contracts](docs/contracts/playtest_input_contracts.md). Preserve UI input ownership, original impact direction/roll sequencing, unchanged body dimensions and checked death/removal. Mouse-driven authored encounter tests do not close safe NEW_LEVEL or M2.

For GPU validation on this developer machine, select the NVIDIA GPU (for example with `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json`) and verify the actual device name in the run log. Keep this a local test/launch choice, not a hardcoded production device requirement.


The [surface panning correction](../Documentation/Implementation/m2_surface_effects.md) connects authored Pan effects and restores the missing tunnel/rail motion cue. Read [contracts](docs/contracts/surface_effect_contracts.md). Preserve original P/Q origin speeds (direction labels convert to zero), independent shared-material rates, session-only clocks, bounded raw metadata and explicit unsupported effects. Do not move happytrain or the collision world to imitate the intro. GPU tests use NVIDIA on this machine; required services and safe NEW_LEVEL keep M2 incomplete.

The [Metal shader continuation](../Documentation/Implementation/m2_metal.md) packages SDL_shadercross-generated MSL beside SPIR-V. Read [contracts](docs/contracts/metal_shader_contracts.md) before changing shaders or backend selection. Regenerate all six artifacts together, preserve reflected bindings and SDL coordinate conventions, and advertise only bundled formats. Ordinary game builds do not require ShaderCross. Linux NVIDIA checks do not establish native Metal execution; D3D12 and M2 remain incomplete.

The [safe NEW_LEVEL continuation](../Documentation/Implementation/m2_level_change.md) promotes a prepared destination with the supported player and a new client, retaining the source on refusal. Read [contracts](docs/contracts/level_change_contracts.md). Preserve the owning frame boundary, seven list/count pairs, active camera, five AI table owners, destruction under each owning adapter, type-1 key removal and monotonic revisions. Never move a GameSession or replay source state as rollback. Clear old UI/input identities; transfer only audio output, not source voices. CityHub still rejects 18 unsupported/one failed entry; M2 stays open. The user confirms macOS execution, but automated Apple coverage is pending. Intermittent frozen dead Cultists blocking the train (including later enemies) remain a priority after this increment; existing deterministic checks do not establish a fix.

The [corpse doorway correction](../Documentation/Implementation/m2_corpse_clearance.md) addresses a reproduced frozen dead Cultist by adding checked source-center separation and swept living-body clearance to initial corpse placement. Original dimensions and bounded failures remain intact; in-game inspection records the extra sweep. Read [the clearance contracts](docs/contracts/corpse_clearance_contracts.md) before extending this policy. This does not certify every later enemy or close M2.

The [authored particle-stream continuation](../Documentation/Implementation/m2_particle_streams.md) connects original CClientParticleStreamSFX/CParticleStreamFX toggles, ramps, particles and inspection. Read [contracts](docs/contracts/particle_stream_contracts.md). Preserve strict burst deadlines, original red-derived colors, moving system origins, publish-before-filter order and checked owner lifetime. The original client never reads the server sound tail; do not invent playback. Limits fail explicitly. CityHub is now 443 created / 31 transferred sounds / 541 editor-only / 10 unsupported / one failed; NEW_LEVEL still refuses and M2 remains open.

The [original sky continuation](../Documentation/Implementation/m2_sky.md) integrates built-in DemoSkyWorldModel/SkyPointer with checked sky slots, first-update lookup, camera parallax and actual SDL_GPU world-model backgrounds. Read [sky contracts](docs/contracts/sky_contracts.md). Preserve deferred pointer removal, empty-list release, index order, unchanged simulation/collision and explicit unsupported types/duplicate slots. The projection/depth reference is later LithTech, not verified retail parity. CityHub has 448 created / 31 sounds / 541 editor-only / five unsupported / one failed entry. M2 and CityHub NEW_LEVEL remain incomplete.

Interactive input/simulation frames are capped at 100 ms by `app_frame_delta`; discarded wall time is inspectable. Resource preparation and diagnostic capture are paused tool work. Keep direct diagnostic stepping and core physics safety bounds unchanged; this is a frontend catch-up policy, not a retail timing claim.


The [proximity-bomb continuation](../Documentation/Implementation/m2_proximity.md) connects original ProximitiesPU, reserved inventory slot10, charged throws, sticking/detection/detonation, independent lifetime and inspection. CityHub now has 450 created / 31 transferred sounds / 541 editor-only / three unsupported / one failed entry. Remote bombs, flashlight and missing `fallengrills` geometry still block CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical. Read [contracts](docs/contracts/proximity_contracts.md). Preserve reserved-slot ownership, source hold/overhold boundaries, shooter-inclusive detection, no inherited fuse, death-delay ordering and checked deferred lifetime. The shared model-projectile solver is replacement physics; do not claim retail parity. P is the application selection binding.

The [remote-bomb continuation](../Documentation/Implementation/m2_remote_bombs.md) connects original RemotesPU, reserved slot11, charged throws, separate detonation and player-owned charge cleanup, with inspection. CityHub now has 452 created / 31 transferred sounds / 541 editor-only / one unsupported / one failed entry. FlashlightPU and missing `fallengrills` geometry still reject CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical. Read [contracts](docs/contracts/remote_bomb_contracts.md). Preserve source no-fuse/no-overhold rules, first-character falling contact, deferred detonation, separate player/shooter links and original destructor/Respawn cleanup. Pending-owner link notifications must not erase the list before destruction. Supported restore retains inventory, never old projectile identities. R selects and B detonates independently of current weapon/ammo.


The [flashlight continuation](../Documentation/Implementation/m2_flashlight.md) connects original FlashlightPU/CInvFlashlight/CFlashlightFX, L toggle, battery, local audio, inactive restoration and visible radial point illumination with inspection. CityHub now has 453 created / 31 transferred sounds / 541 editor-only / zero unsupported / one failed entry. Missing `fallengrills` geometry still rejects CityHub NEW_LEVEL; M2 remains incomplete. Earlier counts below are historical. Read [contracts](docs/contracts/flashlight_contracts.md). Preserve rounded recharge refusal, body-position/internal-angle following, source shade-failure fallback, active-only discharge and forced inactive Restore/KeepAlive. Keep checked child ownership and expose failed updates. Radial brightening is replacement lighting; sky/FX/view weapons, shadows and general illumination remain separate. Regenerate all six SPIR-V/MSL artifacts together; NVIDIA validation does not certify native Metal execution of this change.


The [CityHub entry continuation](../Documentation/Implementation/m2_cityhub_entry.md) records `fallengrills` as an explicitly omitted object because its named BSP is absent. The source instance is rejected before initial update, matching the available LithTech loader behavior; no replacement geometry is invented. CityHub now has 453 created / 31 sounds / 541 editor-only / one omitted / zero unsupported / zero failed entries. Safe NEW_LEVEL preserves the supported player and enters at `01_egressa`; required SPR world materials now decode and animate through SDL_GPU. M2 remains incomplete; earlier CityHub refusal/count statements are historical. Read [contracts](docs/contracts/cityhub_entry_contracts.md). Keep missing-name rejection typed and world-file omission explicit; runtime creation still fails. Ambiguous/invalid geometry and other preparation failures remain blocking. Do not alias absent models or import reference engine dependencies. Later-engine source evidence is not a verified 1998 executable trace.

The [AI alignment correction](../Documentation/Implementation/m2_ai_alignment.md) honors the original SDK fallback for parallel movement/obstacle axes. Read [contracts](docs/contracts/ai_alignment_contracts.md). Preserve valid-frame versus blocked-route distinction, failed partial-route guards, query budgets and the existing right-axis convention. The perturbation policy is corroborated by later LithTech, not a retail1998 trace. Per-owner historical axes are inspectable. Diagnostic relocation evidence does not prove full CityHub traversal; M2 stays incomplete.

The [scripted opportunity-fire continuation](../Documentation/Implementation/m2_ai_script_fire.md) connects original standing/walking/running fire to path/object movement. Read [contracts](docs/contracts/ai_script_fire_contracts.md). Preserve strict cadence, source cached moving aim and shared target, standing residual velocity, distinct blocked-shot/phase rules, one integration and checked post-shot lifetime. No-target decisions must stay distinct from failed services. Inspector values are last-call history; diagnostic encounters do not complete M2.

The [burning death and dismemberment continuation](../Documentation/Implementation/m2_death_effects.md) connects original fire/gib decisions, CCorpse burning/postmortem damage, runtime CGib limbs and client flesh fragments. Read [contracts](docs/contracts/death_effect_contracts.md). Preserve fire priority, NEVERGIB, accumulated-damage launch speed, double negation, strict burn/smoke deadlines, named-node quirks and disabled living-NPC limb loss. Keep nonblocking children independent and cosmetic failures separate from actor removal. Zero elapsed fragment updates must not query zero-length rays. Flame/smoke count-only scaling and smoke movement samples are source rules. Blood-spurt/splat FX, generic gib templates/recursive debris, save/restore and the actual M2 gate remain incomplete.

The [original smell-trail continuation](../Documentation/Implementation/m2_ai_smell.md) connects the original periodic player trail and shared AI SearchSmellTarget state, with checked hint lifetime and inspection. Original no-target/no-cursor exits and zero NPC smell ranges are preserved; no automatic trail acquisition is invented. Required remaining services and the actual M2 gate stay open. Read [contracts](docs/contracts/ai_smell_contracts.md). Preserve strict cadence, global 3D spacing, source guard/phase order, balanced cursor links and deferred world-owned lifetime. Diagnostic seeded routes are not authored autonomous tracking.


The [original blood-pool continuation](../Documentation/Implementation/m2_blood_splats.md) connects CClientSplatFX/CBloodSplatFX with corpse ground-hit publication, persistent oriented sprites and inspection. Read [contracts](docs/contracts/blood_splat_contracts.md). Preserve eleven-existing-neighbor reuse, nearest-prefix/tie order, state-preserving relocation, delta-squared growth, uncapped Term clocks and detached lifetime. Blood spurts/gib impact callers, shared-list eviction and the actual M2 gate remain incomplete.

The [blood-spurt continuation](../Documentation/Implementation/m2_blood_spurts.md) connects original vector-weapon entry/exit jets, fragment launch jets and impact marks, and deferred limb impact requests. Read [contracts](docs/contracts/blood_spurt_contracts.md). Preserve random-before-damage selection, raw visual damage, emission-before-motion, separate particle/emitter gravity, strict fade, distinct splat thresholds and detached lifetime. Cosmetic child failures never cancel damage/removal. Separate flesh sparks, outside-world classification, shared SFX eviction, full save/restore and the actual M2 gate remain open. Earlier blanket blood-spurt/gib-impact omissions are superseded for this subset.

The [original impact-spark continuation](../Documentation/Implementation/m2_sparks.md) connects CSparksFX flesh bursts and metal sparks, with raw-damage recipes, detached particles and inspection. Read [contracts](docs/contracts/spark_contracts.md). Preserve source component velocity clipping, strict fade/removal clocks, cosmetic RNG isolation and explicit empty melee recipes. Exhausted invisible systems are reclaimed after duration; shared-list eviction and other required services remain unavailable. M2 stays incomplete.

The [compact HUD continuation](../Documentation/Implementation/m2_hud.md) displays delivered health, armor and current primary/alternate ammunition independently of the inspector. Physical 1..9/0 and keypad keys select original normal inventory slots; empty slots and original player/camera gates remain enforced. Read [contracts](docs/contracts/hud_contracts.md). Preserve owned snapshots, distinct M16 pools, input suppression and shared UI lifetime in inspector-free builds. The original PCX status-bar artwork and other required services remain pending; M2 is incomplete.

The [original pickup-audio continuation](../Documentation/Implementation/m2_pickup_audio.md) connects collection WAVs and durable results, including keys. Read [contracts](docs/contracts/pickup_audio_contracts.md). Preserve accepted-ack-only playback, original 30/300 radii, inventory1.wav for keys, sound-before-trigger order, failure-independent collection and detached lifetime. Severe post-transition stutter, idle Cultists/durability and missing CityHub water remain user-reported follow-ups. M2 stays incomplete.

The [collision traversal continuation](../Documentation/Implementation/m2_collision_traversal.md) addresses measured post-transition activation spikes with conservative BVH/triangle interval rejection. Read [contracts](docs/contracts/collision_traversal_contracts.md); preserve exact SAT results, equal-time ordering, placement and original AI cadence. Services exposes bounded work counters. CPU model preparation and other frame costs remain; M2 stays open.

The [bound-container index](../Documentation/Implementation/m2_container_index.md) reduces repeated post-transition object scans while retaining fresh checked snapshots, original volume order and deferred destruction. Read [contracts](docs/contracts/container_index_contracts.md); never mutate a bound shape outside Objects. Container inspection includes the index count. CPU model preparation and other playtest reports remain; M2 stays incomplete.

The [model preparation optimization](../Documentation/Implementation/m2_model_preparation.md) reuses exact local geometry for immutable shared ABC assets and removes temporary heap pose scratch. Read [contracts](docs/contracts/model_preparation_contracts.md); preserve weak ownership, current visibility/materials, packed-deformation order and world-revision clearing. Inspection reports actual reuse and retained vertex bytes. Skinning remains on the CPU; remaining frame costs and the M2 exit gate stay open.

The [NPC animation-clearance correction](../Documentation/Implementation/m2_ai_animation_clearance.md) restores original SetAnimation continuation when physical resizing is blocked. Read [contracts](docs/contracts/ai_animation_clearance_contracts.md). Preserve the retained physical body, actual cached AI dimensions, separately inspected request, same-clip reset behavior and explicit invalid/unavailable failures. Inspector counters expose blocked resizes; no forced activation or balance changes are added. The collision solver remains a replacement and M2 stays incomplete.

The [original PolyGrid continuation](../Documentation/Implementation/m2_polygrids.md) connects VolumeBrush surface creation, original high-detail plasma/scale rules and actual transparent SDL_GPU rendering. Read [contracts](docs/contracts/polygrid_contracts.md). Preserve independent publisher lifetime, Current.y pan, hidden-clock pause, shared per-world phases, checked resource failures and revision-owned caches. Visual publishers are excluded from replacement body physics; static containers, explicit always-update timing and separate alpha sorting remain bounded policies. CityHub water now renders; underwater fog, moving volumes and the M2 gate stay open.

The [NPC ammunition continuation](../Documentation/Implementation/m2_ai_ammo.md) connects original FindAmmo and MID_ADDAMMO instead of suspending an empty-ammo actor. Read [contracts](docs/contracts/ai_ammo_contracts.md). Preserve original class selection, ray-hit visibility, empty-route escape, single integration and capped collection. Specific+AvoidTarget skips unused undefined arithmetic; removed pickup trackers retain only their owned route. Private contact/hidden-inspector tests do not prove autonomous retail pickup pursuit or resolve every idle-NPC report. M2 remains incomplete.

The [service coverage continuation](../Documentation/Implementation/m2_service_coverage.md) audits the bounded adapter catalogue and restored-player/exit trace scopes. Read [contracts](docs/contracts/service_coverage_contracts.md). Keep implementation status separate from observed attempts, retain unknown/unexercised entries and teardown failure history, and never interpret a counted call as success. This is not the full historical SDK/client API; M2 remains incomplete.

The [original exit-fade continuation](../Documentation/Implementation/m2_screen_fade.md) connects player FADEIN/FADEOUT and tests the full authored SubwayA exit relay through contact, combat and NEW_LEVEL. Read [contracts](docs/contracts/screen_fade_contracts.md). Preserve literal-space parsing, source no-op versus unsupported commands, strict 100-unit/second endpoints, black hold and fresh-world reset. Drawing never advances clocks. Measured smoke budgets now admit the original final explosions without truncating emission; default particle limits and explicit failures remain. Composited scene darkening is not full retail global lighting. M2 remains incomplete.

The [continuous train/message-lifetime continuation](../Documentation/Implementation/m2_message_lifetime.md) replaces retained per-message pointer tokens with checked, non-reused value identities and bounded live payloads. Read [contracts](docs/contracts/message_lifetime_contracts.md). Preserve borrowed callback readers, stale/foreign rejection, deferred lifetime and per-adapter inspection history. The spawn-to-CityHub route uses original input/combat/contact/exit paths with no teleport or stat edits; it does not certify retail parity or complete M2.

Owned documentation filenames are lowercase. Keep README concise (description,
status, build, contribution, AI-use note and scoped MIT license); implementation
updates belong in docs and workspace Documentation. Preserve historical evidence
snapshots and original/third-party notices when updating references.

The [door-audio continuation](../Documentation/Implementation/m2_door_audio.md) connects original attached busy loops and stop sounds. Read [contracts](docs/contracts/door_audio_contracts.md); preserve stop-before-empty/failure, unretained one-shot overlap, fixed 500/1500 radii, priority 1 and checked owner lifetime. Inspect actual voices and failures; reverb/arbitration, lock sounds and music remain separate. The exact exit population now includes the original periodic smell hint. M2 remains incomplete.

The [trigger-audio continuation](../Documentation/Implementation/m2_trigger_audio.md) connects original activation and lock/explicit-unlock one-shots. Read [contracts](docs/contracts/trigger_audio_contracts.md). Preserve trigger-versus-sender attachment, R*0.1 inner radius, silent key replies, overlapping voices and unchanged lock/activation progression after playback failure. Action-mode console feedback, reverb/priority arbitration and the actual M2 gate remain open.

The [keyframe-audio continuation](../Documentation/Implementation/m2_keyframe_audio.md) connects original retained attached sounds. Read [contracts](docs/contracts/keyframe_audio_contracts.md); preserve the single handle across all targets, replacement before failure, silent keys/OFF leaving playback intact, sound-before-message order and destructor cleanup. Three authored missing resource references remain explicit; dry playback and diagnostic Train Station activation do not complete M2.

The [object snapshot optimization](../Documentation/Implementation/m2_object_snapshots.md) maintains an occupied-slot identity index. Read [contracts](docs/contracts/object_snapshot_contracts.md); publish before constructors, erase after destructors, preserve pending records and full generations, and return fresh owned slot-ordered copies. Never cache live properties or retain index iterators across callbacks. Inspection counts records, not successful objects. M2 remains incomplete.


The [original player VoiceMgr continuation](../Documentation/Implementation/m2_player_voices.md) connects weapon-pickup replies, SOUND and conversation arbitration. Read [contracts](docs/contracts/player_voice_contracts.md); preserve custom-group precedence, strict cooldown, unique-after-success flags, both server chance draws, stop-before-failure, independent local tails and value-only NEW_LEVEL history. Two installed unique references are absent; missing playback is not success. Other original callers, IMA music and the M2 gate remain open.


The [combat voice continuation](../Documentation/Implementation/m2_combat_voices.md) connects original player pain/burning and NPC/corpse kill/gib replies. Read [contracts](docs/contracts/combat_voice_contracts.md); preserve integer net-health loss, chance-draw order, distinct killer filters, checked local-client lookup and cosmetic failure independence. Queued replies are not successful playback; inspect the actual VoiceMgr status. Remaining original callers, interactive music and the M2 gate stay open.

The [single-player death continuation](../Documentation/Implementation/m2_player_death.md) connects original first-death commitment, corpse/gib creation, item cleanup and the client death orbit. Read [contracts](docs/contracts/player_death_contracts.md); preserve the null-killer guard, bitwise EXPLODE test, literal 50.0 threshold, first-key removal, retained weapons/ammo and independent corpse lifetime. Dead single-player USE retains its source ray; no revival shortcut is added. Active powers, multiplayer death, full dead-player/world restoration and the M2 gate remain open.

The [movement-audio continuation](../Documentation/Implementation/m2_movement_audio.md) connects original grounded Jump voices and sampled-height landing sounds. Read [contracts](docs/contracts/movement_audio_contracts.md); preserve strict >56, free-movement reset, raw foot-container precedence, first-ladder order and independent client/server voice ownership. Failed audio never cancels movement or repeats a landing. Preserve checked fall history in live snapshots; impact-force/fall damage and M2 remain incomplete.

The [stopping-contact and falling-damage continuation](../Documentation/Implementation/m2_fall_damage.md) connects original player impact thresholds to accepted collision impulses. Read [contracts](docs/contracts/fall_damage_contracts.md); preserve pre-stop callback velocity, zero-force sensors, deferred removal, the rotating-door early return, self Normal damage and saved server-falling state. Mass 250/friction 0 now reach the engine and restored player. Later LithTech informs force formulas without proving retail parity; dedicated pusher/body/projectile forces and the M2 gate remain open.

The [player damage-impulse continuation](../Documentation/Implementation/m2_player_impulses.md) enables original player knockback and the single-player self-hit exclusion. Read [contracts](docs/contracts/player_impulse_contracts.md); preserve resistance-before-armor, one shared body integration, additive door pushes and Load/force-position resets. Never replay a second client-body impulse. Death retains passive momentum; NPC impulses, already-dead ordering and the actual M2 gate remain open.

The [original music asset prerequisite](../Documentation/Implementation/m2_music_assets.md) preserves original playlist request order and inspects bounded external AASE/AASY/DLS containers. Read [contracts](docs/contracts/music_asset_contracts.md); keep opaque records, missing files and playback unavailable distinct from structural validity. No DLL loading or silent successful MUSIC calls are introduced. Synthesis, personality semantics and the actual M2 gate remain open.


The [DLS data continuation](../Documentation/Implementation/m2_music_dls.md) decodes original external instrument-bank PCM, cue/region mappings and raw articulation. Read [contracts](docs/contracts/music_dls_contracts.md); preserve cue indirection, signed values, region overrides, strict effective loop bounds and immutable world ownership. Cabal's unused invalid wave default remains visible; Crudux/Track1 invalid effective loops refuse decoding without invented repair. No synthesis or successful MUSIC service is claimed; M2 remains incomplete.

The [DLS voice continuation](../Documentation/Implementation/m2_music_sampler.md) adds explicit instrument auditions through the existing audio clock/output. Read [contracts](docs/contracts/music_sampler_contracts.md); preserve region graph precedence, effective loops, signed gain/envelope units, immutable bank lifetime and explicit unsupported connections. Cabal has 33 synthesized / five refused region probes. Diagnostic notes are not original MUSIC playback; M2 stays incomplete.

The [legacy DLS pan continuation](../Documentation/Implementation/m2_music_legacy_pan.md) identifies the installed synth's integer destination 2 and ordered overwrite with fixed-point destination 4. Read [sampler contracts](docs/contracts/music_sampler_contracts.md). Preserve raw records, per-record bounds, explicit unknown-edge rejection and the documented full-precision replacement pan law. All 38 Cabal region auditions now synthesize; original sequences/MUSIC and the actual M2 gate remain incomplete.


The [IMA pattern-note continuation](../Documentation/Implementation/m2_music_notes.md) decodes the installed AASY pattern/grid/note profile with original-reader equivalence and read-only inspection. Read [contracts](docs/contracts/music_note_contracts.md); preserve record order, signed fields, opaque curves/header tails, bounded atomic failure and immutable world ownership. Decoded notes are not final MIDI events or successful MUSIC playback; sequencing, required services and the actual M2 gate remain open.

The [IMA band continuation](../Documentation/Implementation/m2_music_bands.md) decodes original style/section channel records and prepares verified original-order controller/program messages. Read [contracts](docs/contracts/music_band_contracts.md); preserve packed flags, legacy fallback, signed transpose, direct scope, raw unknown fields and immutable ownership. Setup messages are not sent; effective band selection, sequencing and the actual M2 gate remain open.

The [IMA section continuation](../Documentation/Implementation/m2_music_sections.md) decodes authored timing and command/mute/chord records with original-reader checks. Read [contracts](docs/contracts/music_section_contracts.md); preserve signed fields, low-word division, file order, raw unknown values, atomic bounds and immutable world ownership. Header timing precedes style reconciliation; decoding does not implement sequence playback or MUSIC. M2 stays incomplete.

The [IMA timeline continuation](../Documentation/Implementation/m2_music_timeline.md) reconciles style meter, command/chord times and beat-sampled mutes with original executable checks. Read [contracts](docs/contracts/music_timeline_contracts.md); preserve signed/file-order behavior, the initial future mute, negative chord beats, explicit binding failures and immutable bounded ownership. Pattern choice, pitch harmonization, actual MUSIC playback and the M2 gate remain open.

The [IMA variation continuation](../Documentation/Implementation/m2_music_variations.md) decodes complete pattern channels and reproduces original defaults and bounded variation decisions. Read [contracts](docs/contracts/music_variation_contracts.md); preserve immutable authored fields, per-row default bits, random precedence/draw order and unfiltered cycle/hold behavior. Explicit diagnostic choices are not live scheduling or MUSIC playback; pitch conversion, remaining required services and the M2 gate stay open.

The [IMA note-event continuation](../Documentation/Implementation/m2_music_events.md) reproduces the original post-harmonization event arithmetic with explicit pitch/timing/map/draw inputs and diagnostic inspection. Read [contracts](docs/contracts/music_event_contracts.md); preserve zero-pitch suppression, signed range/draw order, octave folding and equal-velocity note-off. Reject unsafe indexes/tick overflow atomically. Prepared messages are not harmonic conversion, live scheduling or MUSIC playback; the actual M2 gate stays open.

The [IMA harmonic-pitch continuation](../Documentation/Implementation/m2_music_pitch.md) reproduces original mode/chord/scale/octave/range arithmetic with explicit runtime harmony and in-game event inspection. Read [contracts](docs/contracts/music_pitch_contracts.md); preserve raw bypass, signed/unsigned nibble differences, ordered range adjustment and zero suppression. Bound nonterminating mode-4 scale walks with explicit failure; retain finite long-walk results. No personality reconciliation, current-chord selection or MUSIC playback is claimed. M2 remains incomplete.


The [IMA prepared-harmony continuation](../Documentation/Implementation/m2_music_harmony.md) connects bounded external REPs metadata and original retained-chord pitch/variation flags. Read [contracts](docs/contracts/music_harmony_contracts.md); preserve signed remainders, original mask/flag order, next-retained-chord classification and explicit missing bindings. Keep opaque personality graphs and the limited zero-ID filename profile visible. Prepared harmony does not select or activate a live chord, schedule music or satisfy MUSIC; M2 remains incomplete.

The [IMA section/pattern continuation](../Documentation/Implementation/m2_music_selection.md) reproduces original current chord/mute lookup and pattern request/choice with explicit ticks. Read [contracts](docs/contracts/music_selection_contracts.md); preserve first-future/file-order behavior, first equal command, low-word signed lengths, exact category matching and fallback order. The bounded linear chooser is executable-checked. Empty-chord global fallback, seek/activation, live MUSIC and the actual M2 gate remain open.


The [IMA grid-cursor continuation](../Documentation/Implementation/m2_music_cursor.md) connects original normal-mode grid progression, exact-command restarts, variation cadence and note admission. Read [contracts](docs/contracts/music_cursor_contracts.md); preserve the saved old ordinal, mask/root-only change test, explicit draws, silent missing grids and atomic world-owned lifetime. This is a manual single-section diagnostic, not audio playback. Clocks, bands/curves, active notes and playlists remain; MUSIC and M2 stay incomplete.


The [IMA band-selection continuation](../Documentation/Implementation/m2_music_band_selection.md) connects original last-section-band priority and reverse-order marked style fallback. Read [contracts](docs/contracts/music_band_selection_contracts.md); preserve header-before-band guards, any-nonzero byte104, absent-default failure and immutable source ownership. Selection and controller-setup errors remain distinct. No live MIDI/soundtrack is claimed; remaining music services and M2 stay incomplete.

The [original music routing continuation](../Documentation/Implementation/m2_music_routing.md) connects selected-band MIDI snapshots to DLS patch/key lookup and controller-aware diagnostic notes. Read [contracts](docs/contracts/music_routing_contracts.md); preserve packed-bank conversion, percussion-only fallback, missing/ambiguous failures, immutable bank ownership and note-on controller snapshots. Decoded instruments are treated as resident for this diagnostic; dynamic controllers, original transport/playlists and MUSIC remain unavailable. M2 stays incomplete.

The [connected IMA grid-note continuation](../Documentation/Implementation/m2_music_grid_notes.md) composes normal-mode harmonic activation, admitted notes and selected-band event preparation. Read [contracts](docs/contracts/music_grid_note_contracts.md); preserve variation-before-note draw order, explicit output ticks, suppressed rows, atomic failure and owned inspection. Prepared pairs are not MIDI submission; active-note scheduling, curves/playlists, MUSIC and M2 remain incomplete.

The [IMA active-note continuation](../Documentation/Implementation/m2_music_active_notes.md) connects original chord-dependent deadlines, creation-serial exemptions and due/retained ordering. Read [contracts](docs/contracts/music_active_note_contracts.md); preserve assignment without min, unsigned-root membership, low-word wrap, inclusive horizons and head-insertion order. Keep bounded atomic failure, owned inspection and explicit admission-only clearing. Terminal tails are retained, not played or flushed; transport, MUSIC and M2 remain incomplete.

The [IMA style-curve continuation](../Documentation/Implementation/m2_music_curves.md) decodes original curve headers and ordered points with independent inspection. Read [contracts](docs/contracts/music_curve_contracts.md); preserve signed/raw fields, 7/10 strides, scope/record bounds, source offsets and atomic immutable ownership. Decoded curves are not evaluated controllers; curve/note scheduling, MUSIC and M2 remain incomplete.

The [IMA style-curve event continuation](../Documentation/Implementation/m2_music_curve_events.md) reproduces original point expansion and ordered timed messages with explicit owner/tick inspection. Read [contracts](docs/contracts/music_curve_event_contracts.md); preserve signed wrapping, regenerated tables, three-tick spacing, endpoint replacement and raw pitch-bend bytes. Atomic bounded preparation does not implement admission, controller delivery or live MUSIC; M2 remains incomplete.

The [ordered grid dispatch continuation](../Documentation/Implementation/m2_music_grid_dispatch.md) connects original curve filters and source-order note/curve preparation, followed by due stops. Read [contracts](docs/contracts/music_grid_dispatch_contracts.md); require the exact style curve binding, preserve variation/draw order and atomic cursor/active-note rollback, and keep owned dispatch distinct from actual playback. Controller delivery, transport and MUSIC still keep M2 incomplete.

The [DLS live voice control continuation](../Documentation/Implementation/m2_music_live_controls.md) connects checked current-voice volume/expression/pan and original raw pitch bend. Read [contracts](docs/contracts/music_live_control_contracts.md); preserve immutable note-on provenance, sample/envelope clocks, release gates, nominal gain, raw bend bytes and atomic rejection. Manual voice updates are not timed channel dispatch or MUSIC; M2 remains incomplete.

The [immediate MIDI receiver continuation](../Documentation/Implementation/m2_music_midi_receiver.md) connects channel controls, held notes and original note-off/repeat selection. Read [contracts](docs/contracts/music_midi_receiver_contracts.md); preserve channel identity, immutable note-on snapshots, newest-held stops, new-region self-nonexclusive choice, bounded fast release and atomic failure. Explicit manual delivery is not timed transport or MUSIC; M2 remains incomplete.

The [timed MIDI continuation](../Documentation/Implementation/m2_music_timing.md) delivers queued notes and supported channel values on the existing sample clock. Read [contracts](docs/contracts/music_timing_contracts.md); preserve stable note order, last-due control/patch lookup, submission-time bank capture, atomic admission and explicit delivery failures. Clear future bank ownership on world unload. Sample scheduling is not IMA tempo/section/playlist transport or MUSIC; M2 remains incomplete.
