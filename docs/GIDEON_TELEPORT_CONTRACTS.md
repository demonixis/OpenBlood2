# Original Gideon fade and teleport states

Selected `MC_FadeOut`, `AI_STATE_Teleport` and `AI_STATE_Special1` now execute
through the original state dispatcher. See the [source ledger](M2_GIDEON_TELEPORT_SOURCES.json),
[combat contracts](GIDEON_COMBAT_CONTRACTS.md) and
[execution evidence](../../Documentation/Implementation/M2_GIDEON_TELEPORT.md).
These remain diagnostic boss methods. Production initialization, boss-health and
damage dispatch, attack/dodge sequencing, shared boss table and save/load are not
implemented. No production Gideon is registered, and M2 remains incomplete.

## Source behavior

The first fade call sets command 101/animating and clears SOLID. It neither changes
alpha nor starts/resets a model animation. Each later call subtracts exactly 1/16
from alpha, clamps at zero, and preserves RGB. Reaching zero clears animating and
increments Metacmd once. A fresh opaque actor therefore needs 17 calls, regardless
of elapsed time. VISIBLE stays unchanged. Completion overflow refuses before the
color/command mutation.

Teleport step 2 attempts `teleport.wav` through the original CAnim_Sound helper,
with radius 1000 and volume 100. A playback failure is recorded but does not stop
the original command advancement. The source does not request GETHANDLE: a zero
helper return is normal even when playback starts. Inspection uses the helper's
actual start count/error; no retained voice is fabricated. The real mixer owns the
attached unretained voice and releases it on completion/removal under existing rules.
Special1's step 2 does nothing except increment; the commented laugh stays absent.

Teleport step 3 draws X/Z in [-1,1], uses Y=0, and casts from **CMovement's cached
position** to that position plus the unnormalized direction times 1500. Special1
uses the current checked target position, direction Y=.2 and one additional draw
in [400,600]. Both restore SOLID before the ray. A hit requests its point minus
direction*100 (Teleport) or *300 (Special1). A miss requests the actual ray endpoint:
the source computes a shortened point but never uses it. Do not normalize the
vector or apply the unused offset. Portable RNG order is explicit X then Z then,
for Special1, distance; this is not retail CRT bit parity.

The typed ray includes all object types without a filter or ignored owner. Its
existing replacement entry-face policy skips enclosing ordinary objects; the
primary-world index remains solid-only. This is not verified retail ray parity.
The move calls collision-aware MoveObject, not TeleportObject. Actual positions can
be clipped or slid by the existing solver, including the primary floor. Requested
and achieved positions are distinct inspection fields. Initial penetration,
missing collision, invalid input, unsampled cache and stale targets fail explicitly.
Earlier source mutations (including SOLID before a failed query) are not rolled back.
No additional velocity integration or forced movement-cache refresh is added.

Step 4 restores alpha to one, retaining RGB. Step 5 uses the existing stored-target
facing command; step 6 computes a new state. If Special1 has no target, it changes
to Teleport and deliberately continues into the same callback's first fade step.
Other special slots retain their existing base handler. Other classes' Teleport
handler reports unavailable; no generic successful state body is substituted.

## Checked lifetime and model color

Nested teleport/fade calls from a move contact reject. After contact callbacks,
owner, target (for Special1), state and Metacmd are checked before advancing. Pending
removal cannot restore color, complete another command or schedule a successful
move. Reclamation remains deferred. Failure can retain already committed collision
motion; there is no claim of transactionally undoing gameplay callbacks.

GetObjectColor/SetObjectColor are typed boolean services for live OT_MODEL records.
The original SDK specifies RGB=(0,0,0), alpha=1; records preserve that default.
RGB and alpha must be finite in [0,1]; invalid
writes change nothing, failed reads leave outputs unchanged. Other object types,
foreign/stale/pending handles reject. Unsupported sprite/world/light color mutation
is not advertised. These are per-instance values, not resource edits or new globals.
The supported player engine snapshot now validates, captures and restores these
values, so the new service cannot silently lose player color during supported
restoration. Original Load overrides and client association rules remain unchanged.

## Presentation and inspection

Model preparation retains alpha on each draw and inspector entry. Original RGB is
additive lighting intent, not texture multiplication. It remains stored/inspectable
while the renderer uses its existing fullbright textures; additive model lighting
is explicitly unavailable. Missing skins stay diagnostic magenta. Alpha<1
marks that instance's batch transparent; the ordinary opaque pass skips it. The
existing SDL_GPU alpha pipeline blends it in a separate pass with shared scene
depth and no depth writes. Alpha zero contributes no pixels. Reappearance returns
the model to the opaque pass. Hidden/inactive and missing models retain inspection
records; rendering does not advance animation or state methods.

Transparent models sort by object-origin depth within their own pass. The pass
follows transparent world faces and precedes detached model FX/sprites; there is
no global transparency ordering, per-triangle self-sorting, lighting or verified
retail renderer parity. Detached explosion models keep their prior alpha path.
Resource revisions, unload/reload and GPU ownership remain unchanged.

Objects shows RGB/alpha. Skeleton shows fade calls, successful moves, ray status,
requested/actual positions (explicitly unavailable after a failed move) and real teleport playback success/error, alongside
existing state commands and failures. The UI explicitly identifies the still
incomplete boss lifecycle. No test actor is inserted into normal worlds.

SubwayA remains 203 created / 4 transferred sounds / 372 editor-only /
1 unsupported Gideon / 0 creation failures. Required boss services/FX and safe
NEW_LEVEL keep the actual M2 gate open. Linux validation is not a retail boss
playthrough, other-platform claim or XR verification.
