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
