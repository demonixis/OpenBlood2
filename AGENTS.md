# Blood2 Project Instructions

Follow the workspace [AGENTS.md](../AGENTS.md) and the [roadmap](../Documentation/Roadmap/README.md).

- This is the new implementation repository. Earlier projects are optional donors, never build dependencies.
- Keep the three project targets small: `engine`, `blood2_game`, `blood2_app`. M1 links original `B2BaseClass`, `CameraSpot` and `Key`; M2 adds `GameStartPoint`, `PathPoint` and selected trigger/load-command code. The service continuation adds original WorldProperties/SmellHint, ABC playback/deformation and primary-world swept collision. M2 is in progress, not a working original game session. Read [docs/OBJECT_CONTRACTS.md](docs/OBJECT_CONTRACTS.md) before adding classes/services; extend the typed subset and tests together.
- Use plain objects/functions and explicit return values. No EnTT/ECS or manual interface/vtable reconstruction.
- Dependency revisions and archive hashes live in `cmake/Dependencies.cmake`. Resolve the latest SDL `main` when updating SDL and retain the exact SHA for repeatability.
- Read [docs/WORLD_CONTRACTS.md](docs/WORLD_CONTRACTS.md) before extending readers or session loading. Preserve exact-path mount precedence, bounded failures and source-entry accounting. Keep opaque format sections and missing required services explicit; the M2 exit gate remains open.
- Read [docs/MODEL_AND_COLLISION_CONTRACTS.md](docs/MODEL_AND_COLLISION_CONTRACTS.md) before extending animation/physics. The Caleb inspection preview is not CPlayerObj; do not mark full session startup complete on that basis.
- Keep commercial resources external. Local paths belong in ignored `blood2.local.ini`/`CMakeUserPresets.json` or command-line options.
- Build/test with the presets in README. GPU tests need a real display and must be explicitly enabled. Report headless, private-data and graphics results separately.
- Test the inspector-free Release build when changing application/platform lifetime or UI integration. Keep inspection data truthful; detected archives are not loaded resources.
- Record selective imports in `docs/PROVENANCE.md`; preserve original reference trees.
- Keep the original object and aggregate callback chain intact. Never replace `SetNextUpdate` with callback-return timing or destroy an object inside its callback. Run the object contract suite and Clang sanitizers for lifetime/dispatch changes.

- The [player continuation](../Documentation/Implementation/M2_PLAYER.md) connects selected original shell/CPlayerObj entry branches. Read [docs/PLAYER_CONTRACTS.md](docs/PLAYER_CONTRACTS.md) before extending them. The classes are partial source ports: preserve the documented omissions and do not describe entry inspection as complete player gameplay. Keep client association handle-checked, messages owned and bounded, and removal/rollback independent of the UI.

- Read [docs/WEAPON_CONTRACTS.md](docs/WEAPON_CONTRACTS.md) before extending inventory, client weapon input/animation, ray fire or damage. Beretta/knife are selective original ports; preserve every FIRE_KEY event (including the two primary knife markers), owned weapon/client models and deferred hit-callback removal. Other weapons, pickups, full combat, audio/FX and muzzle/hand placement are still open. Keep typed shot/damage messages separate from legacy packet ABI; portable seeded randomness is not claimed retail-bit-identical.

- Read [docs/MOVEMENT_CONTRACTS.md](docs/MOVEMENT_CONTRACTS.md) before extending original MoveMgr, player input or support queries. Client-associated players use one shared body and skip generic physics; typed movement must resolve collision exactly once. Keep source rules separate from the documented replacement integrator. Read [docs/LOCOMOTION_CONTRACTS.md](docs/LOCOMOTION_CONTRACTS.md) for current stairs, crouch/animation dimensions, source clip fallbacks and clearance. Selected containers are covered by the contracts below; translating platforms are covered by the door contracts below; bounded rotating transport is covered by the interaction contracts below.

- Follow `.clang-format` and `.editorconfig`. Use `OpenBlood2` for project-owned C++ namespaces; preserve source-global SDK/game classes. Commit code and English documentation after each completed run, in their respective independent repositories.
- Locomotion shape changes are atomic and foot-anchored. A blocked standing attempt preserves the short body and camera; this also applies to alive entry resets. Preserve original `CheckAnims` fallback and expose resolved clips. Keep the new stair solver explicitly separate from verified retail behavior.

- Read [docs/CONTAINER_CONTRACTS.md](docs/CONTAINER_CONTRACTS.md) before extending
  container creation, geometry, liquids, ladders or air/damage. Preserve closed
  shape validation, DAT base-ring metadata, head/body separation and four body
  records. Static containers reject unsupported transforms; removal invalidates
  queries immediately. Player motion must not receive generic volume integration
  a second time. Keep pending effects, AI and specialized volumes visible; keys are extended by the trigger contracts below.

- Read [docs/SPECIALIZED_VOLUME_CONTRACTS.md](docs/SPECIALIZED_VOLUME_CONTRACTS.md)
  for Conveyor/Damage/FreeFall and level-entry volume damage protection. Preserve
  source client/server conveyor differences, one player integration, authored
  numeric damage types, and the level clock across delayed entry/reload. Keep
  ambiguous/missing model bindings explicit; never select an arbitrary model.

- Read [docs/DOOR_CONTRACTS.md](docs/DOOR_CONTRACTS.md) before extending movers,
  door triggers or collision. Translating Door is a selective original port;
  its atomic pusher/rider solver is replacement engine code. Preserve checked
  named geometry, actual bounds centers, atomic blocking, one shared player
  integration and deferred touch removal. Rotation is extended by the interaction contracts below; crushing, audio/portals and the complete M2 gate remain explicitly pending.

- Read [docs/TRIGGER_CONTRACTS.md](docs/TRIGGER_CONTRACTS.md) before extending
  Trigger/BigTrigger/ToggleTrigger, door SelfTriggers or key inventory/pickups.
  Preserve the original active-command guard, player-only lock query, consuming
  key uses, named-filter order, delayed-toggle behavior and typed message ownership.
  SelfTriggers are owned, stationary children; pending parents cannot activate them.
  Player USE ray/input is extended below; generic pickups, audio/localized feedback
  and save/restore remain pending. Keep source-entry counts distinct from generated runtime children.

- Read [docs/INTERACTION_CONTRACTS.md](docs/INTERACTION_CONTRACTS.md) before extending
  player USE, rotating doors or collision. Preserve the 82-unit eye ray and
  direct-Door exclusion, press-edge input, source double angular increment and
  StartOpen behavior. Keep the authored shell immutable, transforms/body moves
  atomic, player push velocity consumed once and contact removal deferred.
  Conservative bounds/work limits must remain visible; this is not verified
  retail physics. Other movers, crushing and complete gameplay remain pending.

The latest [brush evidence](../Documentation/Implementation/M2_BRUSHES.md) connects original RotatingBrush/Rotating
and selected CDestructableBrush damage/removal. Read [brush contracts](docs/BRUSH_CONTRACTS.md) before extending
rotators, shared CDestructable, brush destruction or debris. Preserve source
zero-based initial angles, elapsed-time restart, per-axis ramps and distinct
CanDamage/Destructable gates. Keep atomic motion, reentrant damage guards and
deferred removal. CDebris/WorldModelDebris remain unported; diagnostic requests
are not FX. Pushable/gravity, explosive and SpawnObject brushes reject creation.
SubwayA now has 141 created source entries, 7 owned SelfTriggers, 372 editor-only
and 67 unported entries. Precreate rejection is a failure, not difficulty
filtering. Other movers and complete gameplay remain pending; M2 is open.

The latest [explosion evidence](../Documentation/Implementation/M2_EXPLOSIONS.md) connects original CDebris
properties/requests and explosive brush area damage. Read
[explosion contracts](docs/EXPLOSION_CONTRACTS.md) before extending debris, explosions or
radius damage. Preserve source squared-distance falloff, no visibility filter,
fixed effective 100 damage / 200 radius, multi-material velocity sign alternation,
checked snapshot order and deferred chain/removal. Reports count deliveries,
not health changes. FX/audio records are not rendered or played. WorldModelDebris,
pushable/gravity brushes, SpawnObject and full gameplay remain pending. The earlier
brush paragraph's Exploding rejection/CDebris omission is superseded; M2 stays open.

The latest [pushable evidence](../Documentation/Implementation/M2_PUSHABLES.md) connects selected original
CDestructableBrush Pushable/Gravity, mass/friction, brush damage impulses and
CPlayerObj ground/air priorities. Read [pushable contracts](docs/PUSHABLE_CONTRACTS.md) before
extending dynamic world bodies, pushing or damage physics. Preserve actual bounds
centers, one player integration, original DONTFOLLOWSTANDING, atomic rider/pusher
commit and deferred callbacks. BoxPhysics is required; polygon dynamics, stacks,
SpawnObject, player impulses and complete gameplay remain pending. Earlier blanket
pushable/gravity omissions are superseded for this bounded subset; M2 stays open.

The latest [pickup evidence](../Documentation/Implementation/M2_PICKUPS.md) connects
eight original ammo pickups, four health/armor pickups and destruction SpawnObject.
Read [pickup contracts](docs/PICKUP_CONTRACTS.md). Preserve checked typed acknowledgements,
normal/bonus caps, dead-player guards, independent child lifetime, death-trigger
ordering and deferred removal. Earlier blanket SpawnObject/pickup omissions are
superseded for this subset. Remaining inventory items, timed bonuses, old TypeAction
schemas, audio/FX and full M2 gameplay remain pending.

The latest [objective evidence](../Documentation/Implementation/M2_OBJECTIVES.md) connects original ObjectivesTrigger,
typed client delivery and external cres.dll strings. Read [objective contracts](docs/OBJECTIVES_CONTRACTS.md)
before extending objectives, localization or client broadcasts. Preserve the
nonempty authored-string guard, resource-ID precedence, original game-mode filter,
owned bounded queue and explicit unresolved state. The PE reader executes no code;
ambiguous languages and malformed resources fail. SubwayA now has 142 created
source entries, 7 owned SelfTriggers, 372 editor-only and 66 unported. HUD/audio,
transitions and complete M2 acceptance remain pending.

The latest [camera evidence](../Documentation/Implementation/M2_CAMERAS.md) connects original CameraObj activation,
tracking and typed client camera selection, including movement/USE/fire gates.
Read [camera contracts](docs/CAMERA_CONTRACTS.md) before extending camera modes, cinematics or
player restrictions. Preserve source/client listener differences, strict timeout,
unlink scheduling and already-started FIRE_KEY handling. Presentation/listener
requests are not cinematic rendering/audio. SubwayA now has 144 created source
entries, 7 owned SelfTriggers, 372 editor-only and 64 unported entries. KeyFramer,
CinematicTrigger, transitions and the actual M2 gate remain open.

The latest [keyframe evidence](../Documentation/Implementation/M2_KEYFRAMERS.md) connects original KeyFramer/KeyData timelines,
camera/ordinary-object motion and consumed-key ownership. Read [keyframe contracts](docs/KEYFRAMER_CONTRACTS.md)
before extending scripted paths, linked-camera rotation or controller callbacks.
Preserve first-update initialization, ignored early ON, component rotation offsets,
loop overshoot discard and the source's missing final snap. Keep target offsets paired
through removal and stop reentrant stale updates. World-model/container motion, key
audio and CinematicTrigger remain explicitly unavailable. SubwayA has 148 created
source entries, 7 SelfTriggers, 372 editor-only and 60 unported; nine Keys are consumed
on the first update (155/158 initial instances before/after player, 146/149 afterwards).
M2 remains open; these counts do not imply completed cinematic/gameplay services.

The latest [dynamic light evidence](../Documentation/Implementation/M2_LIGHTS.md) connects original ClientLightFX/GlowingLight
and client CLightFX waveform evaluation. Read [light contracts](docs/LIGHT_CONTRACTS.md) before
extending lights, SFX publication or their damage/lifetime. Preserve source command
prefixes, scheduling/NONE-wave quirks, color /511, absolute phase and deferred
source-bound lifetime. Client records and inspector overlays are real evaluated
state; world illumination/shadows/fog and audio remain unavailable. DirLight spots
and the implicit main light are separate. SubwayA now has 160 created source entries,
7 SelfTriggers, 372 editor-only and 48 unported (167/170 initial runtime instances,
158/161 after consuming nine Keys). The earlier missing GlowingLight statement is
superseded for this subset; M2 remains open.

The latest [train path evidence](../Documentation/Implementation/M2_WORLDPATHS.md) extends original KeyFramer
with translating BoxPhysics world models. Read [path contracts](docs/WORLDPATH_CONTRACTS.md).
Keep unchanged-rotation validation before mutation, primary-world checks without
GOTHRUWORLD, atomic pusher/rider commit, deferred contacts and single player integration.
SubwayA's diagnostic train run records 297 moves/two primary-world blocks per cycle;
this is replacement physics, not verified retail parity. Angular paths, dynamic
world bodies and containers remain unavailable. Class counts stay 160 created,
372 editor-only and 48 unported. Earlier blanket world-path omissions are superseded
only for this translation subset; M2 stays open.

The latest [cinematic evidence](../Documentation/Implementation/M2_CINEMATICS.md) connects original CinematicTrigger with
real bounded WAV decoding, typed voice completion and dry SDL playback. Read
[cinematic contracts](docs/CINEMATIC_CONTRACTS.md). Preserve any-payload rearming without reset,
message-before-sound order, missing-file retry, checked speaker/owner lifetime and
sample-count clocks independent of UI. Terminal odd-data padding exceptions are
bounded; three missing catalog references remain failures. Reverb is requested but
not rendered. General sound emitters/music and complete M2 acceptance remain open.
SubwayA now has 164 created source entries, 7 SelfTriggers, 372 editor-only and
44 unported (171/174 runtime before/after player, 162/165 after consuming nine Keys).
Earlier CinematicTrigger/audio omissions are superseded only for this subset.

The latest [random sound evidence](../Documentation/Implementation/M2_RANDOM_SOUNDS.md) connects original RandomSoundFX and typed fixed-position ambient/spatial voices.
Read [random sound contracts](docs/RANDOM_SOUND_CONTRACTS.md). Preserve case-sensitive prefix commands,
OFF voice tails, first-empty list termination, interval-based replacement, sequence
progression after failed starts, and one release per message read. Ambient attenuates
without panning; sampled voices do not follow later emitter motion. Built-in Sound
loops/streaming and other audio callers remain missing. SubwayA now has 166 created
source entries, 7 SelfTriggers, 372 editor-only and 42 unported (173/176 runtime
before/after player, 164/167 after consuming nine Keys). M2 remains incomplete.

The latest [world sound evidence](../Documentation/Implementation/M2_WORLD_SOUNDS.md) integrates the original built-in Sound
with typed world-owned whole-clip loops. Read [world sound contracts](docs/WORLD_SOUND_CONTRACTS.md).
Preserve immediate deferred source removal, durable success/failure accounting,
source-independent playback and unload ownership. FileStream/TIMESYNC and region
metadata reject playback; priority arbitration and historical VoiceType schemas
remain unavailable. SubwayA has 166 live-created source entries, 4 transferred
world sounds, 7 SelfTriggers, 372 editor-only and 38 unported entries. Runtime
counts remain 173/176 before/after player, 164/167 after nine Keys are consumed.
Earlier blanket Sound-loop omissions are superseded only for this subset; M2 stays open.

The latest [sprite evidence](../Documentation/Implementation/M2_SPRITES.md) integrates original DetailSprite with bounded
SPR manifests, frame clocks, flush placement and independent runtime repeats.
Read [sprite contracts](docs/SPRITE_CONTRACTS.md). Preserve the source's unchanged rotation,
flush-only repeat branch, column order and post-repeat difficulty filtering.
DTX resources are indexed, not decoded or rendered; the overlay is diagnostic.
SubwayA has 178 live-created entries, 4 transferred world sounds, 7 SelfTriggers,
372 editor-only and 26 unported entries, with no failures. Runtime counts are
185/188 before/after player entry, then 176/179 after nine Keys are consumed.
M2 remains incomplete; earlier DetailSprite omissions are superseded only for
this bounded subset.

The latest [scene explosion evidence](../Documentation/Implementation/M2_SCENE_EXPLOSIONS.md) integrates original CClientExplosionSFX
activation/removal and detached one-shot WAV playback. Read [scene explosion contracts](docs/SCENE_EXPLOSION_CONTRACTS.md).
Preserve the original null-damager guard (zero deliveries), cached initial position,
retrigger-before-update behavior and source-independent voice tails. Reports explicitly
mark client FX unavailable; they are not successful FX services. SubwayA now has 187
live-created entries, 4 transferred sounds, 372 editor-only and 17 unported, with no
failures; runtime counts are 194/197 before/after player, then 185/188 after nine Keys.
Required client FX, AI, ExitTrigger and other services keep M2 incomplete.

The latest [character animation evidence](../Documentation/Implementation/M2_CHARACTER_ANIMATION.md) connects the full original
CAnim_Sound animation table to player locomotion, current world-space model node
queries and original hit-sphere measurements. Read [character contracts](docs/CHARACTER_ANIMATION_CONTRACTS.md).
Preserve the >999 previous-slot fallback, untouched slot zero, strafe aliases,
humiliation spelling/order and intermediate-zero ForceAnimation behavior.
Missing head nodes fail explicitly; later-node fallback stays visible. Spheres
are measurements, not active damage/collision services. AI, ExitTrigger, client
FX and complete M2 remain open; SubwayA counts are unchanged.

The latest [character audio evidence](../Documentation/Implementation/M2_CHARACTER_AUDIO.md) connects original CAnim_Sound
voice helpers, player model-key audio and surface/liquid footsteps to real WAV
playback. Read [audio contracts](docs/CHARACTER_AUDIO_CONTRACTS.md). Preserve busy-voice refusal,
player interrupt-before-failure, source 3D speed threshold and left/right alternation,
raw first-container liquid precedence and distinct baked material flags. Typed
movement keeps stale surface messages rejected. AI sound alerts remain explicitly
unavailable; no new AI class or complete M2 gate is claimed.

The latest [character motion evidence](../Documentation/Implementation/M2_CHARACTER_MOTION.md) connects original BaseCharacter model-key motion
and bounded Sparam lookup. Read [motion-key contracts](docs/CHARACTER_MOTION_CONTRACTS.md). Preserve local-up yaw,
source overwrite/order, world-space AddXYZ, centered raw dimensions and the player's
base-handler exclusion. Validate complete numeric groups before mutation; stop on
service failure and preserve deferred touch removal. The seven examined character
assets contain no motion keys; synthetic probes are explicitly labelled, not AI.
AI states/hearing/fire, ExitTrigger, client FX and the actual M2 gate remain open.

The latest [AI model-key evidence](../Documentation/Implementation/M2_AI_MODEL_KEYS.md) connects selected AI_Mgr animation-key
sound dispatch, actual decoded voices and inspection. Read [contracts](docs/AI_MODEL_KEY_CONTRACTS.md).
Preserve source branch priority, server-camera/script early returns, B2_LITE's
1..3 variants, chance draw ordering and class/gender shared voice lifetime.
Fire/show_weapon/extra remain unavailable; diagnostic probes are not registered
world AI. CivilianAI, CultistAI, Gideon, ExitTrigger and the actual M2 gate stay open.

The latest [AI path-manager evidence](../Documentation/Implementation/M2_PATH_MANAGER.md) connects original PathMgr selection
and owned snapshots to shell lifetime and in-game inspection. Read [contracts](docs/PATH_MANAGER_CONTRACTS.md).
Preserve active-only GetNextObject, included SMELL points, unconsumed source objects,
prefix-plus-first-digit matching, reverse enumeration and preflight/no-match output
semantics. Returned paths own their data; source handles are inspection-only.
AI following/actions, remaining classes/services and the M2 gate stay open.

The latest [AI visibility evidence](../Documentation/Implementation/M2_AI_SIGHT.md) connects original AI_Mgr::CanSee
and its VolumeBrush/destructible-glass filter. Read [contracts](docs/AI_SIGHT_CONTRACTS.md). Preserve
body-first rays, head retry only after an obstruction, target-hit requirements and
checked lifetime. Current head poses and hidden/reload inspection are exercised;
range/FOV, target acquisition, states, attacks and production NPCs remain unported.
The 17 unsupported SubwayA entries and actual M2 gate remain unchanged.

The latest [held-model/attachment evidence](../Documentation/Implementation/M2_ATTACHMENTS.md) connects original CWeapon held models,
selected inventory visibility and typed parent/node transforms. Read [contracts](docs/ATTACHMENT_CONTRACTS.md).
Preserve original gun-node/origin fallback, handheld clip, independent child removal,
non-reused IDs and deferred ownership cleanup. Visual children only: chains, physical
children, dropped weapons and client visibility SFX remain unavailable. Hidden/reload
inspection runs outside UI; required AI/ExitTrigger/FX keep M2 incomplete.

The latest [explicit AI target evidence](../Documentation/Implementation/M2_AI_TARGETS.md) connects original named/link selection,
stored-position facing and target MID_LINKBROKEN. Read [contracts](docs/AI_TARGET_CONTRACTS.md).
Preserve same-target no-op, SetTarget's net-zero Metacmd change, local-up yaw,
deferred non-owning target lifetime and checked pose failures. Explicit commands
are not autonomous acquisition or an AI state machine. Source counts and the
actual M2 gate remain unchanged; NPCs, ExitTrigger and required FX remain open.

The [diagnostic 3D world view](../Documentation/Implementation/M2_WORLD_VIEW.md) now renders actual DAT geometry and
current world-model transforms through SDL_GPU. Read [view contracts](docs/WORLD_VIEW_CONTRACTS.md)
before extending rendering, camera presentation or GPU lifetime. Preserve original
coordinates, base rings, current checked visibility, world revisions and the single
player integrator. `--view-world` is navigable inspection, not the M3 playable slice;
texture/model/FX rendering and required NPC/ExitTrigger services remain unavailable. Current
world shaders support Vulkan/SPIR-V only; M2 stays incomplete.

The [DTX world-texture continuation](../Documentation/Implementation/M2_TEXTURES.md) adds bounded version -2 palette decoding, exact-path material caching and opaque SDL_GPU sampling. Read [texture contracts](docs/TEXTURE_CONTRACTS.md) before extending images/materials. Preserve the -2/-5 generation boundary, strict lengths, pre-pivot UVs, world-revision ownership and explicit failed resources. Alpha/sky markers, unlit mip-zero sampling and unsupported version-0 assets remain visible limitations. Models, required original services and the M2 gate remain open.

The [animated model continuation](../Documentation/Implementation/M2_MODEL_VIEW.md) renders checked existing ABC/held/view models with original view offsets and shared world depth. Read [contracts](docs/MODEL_VIEW_CONTRACTS.md) before extending presentation or poses. Preserve read-only clocks, deferred lifetime, hidden/missing inspection, cache revisions and explicit close-view limits. Absent ABC TransformInfo now conjugates normalized node rotations; explicit zero remains direct. Rendering/head queries/attachments share that correction and the file -Z to engine +Z basis conversion; all spatial model fields are normalized once. Lighting/FX, NPCs, ExitTrigger and the actual M2 gate remain open.

The [AI perception continuation](../Documentation/Implementation/M2_AI_PERCEPTION.md) connects original faction lists, FindObjectInRadius, ComputeStimuli, opportunity selection and ExitHint. Read [contracts](docs/AI_PERCEPTION_CONTRACTS.md) before extending NPC acquisition/state updates. Preserve player membership in both factions, source list/priority/boundary rules, dead-target behavior and checked deferred lifetime. Required query failure is -1, never a usable stimulus. Attacker memory is a checked non-owning identity; timed-power storage does not implement pickups/timers. No production NPC is registered; states, attacks, required FX and ExitTrigger keep M2 open.

The [AI state/action continuation](../Documentation/Implementation/M2_AI_STATES.md) adds original transitions, Cultist/Civilian decisions and stationary idle/talk/special commands. Read [contracts](docs/AI_STATE_CONTRACTS.md). Preserve strict idle deadline, last-state exclusions, idle/talk command sharing, special-action random draws and failed-perception guards. Animation resize uses the documented atomic foot-anchored replacement. Diagnostic dispatch is not AI_Update; production NPCs, combat/navigation/death, required FX and ExitTrigger remain unavailable. M2 stays open.

The [AI weapon-action continuation](../Documentation/Implementation/M2_AI_FIRE.md) connects original weapon choice, supported aiming/clearance, standing fire and fire_key delivery. Read [contracts](docs/AI_FIRE_CONTRACTS.md). Preserve raw-ammo selection, strict cadence/range, initial-miss versus head-retry behavior, cached-head writes, dry acceptance and independent completion increments. Player eye alignment must remain player-only. Missing switch columns/services fail; diagnostic firing does not register production NPCs or implement AI_Update. Navigation/death, required FX and ExitTrigger still keep M2 open.

The [AI command/hearing continuation](../Documentation/Implementation/M2_AI_TRIGGERS.md) connects selected original HandleTrigger,
PlayAnimation, SendSoundTrigger and player sound alerts. Read [contracts](docs/AI_TRIGGER_CONTRACTS.md). Preserve
source command early returns, exact hearing boundaries, reversed gunfire class test,
impact guards, two-faction order/duplicates and alert-after-playback-failure. Snapshot
checked identities across callbacks; delivery is not reaction. Scripts and full NPC
initialization/AI_Update/deactivation remain unavailable; M2 stays open.

The [CMovement continuation](../Documentation/Implementation/M2_AI_NAVIGATION.md) adds the original cached transform aggregate
and obstacle-path calculation. Read [contracts](docs/AI_NAVIGATION_CONTRACTS.md). Preserve two side rays,
character-only filtering, zero lateral range on misses, source offsets/partial paths
and accumulated point count. Planning never integrates motion; players do not initialize
this cache. Bounded searches and value ownership are adaptations. Full NPC initialization,
following/actions, shade queries and the actual M2 gate remain open.

The [script movement continuation](../Documentation/Implementation/M2_AI_FOLLOW.md) connects original Script_Walk/Run,
FOLLOWPATH and MOVETOOBJECT helpers, owned arrival actions and velocity commands.
Read [contracts](docs/AI_FOLLOW_CONTRACTS.md). Preserve cached facing timing, initial injury slowdown,
one-point arrival ordering, linked-target lifetime and residual velocity. Failed
partial plans and missing opportunity-fire branches must remain failures. Full
SCRIPT dispatch, production initialization/AI_Update/deactivation, required FX and
ExitTrigger keep M2 incomplete; source-class counts are unchanged.

The [AI script continuation](../Documentation/Implementation/M2_AI_SCRIPT.md) connects original command parsing/preparation,
loop/nonloop sequencing, waits, animation, sound and movement state dispatch.
Read [contracts](docs/AI_SCRIPT_CONTRACTS.md). Preserve one-command cadence, source atoi-prefix rules,
PLAYANIMATIONLOOPING fallthrough, double DONE decisions and owned callback lifetime.
Production SCRIPT entry still requires its unported forced AI_Update; diagnostics
must not count as registered NPCs. Required initialization/update/death, client FX
and ExitTrigger/session services keep M2 incomplete. Source counts are unchanged.

The [autonomous locomotion continuation](../Documentation/Implementation/M2_AI_LOCOMOTION.md) adds original obstruction/ledge probes,
walk/run/moving-fire commands and Cultist close/far attack sequencing. Read
[contracts](docs/AI_LOCOMOTION_CONTRACTS.md). Preserve first-hit self exclusion, false-on-throttled ledge
queries, injury/support timing, independent fire/completion increments and cached
attack thresholds. Missing shade recovery and crawl/jump execution remain explicit
failures. Diagnostic drivers are not production AI_Update; NPC initialization,
remaining states, required FX and ExitTrigger keep M2 incomplete. Counts are unchanged.

The [AI recovery/navigation continuation](../Documentation/Implementation/M2_AI_RECOVERY.md) connects original jump/crouch/crawl,
firing crawl, JumpOver/CrawlUnder, ToPos/Around and visual-search states. Read
[contracts](docs/AI_RECOVERY_CONTRACTS.md). Preserve landing-based completion, cached movement, crawl's
recovery overwrite, arrival/ledge short circuits and independent fire markers.
Failed plans cannot be followed until replanning succeeds. Earlier crawl/jump-body
omissions are superseded; shade recovery, smell/escape/recoil/death, full NPC
initialization/AI_Update, required FX and ExitTrigger keep M2 incomplete.

The [AI escape/guard continuation](../Documentation/Implementation/M2_AI_ESCAPE.md) connects original Cultist/Civilian
escape/hide, GuardLocation and the lateral beg command. Read [contracts](docs/AI_ESCAPE_CONTRACTS.md).
Preserve class-specific hint filters, preceding cached facing, current civilian
dimensions, guard ledge order and residual velocity. Tracking is a checked non-owning
identity, not a new engine link. Diagnostic ray-hit hints do not change authored
marker flags. Earlier escape/guard-body omissions are superseded; smell/recoil/death,
full NPC initialization/AI_Update, required FX and ExitTrigger keep M2 incomplete.

The [production NPC lifecycle continuation](../Documentation/Implementation/M2_AI_RUNTIME.md) registers CultistAI/CivilianAI and connects original initialization/AI_Update.
Read [contracts](docs/AI_RUNTIME_CONTRACTS.md). Preserve class-specific property/table quirks,
source update ordering, deferred child lifetime and independent clocks. Runtime
failures suspend visibly; replacement center-ray activation is not retail PVS.
SubwayA has 202 created source entries, 4 transferred sounds, 372 editor-only,
2 unsupported and no creation failures. Remaining actions/death, Gideon, required
FX and ExitTrigger keep M2 open. User-reported pink train glass and direct movement
failure after intro are retained in the evidence for later investigation.

The [NPC hit/recoil continuation](../Documentation/Implementation/M2_AI_HIT.md) connects original visible-node hit localization,
region damage and nonlethal HandleDamage/MC_Recoil through production AI_Update.
Read [contracts](docs/AI_HIT_CONTRACTS.md). Preserve NORMAL=0 random fallback including raw node 18,
unwrapped side-angle difference, Cabal head kill before armor, strict limp threshold,
remaining-HP/accumulated-damage recoil threshold and Civilian EscapeRun override.
Missing later nodes may reuse the prior transform; invalid poses must fail before
health mutation. Refused/nested packets cannot replay stale reactions. Lethal
corpse/drop/gib services remain explicit failures; NPC impulses, hidden limbs,
Gideon, ExitTrigger and required FX keep M2 incomplete.

The [NPC weapon-drop continuation](../Documentation/Implementation/M2_WEAPON_DROPS.md) connects original Cabal
DropCurrentWeapon, detached hand physics, BerettaPU and player collection.
Read [contracts](docs/WEAPON_DROP_CONTRACTS.md). Preserve left-first selection, melee/busy refusal,
checked independent lifetime, one-second initial support check and ammo-dependent
pickup acceptance. Failed transfers stay inspectable. Player drop controls,
corpses/gibs, required FX, Gideon and ExitTrigger remain unavailable; M2 stays open.
