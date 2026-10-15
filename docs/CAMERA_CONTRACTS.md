# Original scripted cameras

M2 remains incomplete. Read [player](PLAYER_CONTRACTS.md), [movement](MOVEMENT_CONTRACTS.md),
[weapon](WEAPON_CONTRACTS.md) and [trigger](TRIGGER_CONTRACTS.md) contracts together
with these [source selections](M2_CAMERA_SOURCES.json) and
[implementation evidence](../../Documentation/Implementation/M2_CAMERAS.md).

## Original rules

CameraObj retains the B2BaseClass callback chain and difficulty/game-mode filters,
original defaults, world/string properties, start-active behavior, user flags,
ON/OFF/LINK commands, source camera list and target tracking. TypeActionMode is
false by default. The camera is a non-colliding normal object; DebugCutScene=1
requests the original camera model/skin and exposes any resource failure.
Restore is rejected; save/load and the unused source debug-ray FX are not ported.

ON arms a 0.01-second update and resets a positive ActiveTime deadline. Expiry uses
strict `time > deadline`; zero and negative durations stay active indefinitely.
OFF clears the active user flag and schedule. As in the source, a bare LINK
unlinks the target and cancels scheduling even while active, so its timer also
stops until ON or another link rearms it. Missing named targets retain the current
link. Command keywords are case-insensitive; named lookup remains exact and uses
the first match, with the original LINK prefix and name offset rather than new
whitespace parsing. Target deletion sends MID_LINKBROKEN and clears the link.

Camera type, movement, hide-player and listener flags are published once at
initialization. Setup changes the source type without republishing it, preserving
the original behavior. The original Blood II 1.0 workaround suppresses the
**published client listener** flag when GAME_WORLDNAME contains `worlds\`.
Portable slash/case normalization makes the same rule work with normalized REZ
paths. The source IsListener property stays unchanged. World load sets this
variable before creating objects; unload removes it from that adapter only.

## Typed services and ownership

A persistent CameraSettings descriptor replaces the selected SFX_CAMERA_ID packet
and CameraFX metadata. It is value-owned by ServerAdapter, bounded to 4,096 live
entries and keyed by a checked object handle; duplicate, invalid and pending
publications fail. Camera destruction removes its descriptor; unload clears the
registry. No generic SFX manager, packet ABI or success-shaped rendering/audio
service was introduced.

The client selects the first active descriptor in stable creation order, skipping
pending/dead objects immediately. This replaces the old SFX slot array; slot-hole
reuse parity is unverified. The original server g_hActiveCamera uses the source
listener flag and list order: activating a listener selects it; deactivation or a
non-listener activation searches the source list, newest first. These can select
different cameras. A deferred-removal global handle may remain until destruction;
consumers must check liveness. The inspector shows both selections separately.
Server visibility/interest updates using that global are still pending.

Tracking uses a narrow typed AlignRotation service for the source caller's world
Y-up vector. It validates finite bounded vectors, computes pitch/yaw, uses zero
yaw for a vertical direction and rejects coincident targets without replacing the
old rotation. This is new engine math, not verified retail quaternion parity.
Retargeting explicitly breaks the old link (a source lifetime defect); self/stale
links reject. Type outside 0–2 and nonfinite or abs(ActiveTime)>1,000,000 reject
precreate instead of truncating/propagating invalid data. No callback destroys an
object synchronously. The runtime's single bound session remains required.

## Player integration

The client resolves CameraView before movement and again after body movement.
The external pose is separate from the existing player eye used by the selected
weapon path. Remote-camera firing never substitutes the camera's position for
the player's shot origin. Viewport type, player visibility, weapon visibility
and listener position are explicit **presentation requests**; the cinematic world
renderer, letterbox/FOV presentation and audio listener are not implemented.
The later [cinematic/audio continuation](CINEMATIC_CONTRACTS.md) implements the bounded audio listener path; cinematic rendering remains unavailable.
Type 1 requests the source letterbox (top/bottom one-sixth); types 0/2 follow the
source fullscreen branch. A non-listener view requests the default player-body
listener position. Other camera modes/gadgets and third-person/death views remain
pending.

A camera with AllowPlayerMovement=false clears movement commands, requests idle
controls, blocks USE and suppresses new fire input. Original MoveMgr still handles
gravity/environment and the shared body integrates once. The camera gate is
separate from existing player movement blocking. An already-started weapon
animation continues: a later FIRE_KEY receives a typed Blocked acknowledgement
with no shot/ammo consumption, preserving the original server movement guard.
AllowPlayerMovement=true keeps movement/USE/fire available even though external
views request hidden view weapons. Exit/re-entry clears view state; pending camera
removal restores ordinary view/commands at the next client update.

## Inspection and checks

The Cameras tab includes hidden/inactive cameras, source/published properties,
position/forward vector, tracking target/diagnostics, timer/schedule, both selections
and actual player restrictions. ON/OFF/LINK actions queue ordinary original trigger
messages; selection is a generation-checked ObjectId. World keeps failed source
entries. `--cameras-smoke` activates an authored SubwayA camera and tracks the
player, checks its selected view while UI is hidden, and repeats checks after
unload/re-entry. Its diagnostic activation is not a cinematic script or playthrough.

Six synthetic cases cover defaults/filtering, strict timers/unlink quirks, tracking
and link lifetime, server/client selection, player command/late-fire blocking and
validation/adapter ownership. A private catalog checks all 61 CameraObj entries
across 57 readable installed worlds, plus actual SubwayA player selection and
cleanup. SubwayA's original start-point/trigger entry now activates camera1 (seven
seconds), then camera2 (five seconds); control returns just after twelve seconds. The weapon/movement/locomotion private scenarios first assert that
intro and its natural expiry before sending their original inputs; their behavior
assertions remain intact. Related GPU gameplay smoke scenarios also advance the
authored intro to completion (bounded to thirty simulation seconds), including after
reload. No forced OFF, shortened timer or disabled player gate is used. Normal
application entry keeps the intro. The strict existing `16_bridge.dat` rejection remains unchanged.
KeyFramer, CinematicTrigger, world transitions, other required classes/services,
rendering/audio, save/load and complete M2 acceptance remain open. No retail
executable comparison, additional-platform or XR validation is claimed.

The later [keyframe continuation](KEYFRAMER_CONTRACTS.md) supersedes the blanket
KeyFramer omission above for camera/ordinary-object paths. CinematicTrigger,
keyframed world/container motion, rendering/audio and the full M2 gate remain open.

Later [light](LIGHT_CONTRACTS.md), [translating world-path](WORLDPATH_CONTRACTS.md)
and [cinematic/audio](CINEMATIC_CONTRACTS.md) continuations supersede the blanket
omissions above for their bounded subsets. CinematicTrigger now uses real WAV
voices and dry SDL output; KeyData sound requests, reverb, angular/container paths,
general audio/rendering and complete M2 acceptance still remain unavailable.
