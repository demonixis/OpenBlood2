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

Solo publishes this scene several times per frame (camera, each movement substep,
air update), so its cost is bounded: the publisher classifies each object identity
(VolumeBrush, Seeing Eye) once instead of per publication, and an accepted frame
swaps with the replaced scene, whose storage the publisher reuses. A frame that is
bit-identical to the accepted scene apart from its newer sequence is admitted
without revalidating its colliders (NaN never compares equal). The client object
frame swaps the same way; the local object publisher keeps only string/vector
capacity and drops model/sprite references right after delivery. The second
camera selection of a client update reuses the camera frame published just before.

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
CMoveMgr, physics integration and source random draws remain unchanged.

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
warp sprites and fog orientation remain per-eye computations, including XR. The frame
also carries the 0.5.2 world FarZ and the SkyFog interval as truncated values; the
consumer enables sky fog only while a fog colour applies, and receivers refuse a
non-finite or negative far plane and an empty sky-fog interval.

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

## Client flashlight

The [flashlight continuation](../implementation/reports/m9_flashlight.md) moves
CFlashlightFX into the independent client library. Source and owner use world/object
generations; pose/visibility come from ClientWorld, with rays in copied collision
and the player body excluded. Preserve body origin, source light math, failed shade
fallback, miss/hide/removal and invalid/unavailable-query refusal. Current lighting
must not query the authoritative registry or advance state. Server inventory,
battery and source-child rules remain unchanged. The local effect phase shares
one stable collision publication with later moving cosmetics. Stale worlds or
object generations cannot relight an effect. Trusted local frame validation is
not a complete wire decoder. Network creation/events remain separate.

## Multiple server connections and Bloodbath markers

The internal connection registry now accepts up to 16 live tokens for Bloodbath,
Team and CTF. Other modes retain the one-client limit. Tokens remain opaque,
never reused and bounded to 65,536 per adapter lifetime; this is not a wire ID.
A live original player can belong to only one connection, including the restore
window before its client field is assigned. Existing private queues retain their
independent 256-event bounds, and exit detaches before disconnect.

Bloodbath `GoToStartPoint` now makes the original single random ordinal draw over
all named `Blood2StartPoint` objects, including markers whose `Multiplayer` field
is false. It preserves trigger, rotation, checked teleport/ground placement,
physics and forced-position messages. When the selected marker is occupied and
the bounded separation finds no room, the next live markers in list order are
tried (adaptation: no further random draw, no trigger for a fallback marker, its
rotation resent; `entry_start_retries` counts them). There is no origin fallback;
every marker occupied or invalid still fails with owned-list cleanup.
The original solo named/fallback selection and explicit inspection override stay
unchanged. Team filtering, original multiplayer initialization and death/respawn
were subsequent steps at that delivery; wire admission is now covered below.
See [connection verification](../implementation/reports/m9_player_connections.md).

## Fresh Bloodbath player profiles

The shell now dispatches Bloodbath profile initialization to the original
multiplayer entry branch, sharing the established profile/weapon/reset body with
solo. Four characters and their seven original skin paths are admitted; selected
body and physics descriptors agree. Invalid character/skin/name values, duplicate
init and non-player/non-retail loadout types fail before allocation or a random
draw. Valid attributes retain the existing source clamp and 12-point limit.
The original weapon acquisition, dual-hand request and slot-1 selection order stay.
The absent-B2C fallback in the reference client contains weapons 1 through 9;
the current solo PlayerInit default deliberately remains a single Beretta.

`SetMultiplayerAmmo` supplies None (empty), Half (25 bullets/4 shells), Normal
(50 bullets/8 shells/5 BMG/20 batteries/5 spray/10 flares/10 fuel), Double
(twice those quantities plus 5 Howitzer) or Insane (original infinite-ammo setter
and full inventory, including bombs). It reads the same fixed match ammo level
as the already ported pickups. Solo still clears ammo and grants 50 bullets.

Before shared valid profile initialization, the source draws one 0..65535 index
and sends VOICE_START_BB only to the joining client. The typed BloodbathStart
message carries this index; the client resolves index modulo eight using the
original Voice248..255 files, portrait 4 and external cres.dll strings 4001..4008,
with the original English fallback texts and explicit resource diagnostics.
No extra client random draw or server playback is introduced by table selection.
Delivery uses the existing CommLink path; its local server/audio bridge, like
VoiceMgr, still needs extraction before remote gameplay.

This is fresh Bloodbath entry only. Team assignment/forced skins, configurable
healing, dead respawn/slow death and the delayed spawn voice are subsequent lots.
Team/CTF profile requests are explicitly refused meanwhile. Wire admission is
covered by the subsequent continuation below. See [profile verification](../implementation/reports/m9_player_profiles.md).

## ENet Bloodbath admission

The [admission continuation](../implementation/reports/m9_admission.md) opens
Bloodbath entry through a bounded, portable protocol. Initial version 1 used explicit
little-endian fields and five Control messages, each at most 256 bytes. Decode
into a temporary value and validate the complete packet before committing it.
Names, profile values, mounted map paths, player/object identities and world
generations have explicit limits; no pointer or content payload is transmitted.

Hello validates protocol, retail edition and complete mounted-content SHA-256.
WorldOffer names the current approved map and world generation. Ready carries
that generation and the profile after the client loads its own map. Only then
may ServerSession create/initialize the original player and return Joined.
Wrong ordering, channels, duplicate init, malformed values or stale generations
disconnect that peer. Team/CTF admission remains refused until their rules port.

One network owner drives at most 64 incoming events per pump, over the existing
16-peer and transport queue bounds. Monotonic wall deadlines are 5 seconds for
Hello, 30 seconds for loading and 15 seconds for joined-client liveness; joined
peers exchange one-second heartbeats. Expiry precedes accepting a late packet.
Backwards pump time fails without mutating the connection registry. Departure,
timeout and shutdown detach original players before freeing their connections.
A reconnect always gets fresh player and generation-qualified object identities.
The console keeps its existing 60 Hz / six-tick catch-up policy.

An occupied multiplayer start now uses the existing bounded initial actor
separation. This is an explicit replacement-physics adaptation: original code
teleports to its chosen marker and ignores MoveObjectToGround's return. Preserve
that one marker draw and all solo behavior. Try only four face-adjacent horizontal
positions around the overlapping player, nearest first; validate the full
endpoint and swept route against primary/world-model geometry and other bodies.
Only the initial overlapping player is omitted from the escape sweep. Do not
move any existing player. If all four exits are blocked, refuse entry with an
explicit error and remove its newly created objects. There is no extra random
draw, marker retry, origin fallback or runtime collision exception.

This step admits original players and validates process isolation; it does not
yet deliver their gameplay events, world snapshots or movement. Private event
queues keep their existing bounds. Interactive host/client gameplay, slow death
and respawn, match rules, LAN discovery and platform qualification remain open.

## Initial world snapshot over ENet

The [initial snapshot continuation](../implementation/reports/m9_world_baseline.md)
introduced protocol version 2, superseded by version 3 below. After
Joined, the server sends one complete observable object snapshot on reliable
Baseline. Control and Baseline may arrive in either order; a client waits for
both and verifies its addressed player before activating the received world.

Capture shares the immediate local publisher's read-only object traversal,
without advancing callbacks, animation, audio or RNG. The wire record contains
only explicit little-endian values: identities, poses, flags/colors, named assets,
animation/sprite tracks, hidden nodes, attachments, world-model bindings, sky and
owner identities for PolyGrids/streams. There is no original handle, decoded
asset, GPU resource or serialized C++ object layout. This is the object boundary;
collision/environment values, effects recipes, logical voices and HUD still need
their own wire delivery.

A full snapshot is at most 1 MiB. Reliable chunks are at most 32,768 bytes,
including their 30-byte world/sequence/length/offset header. The receiver owns
one bounded assembly, requires exact contiguous offsets and matching metadata,
and refuses old worlds/sequences, duplicates, truncations and excess lengths.
Partial or invalid input cannot replace the accepted world. Decode validates
finite/ranged fields, object ordering/generations, boolean encodings, resource
names, node masks and aggregate counts before committing. No model keys or
other gameplay callbacks execute in a replica.

A separately linked client module resolves models and sprites from its own
mounted resources. It verifies model animation/node bounds, sprite frame bounds
and world-model names/indices against its own DAT. Only a fully resolved frame
replaces ClientWorld. Decoded asset ownership survives server teardown; old
references keep their world/object generation qualification. Cross-frame retired
identity tracking and unreliable incremental updates belong to the next step.

This is one initial frame at each admission, including after the world has
advanced. Existing connected clients do not yet receive subsequent joins, poses
or destructions. Twenty-Hz publication/interpolation, private events, remote
movement/combat and playable host/client operation remain pending.


## Continuous reliable object membership

The [membership continuation](../implementation/reports/m9_world_membership.md)
introduced protocol version 3, superseded by version 4 below. After receiving Joined and resolving the initial frame,
the client sends `BaselineReady(world, sequence)`. Only an exact match enables
continuous publication. The baseline phase has its own 30-second deadline;
a wrong world/sequence or duplicate acknowledgement closes that admission and
removes its player. Independent channel ordering remains supported.

Every three successful server ticks, each admitted peer compares the current
world against its last reliable membership. A patch carries its exact predecessor
sequence, full frame headers, removed identities and new/changed metadata records.
Metadata includes resource bindings, visibility/user flags, hidden nodes,
attachments and world-model bindings. Pose/scale/color/alpha and animation/sprite
tracks accompany creations/metadata changes but do not by themselves trigger a
reliable patch; continuous poses belong to the separate State channel. Missed
publication opportunities coalesce to the latest world, without a catch-up burst.

Patches use the same ordered reliable chunk assembly and 1 MiB aggregate limit.
The receiver validates removals against live generations, requires an explicit
removal before replacing a slot, and remembers the highest admitted generation
per slot for the entire world. Removed generations cannot be reintroduced, even
after several updates. A failed asset binding does not retire or admit identities.
The resulting full world must also fit the snapshot budget. Missing recipient
players, stale bases/worlds, unknown removals and malformed records fail atomically.
No replica executes gameplay callbacks or takes ownership of server objects.

Existing clients now observe arrivals, departures and metadata changes. Unreliable
20 Hz poses/interpolation, collision/environment, private gameplay/audio/effect
messages and interactive movement/combat still require their wire paths.


## Twenty-Hz absolute poses and remote interpolation

The [state continuation](../implementation/reports/m9_world_state.md) introduced
protocol version 4, superseded by version 5 below. Every three successful 60 Hz ticks, after reliable membership changes,
the server sends absolute poses on the unreliable sequenced State channel. Each
packet contains a world, membership revision, server sequence/time and at most ten
object states. Its maximum encoded size is 1,120 bytes, below the 1,200-byte
transport limit. There is no fragmented unreliable world-frame assembly: each
packet can update its objects independently. All states are repeated at the next
publication; missed opportunities coalesce without catch-up bursts. If the next
publication cannot fit the existing 2 MiB peer queue, it is dropped and counted.

Only the currently admitted membership revision can accept a pose. Packets for
future membership are dropped until its reliable update arrives; later publication
repairs the omission. Per-object sequences reject duplicates and older packets;
world/object generations and pose revisions must match. A pose cannot create or
remove an object or change its assets/flags. Invalid playback indices fail before
any object mutation. ClientWorld publication sequence is local and independent
of a server sequence that may span several packets.

Each live object keeps at most eight pose samples. The separate sampling function
uses a caller-provided delayed server time (100 ms in the process probe), linear
position/scale/color interpolation and normalized shortest-sign quaternion
interpolation. Compatible monotonically advancing animation clocks interpolate;
clip/revision changes and loop boundaries select stored playback. No model keys
or gameplay callbacks run. Outside the available interval, samples clamp without
extrapolation. The predicted local player and its attached children retain their
latest values; CMoveMgr remains the only local movement integration.

A changed position through the existing TeleportObject service receives a new
transient server-wide pose revision when it is a discontinuity: any change for a
client-moved player, or a jump beyond 64 units (`pose_continuity_limit`) for
other objects. Original per-update followers (held models, carried flags,
attached leeches) stay continuous, so their State poses keep interpolating
instead of waiting for a reliable patch every update. Capture passes it to the object and its
attached child (the checked attachment service disallows chains). That revision
changes reliable membership and carries the destination pose, clearing that
object's interpolation history. Repeated same-position calls do not create a
revision. This adds no new teleport/collision path and changes no saved gameplay
payload or RNG draw. Replicated gameplay teleports snap; private movement-force
messages and their remote handling still need their own wire path.


## Collision and environment on the wire

The [collision transport continuation](../implementation/reports/m9_collision_transport.md)
introduced protocol version 5, superseded by version 6 below. Object baselines/reliable patches carry pointer-free
collision metadata: shape binding/basis, original VolumeBrush classification,
optional container code/current/gravity/viscosity/Hidden, priority, dynamic/pushable
flags and the Seeing Eye owner exclusion. Collider position/type/flags/attachment
reuse the object record. Dimensions, velocity, acceleration and the posed shape's
origin accompany State packets, now **seven records / 1,132 bytes maximum**.
Container codes preserve the full unsigned 16-bit range, including inert unknowns.

The read-only extractor is shared with local collision publication. Solo object
presentation does not opt into these additional wire values; its collision copy
and CMoveMgr substep ordering remain unchanged. Environment capture performs no
volume callback, damage, air update or random draw.

The client builds primary collision and bound shapes from its own admitted DAT.
It owns the decoded model data and caches independent authored/posed shapes by
world, object generation, model index/basis and pose. Containers use strict closed
shell validation; world models retain the existing triangulated boundary policy.
A transformed world model reproduces the server's basis-relative rotation at the
actual posed shape origin. Straight translation retains the shape and applies
position minus shape origin during queries. This avoids a second DAT translation
and preserves world-model rotation/translation arithmetic.

Object and collision frames commit together only after all resource/geometry
checks pass. Bad DAT bindings, impossible collision classifications and invalid
shape origins cannot advance either frame or the object's accepted State sequence.
Collision uses the raw replicated poses, independently of presentation interpolation.
Membership and per-object State ordering remain generation-qualified; no replica
runs authoritative contacts, pushes, damage or another movement integration.
Retained frames keep primary and shaped geometry after source/world teardown.

Private player/movement messages and the actual network CMoveMgr command bridge
remain next. The all-map geometry checks and process probes do not qualify
interactive movement/combat, degraded-network collision behavior or human LAN play.

## Independent movement services and replica prediction

The [movement service continuation](../implementation/reports/m9_movement_services.md)
builds original CMoveMgr independently of server game classes. Its service calls
are values and client queries; the immediate LocalMovement implementation preserves
solo control/stance, per-substep synchronization, prediction/submission/correction,
voice and air-update ordering. The original integration arithmetic is unchanged.

ReplicaMovement owns a client body, checks admitted world/object identity and uses
only the collision/environment replica. Substeps retain prediction instead of
copying an old authoritative pose. Reports carry owner, full force revision,
sequence, move code, controls, the origin and every CMoveMgr substep since the
previous report (offset, pre-collision velocity/acceleration, duration, ground and
surface), and the last predicted move with its pre-collision velocity. Preserve
first-frame publication; later reports follow every 1/30 s or 12 substeps (one
capped 0.1 s frame), without a burst (adaptation of the original 0.14 s countdown,
whose single destination cannot carry stairs or landings). Reporting uses real
frame duration separately from capped physics time. Prediction passes through a
model it starts inside (players spawned or pushed into each other) and still
stops at every other obstacle.

This is an independently linked prediction/report producer. Wire setup/stance,
report transport, server velocity advancement, correction/acknowledgement handling
and remote voice playback remain pending. The future receiver must qualify force
positions by full revision before calling the internal typed force service; an
eight-bit move code alone never proves freshness across lives or teleports.

## Movement reports and authoritative advancement

The [movement transport continuation](../implementation/reports/m9_movement_transport.md)
introduced 98-byte reliable setup, unreliable client reports and 106-byte
unreliable ACKs. Protocol 9 reports are 117 bytes plus 42 per substep, at most 24
substeps (1,125 bytes, under the 1,200-byte State limit); each substep lasts at
most 1/120 s. Validate the admitted peer/player, world, object generation, full
force generation, byte code and report sequence. Queue up to eight reports per
player in order, dropping the oldest on overflow; ignore stale/duplicate reports
without refreshing liveness. A peer cannot command another player.

Network force generation tracks both original player forces and engine pose
discontinuities. Either invalidates pending reports and ACKs, even when the byte
code wraps or the engine teleport has no original player message. Keep this
network identity separate from solo force codes and saved game state.

Keep remote movement in the server player stage before container damage/cleanup.
Each queued report replays its substeps in order from the server's own body:
destination = current position + offset, submitted with the substep's
pre-collision velocity through the solo `PlayerMove` path. Only collision and
contacts run, so stairs, stopping contacts and falling damage follow the solo
rules; there is no other gravity, acceleration or command integration. A report
after a lost one first makes one collision-checked sweep to its origin. Remaining
substeps are skipped after a contact forces a new pose or kills the player. The
server never extrapolates: after 30 ticks without a report the body velocity is
held at zero and stale controls are neutralized. Retain original
animation/clearance controls each tick, server air, door pushes and held models.

Adaptation (leash): a substep moving faster than 8,000 units/s, or exceeding the
player's movement time, is refused; the catch-up sweep is charged at that speed.
Movement time refills 5 % faster than the server clock and is capped at 0.5 s
(it starts full, absorbing jitter). The ACK then corrects the client. A movement
error isolates its player: it is removed with a warning (`movement_failures`) and
the endpoint disconnects only that peer; the match keeps running.

Capture ACK motion after the last applied report, before later ticks; repeat it at
20 Hz. Publish setup/stance/force changes reliably. A force during report contacts
invalidates that old report's ACK. Client setup/correction reception, complete
private event delivery and interactive remote gameplay remain subsequent work.


## Independent setup and client reconciliation

The [client movement continuation](../implementation/reports/m9_movement_client.md)
feeds reliable setup and receipt-time ACKs to server-independent CMoveMgr. Keep
its original arithmetic and the immediate solo path unchanged. Force changes
snap once through the existing force service, preserve accumulated logical
counters and invalidate previous report history using the full force generation.

Keep ordinary prediction independent of replicated server poses. Resize stance
with checked foot anchoring; a delayed blocked growth retains the prior body
until an authoritative ACK supplies the matching pose/dimensions. An ACK restores
its report checkpoint and replays only later client frames against owned current
collision. Update later checkpoints, without replaying report clocks, audio,
server callbacks, air or damage. Logical counts advance once; expose replay work
separately. Failed replay/prediction must not leave partially advanced state.

Replay re-records substeps only for frames not yet reported. A failed prediction
frame restores its state and holds the body (the replica can briefly disagree
with the server); more than 240 consecutive failures refuse prediction.

Bound history to 512 frames/64 reports and one future-setup ACK. Refuse unsent
current sequences/wrong codes; ignore retired/duplicate/stale owner/force ACKs.
Changed physics/stance retires old history; unchanged newer setup retains it.
Defer corrections embedded in current collision without consuming the ACK,
including corrections with no later frame to replay. Keep unavailable-owner
setup deferred and invalid values refused. The separate-process loopback test
is actual movement transport, not interactive gameplay or degraded combat.


## Reliable private player events

The [private event continuation](../implementation/reports/m9_private_events.md)
uses protocol 7, 20 pointer-free tags and a 2,048-byte packet bound on reliable
Control. Capture pointer-bearing Eye/Orb/creature/CTF identities synchronously at
server emission, before deferred destruction retires their checked handles.
Never serialize an address or resolve a retired handle by dereferencing it.

Preserve per-player order, owner/world/object generation, increasing sequence and
source tick, with 256-event queues on each side and the existing transport byte
budget. Refuse malformed values, sequence gaps and overflow explicitly. Duplicate
or other-owner events cannot update the HUD or repeat ITEMF_SHOWITEMS. Consumers
must drain the bounded client inbox; received audio/camera events are not yet a
claim of completed remote playback/selection.

Admission subsumes PlayerInitRequest; reliable movement setup subsumes legacy
physics/force messages. Do not repeat those operations from this event channel.
Preserve original one-recipient Bloodbath startup and all-recipient null-client
conversation/objective/boss-health broadcasts; attempt remaining recipients after
one failure. Keep the solo event path, RNG, logical audio and mode filters intact.

## Reliable camera state and server permissions

The [camera transport continuation](../implementation/reports/m9_camera_transport.md)
adds private tag 21 in protocol 8. Publish the first active original scripted
camera plus checked Eye/Orb IDs/settings, eye height, alive state and full movement
force generation. Reuse replica poses and local predicted body; never transmit a
GPU view or trust a peer's camera nomination. Validate live original type/owner
before deriving camera movement/look gates. Preserve Eye aim through the existing
original PlayerRotation path and suppress scripted/dead turning.

Append changed CameraState after queued original private events, with force
identity participating in equality. Preserve Eye reset and matching-Orb-clear
semantics, descriptor creation order, original camera priority/listener, hidden
player/weapons, death/chase collision and XR chase refusal. Defer missing objects
or mismatched force identities without committing a partial view.

Only PlayerDead starts the death orbit; slow-death state alone does not. Repeated
death retains the original Begin guard. Wait for a delayed presentation clock to
reach a future death tick, refuse backwards/invalid time, and clear old death
state on an authoritative alive transition/force. Production clock mapping and
interactive presentation remain separate; keep the immediate solo path unchanged.


## Client scene inputs and GPU ownership

The [independent scene continuation](../implementation/reports/m9_scene_inputs.md)
makes ScenePresenter/model/sprite/PolyGrid/light consumers accept borrowed
ClientScene data: locally decoded DAT/resources, ClientWorld/ClientEffects,
client camera/settings and client-owned view-weapon visuals. Do not reintroduce
GameSession or a server adapter into these consumers. Preserve original weapon
placement/muzzle arithmetic, handed/XR transforms and draw order; network firing
remains a separate extraction from the inherited CViewWeapon runtime.

Keep solo publication in the compatibility bridge, once for ScenePresenter and
once when an individual helper is used directly. Preserve original clock-derived
values sampled by that bridge and immutable GPU references. Reject mixed effect
worlds and recording against a different prepared world/revision. GPU resources
remain world-owned by ScenePresenter; ClientScene is not a serializable protocol.
The server-only build excludes the GPU bridge.


## Reusable network client and presentation clock

The [client endpoint continuation](../implementation/reports/m9_network_client.md)
owns admission, local content, replica/collision, CMoveMgr prediction, private HUD
and camera without a server session. Preserve one admission per endpoint lifetime,
cross-channel baseline ordering, phase/owner/channel checks, bounded pumps/inbox
and explicit deadlines/rejections. Do not process a duplicate private event again.

Keep the report cadence above and one client integration, 60 Hz client steps,
six-step catchup and discarded-tick accounting. Newer server stamps alone reanchor
the monotonic mapped clock, with bounded lead during silence. Sample remote poses
100 ms behind it, without changing authoritative reception time. Presentation
callers drain typed events; pending audio/FX/weapon consumers and interactive
host/join remain subsequent work, not implicitly supplied by this endpoint.


## Graphical remote client and host

The [desktop frontend](../implementation/reports/m9_network_frontend.md) creates
no server in the remote path. Hosting owns one ServerSession and admits its local
NetworkClient through the same ENet path. Keep server ticks and network pumps
independent of focus/menus; only local commands are neutralized. Quitting the host
ends the session. Preserve existing controls/preferences and passive ClientScene
presentation; solo keeps its existing frontend and clocks.

Per-frame mouse offsets survive zero-tick frames and are consumed once across
the fixed ticks that run; time discarded under the six-tick overload limit never
discards mouse movement. Clear
pending offsets when commands are neutralized or mouse-move mode ends. Do not
modify CMoveMgr or add another integration to accommodate render scheduling.

Remote audio/FX consumers, original Host/Join screens, LAN discovery and full
gameplay remain separate deliveries. The private loadout continuation below adds
view weapons; the empty effects frame remains an explicit limitation.


## Independent view-weapon services

The [weapon runtime extraction](../implementation/reports/m9_weapon_services.md)
keeps CViewWeapon animation/cadence/RNG in blood2_client_weapons with no server
object dependency. WeaponServices exposes a checked hand/owner, clock, ammo and
typed fire/local-sound delivery. Preserve model-key ordering, committed attacks,
shotgun barrel state, bombs, alternate ammo and deferred owner/selection rechecks.
The local bridge retains synchronous OnMessage/PlayLocalSound behavior and solo
clocks. A remote implementation must separately submit and validate authoritative
fire; cosmetic acceptance alone cannot grant damage or ammo authority.


## Private network weapon loadout

The [loadout continuation](../implementation/reports/m9_weapon_loadout.md) adds
protocol-v9 tag 22: a private snapshot qualified by world, player/object and full
force revision. It carries character/slot, two weapon types, each hand's primary
and alternate ammo, liveness, input/fire permissions and binocular state. The
server reads original inventory/player state after refreshing movement/camera
gates; clients cannot nominate their ammo or permissions. Publish only changes.

Load both changed views and held resources before replacing either hand. Match
SyncWeapons: only type or character changes recreate a view; ammo changes retain
its animation. Keep the original CViewWeapon runtime, one update per existing
60 Hz client tick, and a monotonic clock anchored at the first loadout. Rendering
and correction replay never advance it. Reject invalid data/resources atomically;
ignore old owners, sequences and force revisions. A current-force loadout is part
of client readiness. Solo services, clocks and CMoveMgr remain unchanged.

This step supplies draw/rest presentation only. No firing input or weapon command
is submitted yet; animation sound requests are counted with explicit unavailable
output, never reported as played. Authoritative fire validation/deduplication,
selection, predicted cosmetic shots and remote audio/FX follow separately.


## Reliable player commands

The [command continuation](../implementation/reports/m9_player_commands.md) adds
protocol-v10 B2PC requests/ACKs on Control. Keep the explicit ten-action allowlist,
57-byte requests/48-byte replies, full world/player/object/force identity and
strictly ordered command sequence. Duplicates never dispatch twice. At most 32
queued requests plus undrained ACKs exist per server player; the client likewise
keeps at most 32 outstanding requests. Reject malformed channels, types, stream
gaps and excess capacity. ACKs must match the client's oldest pending identity.

Apply commands after authoritative movement, within the existing server tick.
Refresh gates/force before each command, so a teleport between receipt and execution
produces one negative ACK. Reuse original selection/cycling, USE, item/cursor/use,
alternate zoom and detonation handlers. Empty/current in-range weapon selection
still cancels zoom. Preserve item run/camera/Eye exceptions; ordinary gameplay
refusal is a negative ACK, not a disconnected session. No cheat/raw-object command
is exposed. Dead-player USE/respawn remains to port.

The frontend samples existing bindings and source command order. Held Press input
produces one rising command; focus/menu/loading suppression prevents replay on
resume. USE carries finite bounded original angles (authored yaw may exceed pi),
never a client-selected origin or target. The server casts from its checked body.
ZoomView now drives original FOV rate/limits, external-camera exceptions, crosshair
overrides and quarter-sensitivity mouse look. Weapon firing, drop/grab/taunt,
creature escape, remote item light/audio effects and complete match rules remain
separate work. Solo entrypoints and CMoveMgr are unchanged.


## Animation-validated network fire

The [network fire continuation](../implementation/reports/m9_network_fire.md)
adds protocol-v11 B2WF frames and actual-shot ACKs. One reliable input frame per
normal 60 Hz client weapon tick references a server-issued loadout sequence, full
world/player/object/force identity and bounded aim. No client origin, target, ammo,
damage, seed or reload interval is trusted. Rendering and movement replay emit none.

Run original CViewWeapon on both ends. The server's AnimationOnly mode loads ABC
clips/keys without skins/flashes; default solo Create/Update remain unchanged.
Both timelines use the same issued state and incremented double clock, preserving
original reload/barrel/bomb/alternate/RNG behavior. Client prediction never changes
ammo/health. Actual fire separately rechecks current ownership, force/pose, relevant
hand revision, character/type, life, movement/camera/Eye/binocular gates and ammo,
and invokes original CPlayerObj/CWeapon from the checked body/eye. SOUND_KEY stays
cosmetic; actual fire audio still owns server logical duration/completion.

Publish nonzero per-hand view revisions in the 92-byte loadout. Increment only for
a published type change or character change of a present hand. Ammo/force updates
retain animation, adding the left hand preserves the right, and a published
A → B → A resets A even when no client tick ran between these events. Retain at most
128 server-issued loadouts; input references must exist and be nondecreasing.

Keep commands and weapon frames in one ordered Control queue. Bounds: 32 commands
plus undrained command ACKs, 64 weapon frames, six processed weapon frames per
server tick, one frame of credit per tick capped at 60. This bounded one-second
late-input allowance may produce a catch-up burst; it is not hit rollback or a
fully competitive authoritative movement/combat model. Duplicates never execute
twice. Old force/view frames may advance animation proof but never grant gameplay.

Latest unreliable State ACKs carry processed sequence and cumulative actual shots
per hand/refusals. They never grant ammo or movement. Client queues are bounded;
invalid channels/types/sequences/capacity reject the offending peer. Neutral
menu/focus ticks continue. CMoveMgr receives the firing flag and still integrates
once. Interactive held fire remains neutral until multiplayer death/slow-death/
respawn is connected. Remote shot audio/FX identity/dedup remains separate.


## Bloodbath life transitions and reliable death cancellation

The [life-state continuation](../implementation/reports/m9_network_death.md) adds
protocol-v12 WonkyVision, FinishHimVoice and FragEvent. Preserve original 10%
slow-death chance below −5 death HP, self exclusion, .5-second finishing grace,
5-second explicit USE rescue, 15-second expiry and separate confirmation update.
Confirmed death, not lethal damage alone, starts the death camera/weapon cancellation.
Finishing damage credits the first lethal attacker. Removed killers get no frag
or draw; self is −1, another connected killer +1. Draw the broadcast index once,
independently of recipient count. Team/CTF scoring is not enabled by this subset.

Use the existing independent model replicas for humiliation and checked dimensions;
defer resize from a contact transaction. Source fourth-animation gibs occur on
respawn. Keep same player/inventory, source ammo reset, marker choice and world-owned
shared creation timestamp for delayed spawn voice. A failed respawn must retain
a dead player at the preceding position with explicit failure, never grant a live
unchecked placement. Multiplayer life/frag fields are outside solo saves.

Publish actual dead weapon state at PlayerDead even if USE runs before the next
normal publication. Both endpoints observe every intervening loadout and run
original CancelFiringState without resetting reload/barrel/RNG. Respawn's checked
force invalidates old-life commands/shots. No CMoveMgr integration is added.
Network camera no-hide survives repeated death notifications and clears on respawn.
Wonky FOV/roll/model warble, remote announcements and scoreboard remain consumers
to connect. Remote damage impulses and real-process lethal PvP are the next gate
before frontend held fire; this lot alone is not playable network acceptance.


Protocol v12 also moves weapon-frame ACKs to reliable Control at 20 Hz, only on
processed-sequence changes. These ACKs release a bounded input window and cannot
rely on best-effort State delivery. Keep State snapshots unreliable/sequenced and
all existing queue/input/catch-up limits; the [life-state report](../implementation/reports/m9_network_death.md#reliable-weapon-acknowledgements)
records the reproduced GPU failure, diagnosis and discarded-State regression.


## Reliable original player impulses

The [remote impulse continuation](../implementation/reports/m9_network_impulses.md)
uses protocol v13. Reliable Control MovementImpulse adds the original successful
CDestructable/door delta to the independent client body once. Retain world/player/
object/force identity, contiguous per-force sequence and finite bounded vectors.
Do not run CMoveMgr, change position or reset the 0.14-second clock on receipt.
Retire pre-impulse prediction checkpoints; never let an old ACK erase/replay a delta.
Defer ACKs ahead of reliable impulse delivery and impulses ahead of collision/setup.

Reports and ACKs carry their highest applied impulse. Retain at most 128 pending
deltas per peer. Add only unreceived deltas to a report's overwritten velocity,
before collision; an acknowledged delta must not be added again. A report tick
never integrates velocity*time. Existing intervening substeps remain velocity-only.
Force/respawn retires old deltas and histories. Backlog/velocity failure retires
only the affected peer, leaving dedicated simulation and other players running.

Death neutralizes commands, not passive CMoveMgr motion or checked report receipt.
Preserve gravity/drag in the client and the half-second stale-report server hold.
Enable accumulation only for remote ServerSession players: the solo synchronous
bridge already sees the changed velocity and must never receive a second delta.
Generic SetVelocity interception, remaining weapon-specific pushes/trapped branches,
real-process lethal PvP and full playable-network qualification remain separate.


## Graphical held fire and real-process combat

The [Bloodbath combat connection](../implementation/reports/m9_pvp.md) enables
held primary/alternate fire through NetworkClient after reliable selection/USE/
alternate commands. Reuse existing bindings and press suppression; menus/focus
neutralize input without suspending the host. Keep original weapon timing,
primary/dual-hand rules and one CMoveMgr update per client tick. Presentation
prediction never applies damage. Original slow death and confirmed death remain
distinct; USE respawn changes force before the following original HUD update.

Two independent clients verify actual Beretta shots, private damage/impulses,
one frag, new-life respawn and disconnect on the dedicated. The graphical host
verifies the same loop between two independent remote clients while its local
player/rendering remain active. These scripted retail encounters are bounded
qualification, not human LAN or full M9 acceptance. Keep remaining remote audio/
FX, scoreboard/chat, match limits/rotation, Team/CTF and broad combat qualification
explicit. Preserve solo, PBR, immutable GPU references and independent M2/M3 gates.
