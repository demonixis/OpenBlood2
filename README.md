# Blood II Reconstruction aka OpenBlood2

Latest preservation continuation: [original damage Save/Load](../Documentation/Implementation/M2_DAMAGE_STATE.md)
adds owned health/armor/death state and original death-delay/nigh-invulnerability
rules. Exit inspection now captures inventory and damage separately; full player
restoration, client association and safe destination entry remain pending.
**NEW_LEVEL is still refused and M2 remains incomplete.**

Latest inventory continuation: [original Save/Load subset](../Documentation/Implementation/M2_INVENTORY_STATE.md).
Exit requests now capture owned inventory state; original keep-alive restoration
removes keys and recreates weapons. Full player preservation/client association
and safe destination entry remain unavailable, so NEW_LEVEL stays refused and
**M2 stays incomplete**. Earlier blanket inventory-save omissions are superseded
only for this aggregate subset.

Latest continuation: [original ExitTrigger and shell exit requests](../Documentation/Implementation/M2_EXITS.md).
SubwayA now has one unported entry (Gideon). NEW_LEVEL still requires unavailable
KeepAliveSave/Load and is explicitly refused while retaining the current player and
inventory. Earlier ExitTrigger omissions below are historical; **M2 remains open**.

Previous continuation: [ordinary NPC death and CCorpse](../Documentation/Implementation/M2_CORPSES.md)
connects independent animated corpses and real model-key sounds. Earlier blanket
corpse omissions below are historical. Fire/gibs, corpse damage, required FX,
Gideon and ExitTrigger still keep M2 incomplete.

The inspector is now **collapsible and resizable**: click the title arrow to fold
or unfold it, drag its borders to resize, or use F1 to hide/show it. After entering
the player in Session, keyboard movement is enabled by default: **WASD/ZQSD**,
**Shift** run, **Space** jump, **Ctrl** crouch, **arrows** look, **E** USE. **F2** toggles
keyboard controls. SubwayA's original intro temporarily blocks movement; wait for
it to end. See [input/UI evidence](../Documentation/Implementation/M2_CONTROLS.md).

Previous death prerequisite: [original NPC weapon drops](../Documentation/Implementation/M2_WEAPON_DROPS.md)
connects detached Beretta physics, original pickups and player collection. Corpses/gibs,
required FX, Gideon and ExitTrigger still keep M2 incomplete.

Previous combat continuation: [original NPC hit localization and recoil](../Documentation/Implementation/M2_AI_HIT.md)
connects hit regions, limb damage, injured legs and nonlethal reactions to production
Cultist/Civilian updates, with in-game inspection. Corpse/gib services, required
FX, Gideon and ExitTrigger still keep M2 incomplete.

Production lifecycle foundation: [original Cultist/Civilian initialization and AI_Update](../Documentation/Implementation/M2_AI_RUNTIME.md)
registers the fifteen authored SubwayA NPCs, with original properties, animation,
inventory, activation and state dispatch. Remaining combat/death actions, required
FX, Gideon and ExitTrigger still keep M2 incomplete.

A new, independent C++20 implementation. **M0/M1 are complete for their documented Linux scope; M2 is in progress.** The app now mounts REZ archives, reads DAT v56 geometry and typed properties, and runs a supported subset of original objects in an in-game inspector. ABC skeletal/vertex animation, model callbacks and primary-world collision are implemented. Selected original shell/CPlayerObj entry, inventory, Beretta/knife animation and vector-fire/damage branches and original MoveMgr dry-land movement, stairs, crouch and body locomotion animations, containers, liquids, ladders, conveyors and damage/freefall volumes, original sliding doors, translating world-model collision, Trigger/SelfTrigger, key inventory, KeyPickup, original player USE, RotatingDoor, RotatingBrush, selected CDestructableBrush damage/removal and original CDebris explosive area damage and BoxPhysics pushable/gravity brushes with original damage impulses are connected. Original ammo/health/armor pickups and destruction spawns extend this subset; complete player gameplay remains missing. This is not a playable game.

The three runtime targets remain small: `engine` (resources, world/object runtime, configuration, SDL_GPU), `blood2_game` (original code and typed services) and `blood2_app` (`blood2`, optional Dear ImGui). No previous prototype is a build dependency, and no EnTT/ECS was introduced.

The [diagnostic 3D view](docs/WORLD_VIEW_CONTRACTS.md) now renders actual level
geometry and current moving brushes through SDL_GPU. Run `./build/debug/blood2 --view-world` to enter the original player/cameras with
keyboard controls enabled. F1 opens inspection. [Original DTX textures](docs/TEXTURE_CONTRACTS.md) now appear as unlit opaque materials. [Existing models and first-person weapons](docs/MODEL_VIEW_CONTRACTS.md) now render too;
full combat and M2 completion remain pending. The world shader currently requires
Vulkan/SPIR-V; other backend artifacts remain unimplemented.

## Build and Run

Requirements: CMake 3.28+, Ninja, a C++20 compiler and SDL's platform development dependencies. Only Linux/Wayland/Vulkan has been tested. Windows/D3D12 and macOS/Metal remain target configurations, not verified platforms.

```sh
cmake --preset debug
cmake --build --preset debug --parallel
./build/debug/blood2 --data-dir "/path/to/Blood II"
```

Dependency archives and SHA-256 pins are in [Dependencies.cmake](cmake/Dependencies.cmake). SDL uses the selected `main` commit with upstream OpenXR support; the app does not start an XR session. Downloads/build products are ignored under `.cache/` and `build/`.

This workspace's ignored `blood2.local.ini` already configures `/home/yann/.steam/steam/steamapps/common/Blood II/`:

```sh
./build/debug/blood2
./build/debug/blood2 --view-world
./build/debug/blood2 --inspect-world
./build/debug/blood2 --inspect-player
./build/debug/blood2 --world 02_CityHub
./build/debug/blood2 --world 01_SubwayA --frames 3 --capture build/world.png
```

The default world is `01_SubwayA`: 580 source objects and 116 BSP models. **202 source entries** create, plus **4 transferred world sounds** and **7 owned SelfTriggers**. There are **372 preprocessor-only entries**, **2 unported runtime entries** (Gideon and ExitTrigger), and zero creation failures. Runtime counts before/after player entry are **224/227**, including fifteen NPC muzzle lights. After the first update consumes nine Keys and creates ten NPC held weapons plus the player's held Beretta, the count is **229** with **11 attachments**. These are lifecycle counts, not complete gameplay acceptance. See [NPC evidence](../Documentation/Implementation/M2_AI_RUNTIME.md); earlier sections below retain historical counts.

The World tab shows source properties, status, model selection and a diagnostic wireframe. Objects shows actual original instance state and spawn/remove controls. Resources shows archive precedence, indexed paths and missing world references. Diagnostics shows reached services. Reload/unload operations release original objects/strings and invalidate stale selection. Press **F1** to toggle inspection; Escape quits when UI does not capture input. Models previews the decoded Caleb skeleton, coat deformation, 86 animations and markers; it is not a CPlayerObj instance. Services shows collision index, gravity and original console variables. Session enters and inspects the actual CPlayerObj subset, client association, attributes, health/ammo, spawn and exit. Weapons exposes owned slots/hands, ammo, primary/alternate firing, actual weapon animation, hit/damage traces and sound resource requests. Movement shows the real player body/ABC pose, requested and resolved clips, dimensions, crouch clearance, stair attempts, ground contacts and a local collision map. Enable keyboard movement with **F2**: physical WASD/ZQSD, Shift to run, Space to jump, Ctrl to crouch, arrows to look, **E to USE** (one command per press). Text editing/focus loss suppress input; hiding with F1 keeps keyboard movement running. Containers lists original volumes, geometry, hidden state, body/head contents, air and water/ladder motion; it offers queued triggers and checked position edits. Space/Ctrl move up/down in free movement. Doors inspects the original state machine, transformed geometry, pusher/rider results and queued TRIGGER/TRIGGERCLOSE. Triggers inspects active/inactive sensors, target matches, delays, filters, locks and player keys; Doors links to its SelfTrigger. World key pickups retain resource diagnostics. Session shows the original 82-unit USE ray and target; Doors also shows angular motion, the hinge and posed collision geometry. Brushes shows rotating/destructible geometry, angular ramps, blocking/rider counts, health gates, queued ON/OFF/TOGGLE/KILL and retained destruction requests. Inspector commands test the direct message path. Door audio and portal visibility remain pending. Liquid surfaces/fog/audio remain pending effect requests. World textures and a complete game session remain unimplemented; cinematic, random-emitter and world-loop audio are covered by the later continuations below. Sprites shows original placement/repeats, manifest metadata and frame clocks, with a diagnostic overlay; DTX textures are indexed but not decoded.

## Configuration and Modes

Copy [config/blood2.example.ini](config/blood2.example.ini) to ignored `blood2.local.ini`, or use `--config FILE`. Relative data paths are resolved against the config file's directory. Precedence: **`--data-dir` → `BLOOD2_DATA_DIR` → config**. Paths use UTF-8; explicitly missing/invalid configs fail even with another data-root source.

| Option | Behavior |
| --- | --- |
| `--view-world` | Enter diagnostic 3D with original player/cameras, hidden inspector and keyboard controls |
| `--viewport-smoke` | Check textured world/view-weapon rendering, movement, hidden UI, resize and reload |
| `--check-data` | File metadata/readability only; no parser or GPU |
| `--inspect-world` | Headless REZ/DAT parsing and original subset creation/teardown |
| `--world NAME` | Resource name, default `01_SubwayA`; exact normalized lookup, no traversal/basename fallback |
| `--start-game` | Checks session startup and currently **fails with code 6** on missing original runtime services |
| `--no-inspector` | Hide development UI; simulation remains independent |
| `--frames N` / `--capture FILE.png` | Bounded graphical run / third-frame SDL_GPU readback |
| `--gpu-smoke` | M1 synthetic object/GPU lifecycle checks |
| `--world-smoke` | Real-world GPU, UI toggle, resize, unload/reload and teardown checks |
| `--inspect-player` | Headless original shell/player entry inspection, with explicit gameplay blockers |
| `--player-smoke` | GPU Session panel, hidden player updates, unload/re-entry checks |
| `--weapons-smoke` | GPU Beretta/knife firing while hidden, unload/re-entry; capture at frame 181 |
| `--cinematics-smoke` | GPU original cue, decoded PCM/SDL output, hidden UI and reload (tests use dummy audio) |
| `--worldpaths-smoke` | GPU original SubwayA train translation, collision and reload with UI hidden |
| `--lights-smoke` | GPU original dynamic-light state/KeyFramer placement while hidden, Lights inspection and reload |
| `--cameras-smoke` | GPU original camera tracking, client view/player gates while hidden, inspection and reload |
| `--objectives-smoke` | GPU original objective delivery and localized client text while hidden, inspection and reload |
| `--pickups-smoke` | GPU original ammo collection while hidden, Ward inspection and reload |
| `--brushes-smoke` | GPU original brush ON/OFF, hidden rider transport, explosive chain and dynamic box gravity/impulse, inspection and reload |
| `--interaction-smoke` | GPU SDL E press-edge USE, missing-key denial/unlock, synthetic rotating hinge, hidden motion and reload |
| `--triggers-smoke` | GPU missing-key denial, real key pickup, hidden unlock/door movement, resize and reload |
| `--doors-smoke` | GPU original Door commands, hidden translation, resize, unload/reload; capture at frame 15 |
| `--movement-smoke` | GPU SDL walk/run/jump/crouch, live body pose and hidden stance changes, unload/re-entry; capture at frame 181 |
| `--containers-smoke` | GPU volume simulation while hidden and unload/re-entry: `--world 02_CityHub` (canal/air) or `--world 01_SubwayA` (conveyor), capture at frame 15 |
| `--help` | All options |

Mount order is base, sound, patch P, patch P2; later exact paths win. Nightmare Levels is excluded. Content stays external and is never modified/extracted into the source project. Exit codes: 0 success; 2 config/content metadata; 3 GPU/platform/capture; 4 object/smoke; 5 world/resource decode; 6 incomplete game-session startup.

## Tests

```sh
ctest --preset debug
ctest --preset debug -L world
```

The current suite has **283 synthetic headless tests**, **51 private headless tests**
and **27 serial GPU tests** (361 total with the installation and GPU options enabled).
Coverage includes bounded readers, original object/service behavior, player/weapon
lifetime, movement, interactions, animation/attachments and real GPU presentation.
Synthetic fixtures never substitute for private resource acceptance.

Enable installation-specific tests and real GPU windows explicitly:

```sh
cmake --preset debug -DBLOOD2_GPU_TESTS=ON -DBLOOD2_TEST_DATA_DIR="/path/to/Blood II"
cmake --build --preset debug --parallel
ctest --preset debug
```

GPU captures are written under `build/debug/tests/`, including `viewport.png`
(first person) and `model-private.png` (isolated Caleb/held-model diagnostic).
The ignored local presets enable these options:

```sh
cmake --preset local-debug
cmake --build --preset debug -j8
ctest --preset debug
cmake --preset local-release
cmake --build --preset release -j8
ctest --preset release
```

Public `release` disables Dear ImGui. It still loads the world and executes original object callbacks but presents a clear background until game rendering is implemented. `BLOOD2_SANITIZERS=ON` instruments project code with ASan/UBSan; [M2 evidence](../Documentation/Implementation/M2.md) gives Clang commands and actual results. Sanitizer checks do not cover a GPU session or its driver.

On Windows use an MSVC Developer PowerShell and the same CMake/Ninja commands, then `build/debug/blood2.exe`; on macOS use Apple Clang. These procedures, mobile and headset builds have not been executed.

## Contracts and Remaining Work

Read [world/session contracts](docs/WORLD_CONTRACTS.md), [object contracts](docs/OBJECT_CONTRACTS.md), [provenance](docs/PROVENANCE.md) and [M2 results](../Documentation/Implementation/M2.md). The [roadmap's M2 gate](../Documentation/Roadmap/03_MILESTONES.md#m2--connect-world-session-and-gameplay-services) remains open: complete player/gameplay callbacks, required world classes, broader mover/physics behavior and remaining original services must still be connected. See the [service continuation](../Documentation/Implementation/M2_SERVICES.md) for the new tested subset. M3 has not started.

The [pickup continuation](../Documentation/Implementation/M2_PICKUPS.md) records the preceding pickup stage; read [PICKUP_CONTRACTS.md](docs/PICKUP_CONTRACTS.md) before extending collection or destruction spawns. The [pushable continuation](../Documentation/Implementation/M2_PUSHABLES.md) records the preceding stage; read [PUSHABLE_CONTRACTS.md](docs/PUSHABLE_CONTRACTS.md) before extending body physics, pushing or impulses. The [explosion continuation](../Documentation/Implementation/M2_EXPLOSIONS.md) records the preceding stage; read [EXPLOSION_CONTRACTS.md](docs/EXPLOSION_CONTRACTS.md) before extending debris, area damage or chain reactions. Its effects are inspection requests; client FX/audio remain pending. The [brush continuation](../Documentation/Implementation/M2_BRUSHES.md) records the preceding stage; read [BRUSH_CONTRACTS.md](docs/BRUSH_CONTRACTS.md) before extending rotators, brush damage or debris. The [USE/rotation continuation](../Documentation/Implementation/M2_INTERACTION.md) records the preceding stage; read [INTERACTION_CONTRACTS.md](docs/INTERACTION_CONTRACTS.md) before extending USE, rotating collision or player door pushes. The [trigger/key continuation](../Documentation/Implementation/M2_TRIGGERS.md) records the preceding stage; read [TRIGGER_CONTRACTS.md](docs/TRIGGER_CONTRACTS.md) before extending activation, keys or SelfTrigger ownership. The [door continuation](../Documentation/Implementation/M2_DOORS.md) records the previous stage; read [DOOR_CONTRACTS.md](docs/DOOR_CONTRACTS.md) before extending world-model motion or door commands. The [specialized volume continuation](../Documentation/Implementation/M2_SPECIALIZED_VOLUMES.md) records the preceding stage (Conveyor, Damage, FreeFall and level-entry damage protection). Read [SPECIALIZED_VOLUME_CONTRACTS.md](docs/SPECIALIZED_VOLUME_CONTRACTS.md) before extending these branches. The [container continuation](../Documentation/Implementation/M2_CONTAINERS.md) records the preceding stage. Read [CONTAINER_CONTRACTS.md](docs/CONTAINER_CONTRACTS.md) for original VolumeBrush/MoveMgr/air rules, static shell queries and pending effects. The [locomotion continuation](../Documentation/Implementation/M2_LOCOMOTION.md) records the prior stage. Read [LOCOMOTION_CONTRACTS.md](docs/LOCOMOTION_CONTRACTS.md) for source clip fallbacks, atomic resize and the replacement stair solver. Read [MOVEMENT_CONTRACTS.md](docs/MOVEMENT_CONTRACTS.md) for the original MoveMgr subset, shared body, timing and grounding adaptations, and the subsequent translating platform extension. The [weapon continuation](../Documentation/Implementation/M2_WEAPONS.md) retains the earlier weapon evidence. Read [WEAPON_CONTRACTS.md](docs/WEAPON_CONTRACTS.md) for ownership, original behavior and deliberate limits. Read [PLAYER_CONTRACTS.md](docs/PLAYER_CONTRACTS.md) before extending the source subsets, typed client messages or player lifetime. Entry inspection succeeds; `--start-game` still fails with missing gameplay services.

## Source conventions

Runtime C++ namespaces use `OpenBlood2`; original global game/SDK class
names are preserved. Follow [.clang-format](.clang-format) and
[.editorconfig](.editorconfig). The current formatting was applied and checked
with clang-format 23.1.3 (a development tool, not a runtime dependency). Commit
each completed implementation run with its tests and English documentation.

The Pickups tab inspects all supported ammo/health/armor objects, including hidden/inactive or missing-resource entries, player pools/caps and the latest 32 pickup/spawn reports. Audio and pickup FX remain requests/metadata, not playback.

## Mission objectives continuation

[Increment evidence](../Documentation/Implementation/M2_OBJECTIVES.md) and
[contracts](docs/OBJECTIVES_CONTRACTS.md) cover original ObjectivesTrigger,
typed client broadcasts and bounded PE resource-string lookup from external
cres.dll. The Objectives tab shows source objects, delivery/resolution state and
actual localized text. Use `./build/debug/blood2 --objectives-smoke` for the hidden
UI/reload check. Original HUD and audio are pending; M2 remains incomplete.

## Scripted camera continuation

[Increment evidence](../Documentation/Implementation/M2_CAMERAS.md) and
[contracts](docs/CAMERA_CONTRACTS.md) cover original CameraObj activation/tracking,
typed client camera selection and player movement/USE/fire restrictions. Cameras
shows both source and published listener flags, target/pose, timers and player
restrictions; diagnostic ON/OFF/LINK controls use original messages. Run
`./build/debug/blood2 --cameras-smoke` for hidden-UI and reload checks. The selected
view, visibility and audio listener are inspection data/requests; cinematic world
rendering, KeyFramer/CinematicTrigger and audio remained pending at this snapshot. Its SubwayA
accounting is 144 source instances + 7 SelfTriggers, 154 with player/owned models,
372 editor-only entries, 64 unported and zero failed. M2 is still open.

Earlier increment: [original keyframed paths](../Documentation/Implementation/M2_KEYFRAMERS.md)
connect KeyFramer/KeyData, camera motion and owned consumed keys. The Keyframes tab
shows copied timelines, target transforms/results and unresolved script/sound
requests. Run `./build/debug/blood2 --keyframes-smoke` for hidden-UI/reload checks.
[Contracts](docs/KEYFRAMER_CONTRACTS.md) preserve original timing/rotation quirks.
SubwayA has 148 created source entries + 7 SelfTriggers, 372 editor-only, 60 unported,
zero failed. Nine Keys disappear after copying: live counts are 155 before player
entry, 158 after entry, then 146/149 after initialization. World/container path
motion, CinematicTrigger, sound and full M2 acceptance remain pending.

## Original dynamic lights

[Light evidence](../Documentation/Implementation/M2_LIGHTS.md) and
[contracts](docs/LIGHT_CONTRACTS.md) cover ClientLightFX, thirteen original presets
and client CLightFX waves. Lights exposes hidden/off sources, lifecycle/damage,
parameters/plots, evaluated color/radius/position, an XZ placement overlay and
queued ON/OFF/TOGGLE/KILL. Twelve authored SubwayA GlowingLight objects now follow
the original sublight KeyFramer. `./build/debug/blood2 --lights-smoke` checks them
while UI is hidden and after reload. The computed point-light state does not yet
illuminate the world; shadows/fog, general SFX/audio, DirLight spots and the implicit
main light remain pending. M2 stays incomplete.

## Keyframed train translation

[Latest evidence](../Documentation/Implementation/M2_WORLDPATHS.md) and
[path contracts](docs/WORLDPATH_CONTRACTS.md) extend original KeyFramer to static
BoxPhysics brushes with unchanged rotation. Primary-world obstruction and
pusher/rider motion are atomic; original time/endpoint rules remain unchanged.
Keyframes now exposes queued ON/OFF, brush bounds/blockers and a path/collision
plan view. `./build/debug/blood2 --worldpaths-smoke` checks SubwayA's train while
hidden and after reload. Two full paths record 297 moves and two primary-world
blocks each; this is replacement physics, not verified retail train behavior.
Angular paths, moving containers, other required classes/services and M2 remain open.

## Original cinematic cues and audio

[Cinematic contracts](docs/CINEMATIC_CONTRACTS.md) connect original cue sequencing
to actual WAV sample duration, dry local/attached mixing and SDL playback. The
Cinematics tab inspects inactive controllers, all cues, voices, PCM and failures.
Interactive runs open the default audio device; output failure is visible and
leaves offline timing. Headless tests avoid output; the device/GPU checks use SDL's
dummy driver. Reverb, built-in Sound loops/streaming, other audio callers and
music remain unavailable. This is not complete cinematic rendering or M2 acceptance.

## Original random sound emitters

RandomSoundFX now preserves original interval-based random/sequential playback,
fixed X/Z placement and ON/OFF/TOGGLE behavior through typed owned voices. Ambient
attenuates with distance without panning; positional audio uses the existing dry
spatial mixer. OFF lets the current voice finish. The Random sounds tab exposes
hidden/inactive emitters, duplicate-name object IDs, ignored slots, missing resources,
schedules, decoded voice state and output diagnostics. Built-in Sound loops,
streaming, priority arbitration, reverb/Doppler and complete M2 gameplay remain pending.
See [contracts](docs/RANDOM_SOUND_CONTRACTS.md) and [source ledger](docs/M2_RANDOM_SOUND_SOURCES.json).

```sh
ctest --test-dir build/debug -R 'randomsound' --output-on-failure
SDL_AUDIODRIVER=dummy ./build/debug/blood2 --random-sounds-smoke --capture build/random-sounds.png
```

The [world sound continuation](../Documentation/Implementation/M2_WORLD_SOUNDS.md) adds original built-in Sound loops that
outlive their consumed source objects, plus inspection and bounded playback checks.
At that increment, SubwayA had 166 live-created entries, 4 world-owned sound entries, 372 editor-only
and 38 unported entries; runtime counts were 173/176 before/after player. Streaming
and historical VoiceType schemas remain unsupported. M2 remains incomplete.

Use `--world-sounds-smoke --capture build/debug/world-sounds.png` with the configured
content root for the loop ownership/output GPU scenario (silent validation:
`SDL_AUDIODRIVER=dummy`). Contracts: [world-owned sound](docs/WORLD_SOUND_CONTRACTS.md).

## Original DetailSprite and SPR inspection

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

Use `--sprites-smoke --capture build/debug/sprites.png` with the configured external
content root to verify the twelve authored sprites, hidden-inspector clocks and
unload/reload. SPR frames resolve exactly; DTX decoding and world rendering remain
unavailable. Full gameplay still exits with the explicit M2 gate failure.

The latest [scene explosion evidence](../Documentation/Implementation/M2_SCENE_EXPLOSIONS.md) integrates original CClientExplosionSFX
activation/removal and detached one-shot WAV playback. Read [scene explosion contracts](docs/SCENE_EXPLOSION_CONTRACTS.md).
Preserve the original null-damager guard (zero deliveries), cached initial position,
retrigger-before-update behavior and source-independent voice tails. Reports explicitly
mark client FX unavailable; they are not successful FX services. SubwayA now has 187
live-created entries, 4 transferred sounds, 372 editor-only and 17 unported, with no
failures; runtime counts are 194/197 before/after player, then 185/188 after nine Keys.
Required client FX, AI, ExitTrigger and other services keep M2 incomplete.

Use `--scene-explosions-smoke --capture build/debug/scene-explosions.png` for the
private GPU/audio check (SDL dummy output is used by CTest).

Latest M2 continuation: [original character animations and model nodes](../Documentation/Implementation/M2_CHARACTER_ANIMATION.md)
connects the original animation table to locomotion and exposes current skeletal
transforms and original sphere measurements. AI/damage-localization consumers,
ExitTrigger, client FX and the actual M2 exit gate remain incomplete.

Latest M2 continuation: [original character audio and footsteps](../Documentation/Implementation/M2_CHARACTER_AUDIO.md)
adds attached voices, original animation-key playback and material/liquid selection.
AI hearing, remaining classes/services and the M2 acceptance gate are still open.

Latest M2 continuation: [original character motion keys](../Documentation/Implementation/M2_CHARACTER_MOTION.md)
adds bounded bracket parameters, source-ordered rotation/velocity/displacement/dimensions
and truthful inspection. Synthetic motion and actual private-key coverage are
separate; remaining AI/classes/services keep M2 incomplete.

The preceding character-motion increment passed 290/290 tests in both Debug and inspector-free Release,
and 267/267 sequential headless tests with Clang ASan/UBSan/leak checks on Linux.

Latest M2 continuation: [original AI animation-key audio](../Documentation/Implementation/M2_AI_MODEL_KEYS.md)
executes 140 sound markers from six real NPC models through the original dispatch
rules. Fire/extra actions and complete NPC behavior remain unavailable; the
17 unsupported SubwayA entries and actual M2 acceptance gate are unchanged.

The AI model-key increment passed 300/300 tests in Debug and inspector-free
Release, plus 277/277 sequential headless checks with Clang ASan/UBSan/leak
detection on Linux. The diagnostic GPU probe preserves actual NPC model keys.

Latest M2 continuation: [original AI path selection](../Documentation/Implementation/M2_PATH_MANAGER.md)
connects PathMgr, owned route data, active enumeration and Session inspection.
SubwayA's seven points select in the original reverse enumeration order. AI
following/action delivery and the unchanged M2 exit gate remain incomplete.

The AI path-manager increment passed 307/307 tests in both Debug and inspector-free
Release, plus 284/284 sequential headless checks with Clang ASan/UBSan/leak
detection on Linux. Seven real SubwayA points are checked across hidden inspection
and reload; AI path following remains unavailable.

Latest M2 continuation: [original AI visibility](../Documentation/Implementation/M2_AI_SIGHT.md) connects body/head
ray checks, source volume/glass filtering and current-pose inspection. No NPC
state machine or target acquisition is claimed; the complete M2 gate remains open.

The AI visibility increment passed 315/315 tests in both Debug and inspector-free
Release, plus 292/292 sequential headless checks with Clang ASan/UBSan/leak
detection on Linux. The private catalog covers 240 animation midpoints across
six NPC models; GPU inspection verifies hidden queries and fresh reload identities.

Latest M2 continuation: [original held weapons and attachments](../Documentation/Implementation/M2_ATTACHMENTS.md) adds
Beretta/knife hand models, current-pose attachment transforms and in-game inspection.
The source's missing-gun-node fallback remains visible in diagnostics. Dropped weapons,
client visibility SFX, required NPC classes and complete M2 remain unavailable.

The held-model/attachment increment passed 323/323 tests in both Debug and
inspector-free Release, plus 300/300 sequential headless checks with Clang
ASan/UBSan/leak detection on Linux. Seven private character models cover 652
clip/hand samples, including 160 explicit missing-gun-node origin fallbacks.

Latest M2 continuation: [original explicit AI targets and facing](../Documentation/Implementation/M2_AI_TARGETS.md) adds
named target selection, checked links, stored-position/local-up orientation and
inspection. Target departure/re-entry is exercised without owning the target.
Autonomous perception, NPC states and the actual M2 acceptance gate remain open.

The [animated model continuation](docs/MODEL_VIEW_CONTRACTS.md) adds real ABC/held/first-person weapon drawing, original view offsets and a corrected legacy node-rotation default. Hidden/missing objects remain inspectable. Required NPCs, transitions and client FX still block M2 completion.

The animated-model continuation passes 350/350 tests in both Debug and inspector-free
Release, including 27 GPU cases, plus 116/116 targeted Clang ASan/UBSan/leak checks.
The final reader normalizes inverse rotations and model -Z into the engine basis.

The [AI perception continuation](docs/AI_PERCEPTION_CONTRACTS.md) passes 361/361
Debug and inspector-free Release checks, including 27 GPU cases, and all 334
headless ASan/UBSan/leak cases. Player membership, candidate/stimulus rules and
checked attacker lifetime are exercised; required original NPC states and the
complete M2 gate remain open.

The [AI state/action continuation](docs/AI_STATE_CONTRACTS.md) passes 372/372 Debug
and inspector-free Release checks (27 GPU cases), and 115 targeted headless
ASan/UBSan/leak checks. Stationary action completion, state decisions, failure and
lifetime are tested; production NPCs and the complete M2 gate remain open.

The [AI weapon-action continuation](docs/AI_FIRE_CONTRACTS.md) passes 384/384 Debug
and inspector-free Release checks (27 GPU), plus 136 targeted headless ASan/UBSan/
leak checks. Selection, clearance, timed/key firing, real damage and deferred removal
are verified for the bounded path; required production NPCs and M2 remain open.

The [AI command/hearing continuation](docs/AI_TRIGGER_CONTRACTS.md) connects original trigger commands,
two-faction sound broadcasts and actual player alerts. Search/dodge/attack selection
is inspectable but their complete NPC actions and the M2 gate remain unavailable.

Command/hearing verification: 393/393 Debug and inspector-free Release checks
(27 GPU each), 145 targeted ASan/UBSan/leak checks, and 19/19 final Debug hearing/audio
checks after numeric-validation refinements. See the [evidence](../Documentation/Implementation/M2_AI_TRIGGERS.md).

The [CMovement continuation](docs/AI_NAVIGATION_CONTRACTS.md) adds original transform caching and obstacle
path planning, with inspectable partial failures and bounded searches. Following,
production NPCs and the actual M2 session gate remain incomplete.

CMovement verification: 402/402 Debug and inspector-free Release checks (27 GPU
each), plus 162 targeted ASan/UBSan/leak checks. See [evidence](../Documentation/Implementation/M2_AI_NAVIGATION.md).

The [script movement continuation](../Documentation/Implementation/M2_AI_FOLLOW.md) connects original Script_Walk/Run,
FOLLOWPATH and MOVETOOBJECT helpers, owned arrival actions and velocity commands.
Read [contracts](docs/AI_FOLLOW_CONTRACTS.md). Preserve cached facing timing, initial injury slowdown,
one-point arrival ordering, linked-target lifetime and residual velocity. Failed
partial plans and missing opportunity-fire branches must remain failures. Full
SCRIPT dispatch, production initialization/AI_Update/deactivation, required FX and
ExitTrigger keep M2 incomplete; source-class counts are unchanged.

Script movement verification: 413/413 Debug and inspector-free Release checks
(27 GPU each), plus 88 targeted ASan/UBSan/leak checks.
See [evidence](../Documentation/Implementation/M2_AI_FOLLOW.md).

The [AI script continuation](../Documentation/Implementation/M2_AI_SCRIPT.md) connects original command parsing/preparation,
loop/nonloop sequencing, waits, animation, sound and movement state dispatch.
Read [contracts](docs/AI_SCRIPT_CONTRACTS.md). Preserve one-command cadence, source atoi-prefix rules,
PLAYANIMATIONLOOPING fallthrough, double DONE decisions and owned callback lifetime.
Production SCRIPT entry still requires its unported forced AI_Update; diagnostics
must not count as registered NPCs. Required initialization/update/death, client FX
and ExitTrigger/session services keep M2 incomplete. Source counts are unchanged.

Script interpreter-state verification: 422/422 Debug and inspector-free Release
checks (27 GPU each), plus 97 targeted ASan/UBSan/leak checks.
See [evidence](../Documentation/Implementation/M2_AI_SCRIPT.md).

The [autonomous locomotion continuation](../Documentation/Implementation/M2_AI_LOCOMOTION.md) adds original obstruction/ledge probes,
walk/run/moving-fire commands and Cultist close/far attack sequencing. Read
[contracts](docs/AI_LOCOMOTION_CONTRACTS.md). Preserve first-hit self exclusion, false-on-throttled ledge
queries, injury/support timing, independent fire/completion increments and cached
attack thresholds. Missing shade recovery and crawl/jump execution remain explicit
failures. Diagnostic drivers are not production AI_Update; NPC initialization,
remaining states, required FX and ExitTrigger keep M2 incomplete. Counts are unchanged.

Autonomous locomotion/attack verification: 433/433 Debug and inspector-free Release
checks (27 GPU each), plus 120 targeted ASan/UBSan/leak checks.
See [evidence](../Documentation/Implementation/M2_AI_LOCOMOTION.md).

The [AI recovery/navigation continuation](../Documentation/Implementation/M2_AI_RECOVERY.md) connects original jump/crouch/crawl,
firing crawl, JumpOver/CrawlUnder, ToPos/Around and visual-search states. Read
[contracts](docs/AI_RECOVERY_CONTRACTS.md). Preserve landing-based completion, cached movement, crawl's
recovery overwrite, arrival/ledge short circuits and independent fire markers.
Failed plans cannot be followed until replanning succeeds. Earlier crawl/jump-body
omissions are superseded; shade recovery, smell/escape/recoil/death, full NPC
initialization/AI_Update, required FX and ExitTrigger keep M2 incomplete.

AI recovery/navigation verification: 447/447 Debug and inspector-free Release
checks (27 GPU each), plus 134 targeted ASan/UBSan/leak checks.
See [evidence](../Documentation/Implementation/M2_AI_RECOVERY.md).

The [AI escape/guard continuation](../Documentation/Implementation/M2_AI_ESCAPE.md) connects original Cultist/Civilian
escape/hide, GuardLocation and the lateral beg command. Read [contracts](docs/AI_ESCAPE_CONTRACTS.md).
Preserve class-specific hint filters, preceding cached facing, current civilian
dimensions, guard ledge order and residual velocity. Tracking is a checked non-owning
identity, not a new engine link. Diagnostic ray-hit hints do not change authored
marker flags. Earlier escape/guard-body omissions are superseded; smell/recoil/death,
full NPC initialization/AI_Update, required FX and ExitTrigger keep M2 incomplete.

AI escape/guard verification: 458/458 Debug and inspector-free Release checks
(27 GPU each), plus 145 targeted ASan/UBSan/leak checks.
See [evidence](../Documentation/Implementation/M2_AI_ESCAPE.md).

Production NPC lifecycle verification: 472/472 Debug and inspector-free Release
checks (27 GPU each), plus 179 targeted ASan/UBSan/leak checks. See
[evidence](../Documentation/Implementation/M2_AI_RUNTIME.md) for the no-player
self-target boundary and remaining M2 requirements.

NPC hit/recoil verification: 481/481 Debug and inspector-free Release checks
(27 GPU each), plus 188 targeted ASan/UBSan/leak checks. See
[evidence](../Documentation/Implementation/M2_AI_HIT.md) for source quirks,
nonlethal production recoil and the remaining death/M2 boundary.

The [NPC weapon-drop continuation](../Documentation/Implementation/M2_WEAPON_DROPS.md) connects the original Cabal drop,
independent falling Beretta, original pickup and player collection. Corpse/gib
services, player drop controls and the full M2 exit gate remain open.
