# M9 multiplayer

Authorized on 2026-10-30. This contract describes the accepted destination, not a
claim that network gameplay is already implemented. See the maintained
[implementation record](../implementation/reports/m9_multiplayer.md).

## Scope and preservation

Retail The Chosen: Bloodbath, then Team and CTF; IPv4 LAN discovery and direct
address connections; one shared simulation for a playable host and a console
dedicated server. Initial capacity is 16 players including the host. Desktop
Linux/Windows/macOS qualification is separate from compiling mobile/Quest.
Commercial data remain external. No retail wire compatibility, downloads, mods,
demo, Nightmares/Soccer, cooperative campaign, spectator role, multiplayer saves,
master server, relay/NAT traversal or host migration is introduced.

Original references: `sources/blood2/ClientShellDLL/NetStart.cpp` (STARTGAME_HOST,
CLIENT/CLIENTTCP, game defaults), `Blood2Serv/GameServDlg.cpp` (HostGame,
StartWorld, Update, port 27888), `Shared/NetDefs.h` (16 default, 128 array
capacity), `ObjectDLL/BloodServerShell.cpp` (admission, teams, frags, rotation),
`PlayerObj.cpp` (multiplayer init, respawn, slow death, movement, fire) and
`FlagObjects.cpp`/`TeamMgr.cpp`. Paths are relative to the original source tree.
The old MFC dedicated frontend and Win32 network wizard are replaced, not imported.

Keep original game rules and document engine adaptations. In particular, retain
slow death's actual 10% branch rather than its inaccurate 25% comment; death
confirmation owns frags. Keep the 7.5-second map-change delay, original inventory
and pickup reset rules, and existing checked CTF drop/capture/lifetime contracts.
Host and dedicated time-limit defaults remain 15 and 10 minutes respectively;
both default to Bloodbath, 25 frags, ending on frags. M2-GPU-001 and M3 acceptance
are unaffected.

## Ownership and presentation

One authoritative server may bind original game globals per process. A remote
client owns no original server objects. A host owns one server and an independent
client replica; solo migrates to that replica with immediate local delivery and
unchanged current frame deltas, callback order, saves and pause semantics.

ServerSession owns simulation, collision, gameplay model keys, logical sounds,
players and match state. ClientSession/ClientWorld own presentation state, local
movement, view weapons, effects, listener and playback. Replicas must never run
ObjectDLL callbacks. Network identifiers have generations and a world epoch;
HOBJECT/HCLIENT pointers are never encoded. Destruction and map replacement
invalidate references before the next consumer runs.

The dedicated build excludes GPU rendering, ImGui, XR, frontend, setup/download
and audio devices. It still validates resources needed by gameplay, preserves
logical sound duration/completion and the server random sequence. Presentation
failure on one client must not remove an authoritative object. Client FX RNGs
remain separate. Activation considers every connected player's valid observer.

## Transport and admission

ENet upstream is statically pinned to
`5a9c537fd464b3c6d3c55e1d3bd47588faf71b42`; archive SHA-256
`537d42f31b0c0d27f5a9635f6c224f2451c1c1484bfde50e94c884e96181c290`.
ENetHost has one owner. Reliable control/events, reliable initial state, and
unreliable sequenced snapshots use separate channels. Decode and queue budgets
apply before mutation; no serialized native layouts or pointers.

Protocol, retail edition and a full-content SHA-256 fingerprint must match before
admission. The save fingerprint is insufficient: it hashes directory metadata,
not all archive payload bytes. Initial state includes current persistent effects,
looping audio and attachments, without replaying historical one-shots. Private
HUD/inventory/camera events go only to their owner. Disconnect/reconnect creates
a new identity; a matching display name is never authentication.

## Movement and clocks

Preserve the original client movement model, not modern command replay: MoveMgr
computes client movement and uploads position/velocity with the original 0.14 s
countdown and move code. Server bodies advance once between reports from the
received velocity through checked physics. Do not uncomment the original
HandleMovement's disabled SetVelocity/SetAcceleration calls or integrate twice.

The original server's leash teleport after failed collision is replaced with a
checked server correction. Invalid destinations, stale lives/worlds and invalid
values fail. Authorized gameplay teleports use a reliable force-position event
with a new move code. Fire remains server-side; validate identity, weapon, hand,
ammo, state, origin and source-derived cadence; deduplicate FIRE_KEY deliveries
without suppressing legitimate bursts/dual wield/alternate markers.

Multiplayer server: 60 Hz, current collision substeps, at most six catch-up ticks
per outer iteration, reported discarded overload. Snapshots: 20 Hz, remote
interpolation, no interpolation of local predicted bodies or teleports. Transport
timeouts use monotonic wall time; match logic uses simulation time. Dedicated
servers tick with zero clients. Menus/focus loss neutralize local input without
pausing network simulation. Host departure ends the match.

## Verification gates

Codec golden bytes, truncation/overflow/nonfinite/stale-reference tests and
fuzzing; real-process dedicated plus clients and host plus remote clients;
join-in-progress, loss/reordering, timeout, rotation and clean shutdown. Exercise
all 22 retail multiplayer maps with two clients and representative maps at 16.
Qualify RTT 0/50/150 ms, loss 0/1/5%, jitter up to 20 ms. Check every movement
dependency (doors, platforms, water, ladders, impulses, imprisonment), private
state routing, slow death, pickups and flags. Preserve immutable GPU references,
solo/save/load behavior and source RNG/audio clocks. Report synthetic CI, private
data, sanitizer, real GPU and human LAN results separately; builds alone never
qualify interoperability.

## Pickup loading prerequisite

The first implementation restores original multiplayer pickup initialization:
ammo and bomb items use the ammo rule, Health/MegaHealth use health, Ward/NecroWard
use armor, timed powers and charged inventory items use powerups. Defaults are
15/30/45/60 seconds respectively, with normal quantities. Source `LEVEL_NONE`
removes the pickup; half/normal/double/insane multiply by 0.5/1/2/3. Item quantities
retain byte truncation/clamping and the minimum one for a positive multiplier.
WeaponPickup keeps its original multiplayer removal; it is reported as source
filtering, not an unavailable class. The existing blanket ItemPickup multiplayer
precreate refusal was a reconstruction limitation and is removed.

Successful typed acknowledgements still require the live outstanding toucher and
exact offered value. Consumption hides visibility/touch, schedules the source
delay and triggers once. The original scheduled Update restores saved flags and
plays `sounds/powerups/powerup2.wav`; it does not replay the pickup trigger.
Duplicate touches while hidden cannot collect twice. String-spawned drops set
respawn to zero, so their existing bounce state never schedules the source spring.
Solo stays removal-only and retains its initialization callback order. Multiplayer
saves and custom respawn overrides remain refused/unimplemented.

The shared typed offer stores the already-scaled value once at initialization,
equivalent to source subclasses' multiplier applied at each touch while rules are
constant. The new internal pickup-rule structure is not yet a network/UI settings
interface. Cosmetic resource failures remain explicit. Donor functions and engine
adaptations are recorded in the [source ledger](../implementation/sources/m9_foundation_sources.json).

## Local replica, first presentation lot

`ClientSession` now owns a passive `ClientWorld`, independently of the original
server registry. At the owning presentation boundary, `GameSession::publish_client_world`
copies a complete local frame and delivers it immediately. Publication during
an object callback is refused. `ScenePresenter::update` publishes once for all
its consumers/eyes; standalone scene helpers publish before invoking the same
consumers. Simulation still uses exactly its existing order and supplied delta;
neither publication nor replica rendering ticks animation, physics, callbacks,
audio or RNG. A paused frame may be published again without advancing time.

The frame contains live objects in slot order, including inactive/hidden objects:
poses, model/skin paths, scale, alpha/color, flags, animation/sprite playback,
hidden nodes, resolved attachment pose/parent, world-model binding and sky slots.
It also carries ModelAdd/ModelDirAdd, local player/Eye/Orb identities and the
owner-alive gate for view weapons. PolyGrid and particle-stream visibility uses
copied effect-to-owner identities; their effect pools are still shared storage.
`append_client_models`, `append_client_sprites`, world/sky draw construction and
model lighting consume the replica without an original object registry. Existing
GPU structures, shaders, shading arithmetic and XR weapon placement are unchanged.

`ClientObjectId` combines a world epoch and slot/generation. Distinct local session
candidates and every teardown get distinct epochs, including save-load/NEW_LEVEL
promotion. Old-world, duplicate/out-of-order frames, invalid object identities,
nonfinite/backwards clocks and more than 65,536 records are refused atomically.
The complete-frame replacement drops absent and pending-removal objects before
the next consumer; stale attachment/sky IDs resolve to nothing. The renderer also
checks the epoch when invalidating world-owned geometry and lighting caches.
Bare `ObjectId` lookups are only for entries in the currently accepted frame.

This is an **in-process frame**, not a wire schema: local delivery shares immutable
decoded model/sprite resources and copies mutable playback/pose values. No
HOBJECT, HCLIENT, BaseClass, callback or GPU resource is stored in the replica.
Future network resource resolution must use names and independently mounted data,
and validate/bound all decoded fields before admission. The current frame receiver
is not an untrusted network decoder or a claim of full protocol fuzz coverage.

Movement, original client-shell ownership, HUD/inventory/camera reads, FX pools,
dynamic lights/fog and logical sound versus device playback still require
separation. This lot does not enable a remote client, admission or host play.
See the [local replica report](../implementation/reports/m9_client_world.md).


## Local movement separation

The solo local continuation gives `ClientSession` a `ClientMovement` value body;
`CMoveMgr` retains its existing control, gravity, drag, liquid/ladder/conveyor and
jump math. The local bridge copies server stance/environment/support and external
corrections in the same order as the shared-body baseline, at every existing
substep. The frame cap (0.1 s), substeps (at most 1/120 s), pause behavior and solo
clocks are unchanged. No multiplayer 60 Hz clock is applied to solo.

Prediction reuses the collision solver on a scratch body without an ObjectDLL
instance. It can calculate a potential push but never commits it or invokes touch,
damage, pickup, animation or RNG callbacks. Only the authoritative player handler
commits the raw integrated command, using the same collision path and pre-stop
callback velocity. The server does not add velocity*dt again; its generic physics
continues to skip that associated player. The immediate acknowledgement copies
resolved velocity/pose/support, including gameplay impulses. A failed speculative
query is counted and leaves the local pose unchanged; server validation still owns
failure diagnostics and partial/deferred contact semantics.

`LocalMoveReport` contains world+object identity, force revision, sequence and
`PlayerMove` values, never handles. Only the exact outstanding report may be
submitted, once. Sequence does not restart on a rebind; world reset invalidates the
owner. The original force-position byte keeps wrapping, with an additional
non-saved 64-bit server revision preventing acceptance after a full byte cycle.
Exit clears the body and counters. Restore and NEW_LEVEL establish new world/client
identities; no save layout changes. Inspection shows predictions, acknowledgements,
refusals, unresolved predictions and the owning world/object.

The initial movement lot used a live-server collision query bridge; the
[client collision replica](#client-collision-replica) below supersedes that
transitional boundary. Environment/animation/audio requests still use original
server services. The second collision pass is extra CPU work, not a second time
integration. Multiplayer uploads, advancement between them, interpolation,
admission and network gameplay remain separate. See the
[initial movement verification](../implementation/reports/m9_local_movement.md).

## Client collision replica

`ClientSession::collision` owns a passive `ClientCollisionWorld`. Before each
local prediction, the publisher sends a complete, slot-ordered frame with world
and sequence identities, collider IDs, poses/dimensions, velocities/accelerations,
flags, surface flags, attachment/dynamic/pushable/priority traits and the original
fresh Seeing Eye owner exclusion. Pending/dead registry entries disappear from
the next frame; stale generations cannot resolve. Invalid identities, unordered/
duplicate slots, invalid vectors/dimensions, missing shape/primary geometry and
more than 65,536 colliders fail without replacing the accepted frame.

The publisher deep-copies the primary collision index and posed world-model/
container geometry. It shares no server shapes or query counters with the client.
A geometry revision changes before build/clear/transform, including failed edits,
so replacing geometry at the same address invalidates the primary copy. Copies
retain this identity; read-only sweeps do not change it. Posed shape copies are
cached by complete object identity and the source immutable shape's weak identity;
translations copy only the pose, rotations/replacements copy the new posed shape.
Dead entries are evicted and teardown clears caches and the client's scene. This
is local transport of validated engine geometry, not a wire/resource codec.

Client queries use only this scene, and can still run after the server is destroyed.
The client solver retains the current replacement slide/stair/head-skin/overlap
rules and read-only push/rider validation (four legs, 64 riders, no chain push).
Common bounds/transformed-shape sweep arithmetic is extracted unchanged for both
sides. The client kernel deliberately excludes contact collection, stopping-force
callbacks and push commits; its geometric path is checked against the authoritative
solver. `CMoveMgr` remains unchanged and integrates once. Server validation still
commits the integrated command and corrects the local body immediately, preserving
solo contacts, impulses, pose and clocks. The old server prediction entry point is
removed. Inspection exposes scene sequence/size, geometry copies and publication
failures; failed publication never queries an older scene as if it were current.

Authoritative acknowledgements still query server support, as do server gameplay
services. Environment, stance and audio exchanges remain local original services;
this lot does not replicate volume rules, resize decisions, camera rays or weapon
rays. Network delivery, geometry resource admission and delayed multiplayer timing
remain pending. See [verification](../implementation/reports/m9_client_collision.md).

## Client environment replica

The local collision frame now also carries optional `ContainerEnvironment` values
(code, current, gravity, viscosity and Hidden) for recognized containers. Geometry
and values are admitted atomically with the existing world/sequence/object checks.
Non-finite values, invalid current vectors or viscosity outside 0..1 reject the
whole frame. A built-in Container retains its full unsigned 16-bit code, including
zero and otherwise inert unknown values, and zero current/gravity/viscosity.
Absence of environment metadata means an unrecognized container, not code zero.
The publisher shares the authoritative value extractor with `GetContainers`.

`ClientCollisionWorld::environment` derives body and head membership solely from
copied geometry and values. It takes a world-qualified player ID, a client body and
a head point. Results contain generation-qualified IDs, never original handles.
Volumes retain registry slot order, including hidden/inactive entries; every body
overlap counts, only the first four are returned. The first head overlap wins even
if hidden or code zero. Hidden masks its head code without losing the volume ID or
properties. Shapes already contain the world pose; DAT translation is not applied
again. Concave membership uses the existing exact container geometry routines.

CMoveMgr owns `ClientContainerState`; its liquid, ladder and conveyor algorithms
change only their argument types. Hidden-first category consumption, previous-step
swim/ladder speeds, additive currents, surface-swim gates and FreeFall classification
are unchanged. No client viscosity rule is invented. Client queries never execute
`MID_AFFECTPHYSICS`, damage, air, callbacks, RNG or time integration.

The immediate bridge publishes fresh frames before environment queries and before
collision predictions; immutable shape copies are reused. It supplies character and
eye-offset values from the local player. Final client membership is sampled before
`UpdateAirLevel`, which still refreshes authoritative membership and applies air /
suffocation once per capped frame. Landing keeps its own authoritative query.
Server volume physics and damage are unchanged; their intentional conveyor/Hidden
and grounded-character differences from CMoveMgr remain intact. No server membership
list is copied to CMoveMgr. One integration and immediate collision correction remain.

The Containers inspector identifies client membership alongside authoritative air,
health, volume callbacks and membership counters. This is still local transport;
full snapshots are published synchronously, and geometry allocation is cached, not
network-encoded. Stance, profile/eye offsets, camera/fog/weapon point queries and
other presentation exchanges remain local services. This lot neither adds delayed
multiplayer timing nor qualifies network play. See the
[environment verification](../implementation/reports/m9_client_environment.md).

## Local client cameras and HUD

`ClientSession` now owns camera state, death/chase state, stats, inventory tabs and
the local presentation clock. `CBloodClientShell` retains references to that owned
state for its original event handlers; it no longer reads server poses or performs
server rays to select/render a camera. `ClientCameraView` contains an `ObjectId`,
not an original handle. World-qualified frame admission rejects old sequence/world,
invalid identity/pose/clock, duplicate camera slots, invalid camera type and more
than 4,096 descriptors before replacing the accepted camera frame.

`LocalPresentation` publishes player pose/eye height/alive state, checked owned Eye
and Orb targets, and all live camera descriptors in original creation order. The
client preserves first-active scripted selection, Eye then Orb override while alive,
scripted-camera priority over the received death camera, normal/chase/zoom/XR gates,
listener versus player-body position, player/weapon visibility and look suppression.
Published settings remain initialization-time settings, including the original
listener workaround; the source global active listener remains independent.

Death and chase use `ClientCollisionWorld::intersect` on copied geometry. It retains
primary-world ties, slot-order object ties, solid/ray-hit modes, object-only interior
skipping and the original VolumeBrush exclusion (built-in Container is distinct).
Attached collider poses are refreshed at publication and remain eligible for rays.
The five chase clearance rays, two-unit margin, 20-unit fallback, 10,000-unit aim
convergence and original death orbit/one-second transition math are unchanged.
Orientation math is separated from ObjectRecord node lookup so passive camera/HUD
consumers link without the original object dispatcher.

The server still owns permissions and Eye rotation. A local command contains only
world/object identity, frame sequence and finite bounded aim; wrong owners, stale
worlds, reused sequences and changed camera selections fail. The bridge derives
movement/USE/fire permission again from authoritative cameras and checked remote
ownership; a client-rendered view cannot clear that gate. No camera command moves
or integrates the player. Existing CMoveMgr substeps, collision correction, voice
and already-started weapon animation timing remain unchanged.

Stats and inventory events enter `ClientHud` with the associated world/object and a
monotonic local sequence. Server and receiver share the existing validation rules.
Stale/duplicate events cannot replay ITEMF_SHOWITEMS or partially replace accepted
data. Entry/departure/world reset clear ownership and values; diagnostic counters
remain monotonic, as before. Original PCX/FNT layout, maxima, alternate ammo,
weapon/item tabs, CTF icons, crosshair and external/death/XR gates are unchanged.
Both application paths now use `HudSnapshot`, with the client clock and world
identity; no HUD renderer reads Objects time. Menus retain the existing pause clock.

This is an immediate solo bridge, not a network codec or complete client shell
extraction. Legacy handle events for inventory/audio, authoritative animation/stance,
weapon/FX paths, remaining presentation and admission/network gameplay stay pending.
The [camera/HUD report](../implementation/reports/m9_client_view.md) records the
Linux checks and the limits of this lot.

## Local client audio

The [audio lot](../implementation/reports/m9_client_audio.md) separates authoritative
`AudioTimeline` voices from `ClientSession::audio` playback. Server sound services
keep real resource validation, sample-derived durations, explicit stops, completion,
stream failures and original ownership. The server timeline has no device, PCM
buffer, gain/pause setting or music transport. `step_server` never advances or
consults the client mixer.

The local publisher copies complete frames with world/sequence, monotonic sound IDs,
generation-qualified owner/speaker provenance and resource/playback values. No
original handles or asset pointers cross into the frame. The receiver bounds counts
and values, checks local asset metadata, refuses duplicate/retired identities and
replaces accepted membership atomically. Playback has its own buffered clips and
stream readers. Existing voices keep their playback cursor through repeat snapshots;
late voices start at the supplied logical cursor. Client playback failure cannot
poison the server's reader or another client.

Retain original voice order, fractional 48 kHz stepping, interpolation and source
owner/speaker/removal rules. Publish new voices without advancing them, and preserve
the preceding completed listener pose. Keep menu and game mixers independent.
Client output, gain/pause and original music move together on new-game/load/NEW_LEVEL
promotion; world caches do not. Inspect server completion separately from client
cursor/PCM/gains.

The 60 ms interactive output lead is presentation-only. It no longer advances server
completion early. This is an explicit correction of prior device-dependent logical
cursor advancement; zero-lead/offline solo stepping is unchanged. Solo pause gates
both timelines, whereas dedicated advancement ignores client pause/output entirely.
CMoveMgr, physics integration, source random draws and PBR files remain unchanged.

This is an immediate local boundary. The legacy client FX/VoiceMgr entry points,
per-recipient sound delivery, wire codec, delayed synchronization and network
qualification remain pending. Separate caches retain existing bounds and add local
memory/CPU work; a server duration-only reader is not introduced.


## Local client visual effects

The [effects presentation lot](../implementation/reports/m9_client_effects.md)
adds `ClientSession::effects`. `publish_client_world` also publishes complete,
world-qualified effect values before scene consumers run. The standalone client
consumers cover sprites/particles, models/casings, clipped marks, PolyGrids,
tracers, flashlight/dynamic/projectile lights and per-eye world/volume fog. They retain no
original object/effect instance or GPU resource. Particle/surface/mark data is
copied; immutable model/sprite assets are shared locally, and posed fog volumes
are deep-copied into a client-owned cache. Publication does not step any subsystem.

Preserve effect family/creation order, muzzle insertion, whole-system blood/splash
batching, visibility, per-entry diagnostics, mark clipping, source light flags and
existing platform light budgets. The first containing volume still masks later
ones even when hidden; visible fog-off volumes retain world fog. Unaligned negative
warp sprites and fog orientation remain per-eye computations, including XR.

Receivers refuse old worlds/sequences, backwards/invalid clocks, duplicate or invalid
family-qualified IDs, invalid poses and excessive aggregate counts atomically.
Unload clears the accepted frame and geometry cache; absent effects disappear on
replacement. Retained frames remain usable after source teardown. This local frame
is not an untrusted wire decoder; network asset resolution and full field validation
remain ahead of network admission.

The legacy producers, advancing pools and their collision/audio queries still run
through the local game adapter. Their recipes, clocks, callback order and separate
cosmetic RNGs are unchanged. Moving these runtimes to an independent remote client
requires a later event/service boundary; this delivery separates every scene consumer
without claiming that additional migration. Server logical audio, solo pause,
CMoveMgr single integration and saved payloads are unchanged.

## Client effect ownership and collision queries

The [runtime continuation](../implementation/reports/m9_effect_queries.md) owns
original cosmetic pools through ClientSession in a separately linked library.
The local server adapter retains publication/inspection aliases. Reset preserves
source pool ordering, seeds and identity lifetime. This does not yet remove all
legacy update services or provide remote event delivery.

Original explosion fragments now query the independent collision replica at their
existing update point. Retain SolidOrRayHit selection, source reflection/rest/fade
arithmetic, terminal motion and trails. Publishing geometry must not advance any
body, clock or RNG. Failed publication supplies unavailable queries, never a server
fallback. Primary-only effect queries keep the original ray bound and texture-flag
quirk; point-container queries retain first-overlap hidden/zero-code masking.

## Owner-bound client effects

The [owner-bound continuation](../implementation/reports/m9_effect_owners.md)
replaces retained server handles in object particles, streams, PolyGrids, WarpGates
and CLightFX with world-qualified object identities. Creation receives a copied
owner descriptor; the local publisher preserves attachment refresh. Updates use
ClientWorld only, after stable object publication at the original effect phase.
No callback, movement integration, RNG draw or sound advance belongs in publication.
An old world or object generation must not bind to a replacement owner.

Keep original pool order/budgets, PolyGrid position-only polling and hidden update
gates, missing-owner removal, warp child/ramp rules and light phase/visibility.
Restore rollback and duplicate-publication checks resolve the same identities.
Moving cosmetic collision/audio and flashlight services remain a separate step;
this boundary is not a network codec or late-join implementation.

## Client cosmetic sound ownership

The [effect audio continuation](../implementation/reports/m9_effect_audio.md) routes
fragment, blood-drop and casing impacts to ClientAudio. They are buffered spatial
one-shots with no server owner or speaker; their completion and removal belong to
the client. They never consume logical voice capacity, sound IDs or server clocks.
The dedicated explicitly has no local cosmetic receiver, including during immediate
effect creation. Absence or resource failure remains an inspectable cosmetic error.

Logical IDs use the lower 63 bits; local cosmetics use the upper namespace, never
reused across worlds. Replicated descriptors cannot claim cosmetic IDs. Accepted
frames preserve cosmetic cursors while replacing logical membership. Keep the
128-voice mixer bound; logical admission displaces the oldest local one-shots when
necessary. Expose started, refused and displaced counts. Invalid frames cannot
displace voices. Original request selection, attenuation, interpolation, sample
stepping and separate cosmetic RNG remain unchanged. Mixed ordering is logical
voices followed by cosmetic voices; arbitrary interleaved historical sums are not
claimed bit-identical. Movement integration, server completion and save payloads
are unchanged. The legacy effect publisher still forwards locally; remote events
and the remaining moving-effect services are separate work.

## Moving client effects and child outputs

The [moving effect services continuation](../implementation/reports/m9_effect_services.md)
removes ServerDE and original handles from flesh/destruction fragment, casing and
blood-emitter creation/update interfaces. Keep source calculations, RNG sequences,
primary-only geometry queries, zero texture flags, immediate first spurt updates
and pool order. Typed outputs publish directly into client pools and ClientAudio.
After authoritative completion, copy collision once before the moving FX phase;
publication and updates must not integrate a body or dispatch server contacts.

The synchronous local creation bridge still supplies live read-only geometry while
inside callbacks, preserving existing ordering/resource failures without copying a
busy registry. No effect retains those services. Client updates use the replica
exclusively; unavailable geometry remains explicit, with existing cosmetic failure
policy. Flashlight extraction, creation event delivery and remote qualification
remain subsequent work. No new network acceptance is implied.
