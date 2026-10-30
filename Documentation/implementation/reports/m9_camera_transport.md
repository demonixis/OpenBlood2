# M9 — network camera state and authoritative view gates

2026-10-30, following `ab81b88d`. Camera selection now reaches independent clients
through the reliable private event stream. The server derives movement/look gates
from its original camera/owner objects; the client reconstructs presentation from
replica poses and its predicted body. Interactive rendering and the remaining
presentation consumers still belong to M9.
See the [contract](../../contracts/multiplayer_contracts.md) and
[source ledger](../sources/m9_camera_transport_sources.json).

## Behavior

Protocol 8 appends private event tag 21, `CameraState` (**86 bytes** including the
private header). It contains the full movement force generation, first active
scripted camera ID/settings, checked Eye/Orb IDs, eye height and alive state.
Positions, rotations and GPU data are not duplicated: the existing ClientWorld
provides remote poses. The server's existing GetCameraView preserves descriptor
creation order, so the selected scripted descriptor is enough; a client never
needs a network copy of the whole 4,096-entry server camera list.

Eye/Orb identities follow original private message semantics. Eye clears before
accepting a new active identity; an Orb off message clears only the matching
current Orb. Refresh checks live type and firing-owner identity, clearing retired
or unrelated references. It derives `camera_movement_blocked` from the same rules
as LocalPresentation::commit before deriving movement setup. Scripted/dead look
cannot overwrite authoritative aim; Eye/Orb keep the existing original aim path,
including active Eye rotation. Movement commands are still integrated only by
client CMoveMgr. Report destinations remain under the selected client-authored,
collision-checked model; this does not add competitive input simulation.

Changed camera state is appended after queued original private events, including
PlayerDead. The force generation participates in comparison, so death/respawn
coalescing back to alive cannot leave an older death notification applied last.
This shares private event bounds/order and refuses queue failure explicitly.
No camera ID or permission is accepted from a peer.

`NetworkCamera` builds existing ClientCamera frames from replica IDs/poses and
local predicted position, retaining the original Eye → Orb → scripted priority,
body-versus-camera listener, hide-player/weapon flags, death orbit, chase collision
and XR chase refusal. The predicted eye never edits the authoritative replica.
Missing cross-channel objects or mismatched force generations defer presentation
without replacing the last valid view. Successful updates commit a checked camera
candidate; invalid time/aim and backwards presentation time are refused.

PlayerDead remains the only death-orbit trigger. Damage-dead/slow-death state alone
does not start it, and repeated death notifications preserve the original first
start. If a deliberately delayed presentation clock has not reached the event's
server tick, the view waits rather than passing a negative elapsed time to the
original orbit. An authoritative alive transition/force clears the prior death
state. The caller supplies its mapped world clock; production clock synchronization
and the interactive frontend remain subsequent work. Existing solo camera/FOV,
zoom, post-processing and XR code paths are unchanged.

## Verification

The native selection covers **82 distinct tests**: network admission, codecs,
replicas, movement/private events/views/cameras, solo movement/environment,
player restore and GPU camera/restore checks. ASan/UBSan covers **38 network
tests**. The broad runs initially passed 80/82 and 36/38: both process probes
exposed comparison of a double network timestamp against its float camera copy.
Keeping a separate double last-successful clock fixes repeated fractional times
without accepting backwards time. A targeted fractional-clock regression plus
both process probes then pass **3/3 in each build**. Original float camera/death
arithmetic remains unchanged. Both failed and corrected logs are retained under
`evidence/m9_camera_transport_*`; commercial data remains external.

The native movement process probe runs one dedicated plus two independent client
processes for 900 server ticks. Both clients render headless camera state at the
36-unit eye offset, with 40/7 reports and 39/6 corrections respectively. The
server records two admissions/departures and zero rejected clients, timeouts,
discarded ticks or state drops. `nm -C` finds no GameSession, ServerAdapter,
CBloodServerShell or Objects methods in either independent client fixture.

All five solo oracle traces remain byte-identical to the preceding lot: effects
240 frames, audio 360, movement 360, environment 720 and view 300. All **6,519**
protected PBR files, captures, historical evidence and reference files match their
pre-edit hashes. No GPU reference was regenerated. Native client/server and the
sanitized server-only targets compile; selected GPU camera/restore tests pass on
this Linux machine. These results do not resolve M2-GPU-001.

Commands used (from Blood2; complete outputs are retained):

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_camera_wire_tests network_server_camera_tests network_admission_process_tests -j6
ctest --test-dir build/m9-fx-runtime -R '^network\.(client_movement$|movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_|player_|server_player_events$|announcer|camera_wire$|server_camera$|view_)|^movement\.|^player_restore\.|^gpu\.player_restore$|^containers\.|^cameras\.|^gpu\.cameras$' -E minefield --output-on-failure -j3
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.(client_movement$|movement_|server_movement$|replica_movement$|world_snapshot|world_state$|world_receiver$|wire_collision$|admission_|player_|server_player_events$|announcer|camera_wire$|server_camera$|view_)' --output-on-failure -j3
ctest --test-dir build/m9-fx-runtime -R '^network\.(camera_wire|admission_processes|movement_processes)$' --output-on-failure -j3
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.(camera_wire|admission_processes|movement_processes)$' --output-on-failure -j3
```

The independent fixture verifies source camera priority/listener rules, predicted
eye placement, slow-death versus PlayerDead timing, non-refreshing death start,
future-event deferral, force snap, stale generations, backwards clocks, chase/zoom
and XR behavior. Its first run incorrectly expected repeated PlayerDead to restart
the orbit; the retained failure led to a corrected test of the original
CPlayerDeathCamera::Begin guard, with a separate fresh future-event case.

The retail Crypt fixture creates original CameraObj, SeeingEye and Orb objects.
It checks first-active creation order, distinct movement/look permission,
authoritative aim suppression, active Eye aiming, Eye/Orb identities, removal
cleanup and force-qualified camera publication. The separate-process clients
also consume camera events while continuing independent movement and replication.
These headless views do not qualify interactive multiplayer, degraded combat,
Windows/macOS interoperability, human LAN or mobile/Quest. M2-GPU-001 and M3
acceptance remain independent.
