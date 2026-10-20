# Original DetailSprite and SPR resources

M2 remains incomplete. Read [object lifetime](object_contracts.md),
[world loading](world_contracts.md), [collision](model_and_collision_contracts.md),
[source selections](../implementation/sources/m2_sprite_sources.json) and
[implementation evidence](../implementation/reports/m2_sprites.md).

## Source behavior

DetailSprite retains the original B2BaseClass callback chain and OT_SPRITE=3.
World/string precreate reads Filename, ScaleX/Y, FlushWithWorld, Rotatable,
Chromakey, Repeat, RepeatX/Y and SpacingX/Y; runtime creation retains the supplied
creation structure. The registered Rotatable default is true, but the runtime
constructor's member is false. Precreate clears the skin, sets the scale and
visible/rotateable/chromakey flags; BaseClass property overrides still run after
these writes. Initial update disables the 0.1-second schedule and marks saveable.
The saveable flag does not imply save/restore support.

Flush uses the authored forward vector with IGNORE_NONSOLID and without
INTERSECT_OBJECTS. The typed CastWorldRay queries only solid primary-world
triangles. It excludes dynamic objects/world models and clips an infinite ray to
the existing +/-10,000,000 collision domain. Invalid/unavailable differs from a
miss. A hit moves the object to point + normal * 0.25 using TeleportObject, the
SDK-documented equivalent of SetObjectPos. Rotation stays unchanged: the source
only assigns a local forward variable. This is replacement collision, not a
verified recreation of retail ray semantics or backface behavior.

Repeats execute only inside the flush branch, including on a ray miss. Creation
order is the remaining first Y column, then each X column with all Y rows. Offsets
use the original up/right vectors, not the hit normal. RepeatX=0 retains the first
Y column; RepeatY=0 creates no children. Runtime children inherit position,
rotation, scale, flags and filename, while their own Repeat/Flush members remain
false. They never recursively repeat. Children are independent; parent removal
does not remove them. Source B2 difficulty filtering runs after child creation,
so filtering a parent can leave its runtime children alive.

Filename is value-owned; diagnostic child handles do not own objects. Signed
counts cast to unsigned as in the source, then reject outside 0..1024 or when
max(1, RepeatX)*RepeatY exceeds 1024. Nonfinite/out-of-domain scale, spacing and
child positions fail. These are explicit safety bounds; negative counts do not
produce huge unsigned loops. Partial repeat failures remove the failed parent
but retain already created independent children, with an explicit error. Unknown
properties, restore, missing SPR and missing frame resources fail creation.

## Manifest and playback services

SPR has five little-endian uint32 fields: frame count, frame rate, transparency,
translucency and color key; each frame has a uint16 byte length and texture path.
The later-generation SpriteFile/runtime readers confirm this layout; the actual
Blood II resources independently pass it. No donor implementation is imported.
The reader accepts 1..4096 frames, 1..1000 fps, boolean flags, paths up to 1000 bytes,
and at most 1 MiB. Truncation, NULs, unsafe/empty paths, non-DTX frames and trailing
bytes reject without changing the previous decoded manifest.

SpriteStore resolves exact mounted SPR/frame paths, caches shared immutable
manifests by archive and byte range, rechecks frame presence on cache hits and
releases cache entries on world unload. It does not decode DTX or infer aliases.
A resource being indexed is not evidence of valid texture pixels. The manifest
binds after original precreate and before initial update. A failed binding retains
a durable Failed world entry, never a successful placeholder instance.

Each live active object has its own looping frame clock. It uses the integer
1000/fps millisecond period visible in the later-generation runtime reference,
with fractional milliseconds retained across simulation steps. This timing policy
is an explicit adaptation, not verified Blood II retail timing. Visibility and
inspector state do not stop it; explicit inactive state pauses it. The existing
model-update traversal advances sprites without a separate per-frame snapshot.
No model-string events are invented for sprites. Invalid deltas leave clocks
unchanged; unload releases instances/manifests and invalidates inspection IDs.

## Inspection and limits

The Sprites tab shows authored source dispositions, failures, live and runtime
repeat instances, hidden/inactive state, transform, flags, original properties,
flush result/polygon, repeat handles, manifest metadata and current frame presence.
Object IDs distinguish unnamed/repeated instances. The XZ overlay shows positions
and authored forward axes only. DTX decoding,
textured world drawing, billboards, transparency/depth behavior, clipping,
SpriteControl APIs, save/restore and verified retail visual/timing parity remain
unavailable. The ordinary World view retains all rejected source properties.

SubwayA's twelve authored blood splats all bind sprites\bloodsplat3.spr: one
frame, 15 fps, transparency true, translucency false, color key 0, referencing
SpriteTextures\bloodsplat3.dtx. Scale is 0.5, Y=129, Rotatable/Chromakey true,
Flush/Repeat false. Across 57 readable worlds, 124 DetailSprite entries contain
120 valid referenced manifests, four missing/empty references, and one old
TypeAction schema rejection. The two accepted flush placements hit real Temple
entrance geometry. No catalog entry repeats; synthetic tests cover that branch.
The known 16_bridge.dat structural rejection remains unchanged. No commercial
resource bytes are copied into the repository.
The later [object contracts](object_contracts.md#retired-world-properties) supersede TypeAction rejection: the retired flag is accepted and never read.

The [sprite presentation continuation](sprite_view_contracts.md) now draws supported
original sprites with actual DTX alpha and shared world/model depth, before view
weapons. Earlier blanket sprite-rendering omissions are superseded for this bounded
subset. World/model opacity policy is unchanged; required FX and M2 remain open.

## Zero-length original animations

The complete-level audit found `sprites/floorplate.spr` with five valid DTX frames
and raw uint32 FPS 65537. The original installed Client.exe loader at 0x463a10
reads that uint32 unchanged, stores it at animation+0x2c, and calculates
`(1000 / fps) * count` (0x463b09–0x463b1f). The tracker initializes frame zero
(0x463c60) but clears its current frame when animation length is zero
(0x463cad–0x463cb2, 0x463d08). The world-surface update at 0x401c23–0x401c39
only replaces the surface texture when a current frame exists, retaining the
initial texture otherwise. The later runtime source confirms the tracker rule.

The reader therefore accepts every nonzero uint32 FPS without truncating or
clamping it. Existing count, byte, boolean and path bounds remain. A period of
zero clears `SpriteTrack::has_frame` without division/modulo by zero. Sprite
objects stop drawing, PolyGrids have no sprite texture, and world materials retain
the initial frame. FPS zero still fails instead of reproducing division by zero.
These rules do not alter commercial files or fabricate an animation speed.
Client.exe SHA-256: `edf7442f26dcbef72c3b69b4560d0fa41d6c9dafa45e603fb12bd32fe6eb2dd7`.
