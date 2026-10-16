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
