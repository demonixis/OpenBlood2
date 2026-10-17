# Diagnostic 3D world view

M2 remains incomplete. This is the first real world rasterization in the new SDL_GPU
runtime. It renders existing DAT geometry and current world-model transforms with
the existing original player/camera selection. It is not the M3 playable slice or
the complete M4 renderer. See [evidence](../../Documentation/Implementation/M2_WORLD_VIEW.md).

## Geometry and identity

`WorldMesh` owns CPU vertices and one range per DAT model. The primary model stays
in original world coordinates. Additional models store vertices relative to their
DAT translation pivot; each live OT_WORLDMODEL draws with its current object position
and quaternion. This matches the existing posed collision shell, including authored
rotation and subsequent translation/rotation; DAT min/max are not added to positions.
No second scene graph, ECS or donor renderer was imported.

The v56 base ring is triangulated as a fan; serialized extra ring vertices are not
appended as another fan. BSP convex-face semantics are assumed, not a general
concave-polygon tessellator. Indices, finite positions/normals, ring size (3–512),
model count (4,097), per-model points/polygons (65,536) and emitted vertices
(3,000,000 / 72 MB of vertex payload) are checked. Vector capacity, staging and
GPU copies require additional bounded storage. Failure empties the mesh. Degenerate triangles are
counted and omitted. The existing bounded DAT reader remains the data boundary.

Invisible surface bit 2 is omitted. Bit 3 transparency and bit 4 sky are counted and
shown as opaque colored diagnostic surfaces. Their values were checked against
later-generation LithTech definitions, not claimed to prove complete v56 material
parity. Ordinary face colors encode absolute normal components; they do not infer
a directional light. The user's DirLight/implicit-main-light observation remains
unresolved. There are no textures, lightmaps, dynamic illumination or shadows yet.

Every frame resolves current checked records. Alive inactive objects remain visible;
FLAG_VISIBLE=false and pending/removed records do not draw. Exact case-folded named
world-model matches must be unique and backed by the existing collision shell.
Containers are excluded; unbound DAT models are counted, never drawn at guessed
locations. Models, held/view weapons, sprites, liquid surfaces, particle FX and
unsupported source objects are not rendered. The existing World/Objects tabs retain
all those records and their diagnostics.

A monotonic `GameSession::world_revision` changes after successful teardown, including
reloads completed between two presented frames. Diagnostic harnesses that append
world geometry also increment it; geometry is otherwise immutable after loading. GPU geometry is cleared/rebuilt on
revision change. Rejected world reads preserve the old revision/world. CPU/GPU caches
own no original object pointer or handle and never create objects, call game commands,
advance clocks or integrate physics. Static vertices upload once per loaded revision;
per-frame world transforms are small uniform blocks. SDL owns deferred GPU release.
Depth/pipeline allocations can persist through unload; geometry cannot.

## Camera and GPU

The existing `CBloodClientShell::view` supplies the selected original CameraObj pose
or player eye and pitch/yaw. No new movement or camera simulation is introduced.
Camera type 1 uses the source one-sixth top/bottom letterbox request. The diagnostic
projection is explicitly 90 degrees horizontal, near 1, far 65,536 original units;
retail FOV, bob/recoil/death views and zoom are not claimed.

Original +Y up / +Z forward are projected to SDL's left-handed NDC with Z in [0,1].
SDL performs backend orientation conversion. D32 depth uses LESS, writes and clipping;
both windings draw to aid inspection. Swapchain dimensions recreate depth targets,
bounded to 8,192 per axis. The world pass precedes a separate ImGui color-load pass.
UI visibility has no effect on rendering or simulation; no-ImGui Release uses the
same world path. Presentation without an entered player remains inspection-only.

The packaged world shaders now support Vulkan/SPIR-V and Metal/MSL. See
[Metal shader contracts](METAL_SHADER_CONTRACTS.md) for backend selection, generated
bindings and optional SDL_shadercross regeneration. Both artifacts are checked in;
ordinary builds need no shader toolchain. macOS execution is user-confirmed; native automated tests remain pending,
while Linux Vulkan has actual GPU evidence. D3D12 artifacts remain unavailable.

## Use and evidence

`--view-world` mounts the selected world and enters the existing original player
subset, starts with the inspector hidden and keyboard controls enabled, and retains
the authored intro. F1 toggles inspector visibility; its title arrow folds/unfolds it. F2 toggles movement,
which now also starts enabled for normal Session entry. Physical WASD/ZQSD,
arrows, Shift, Space, Ctrl and E retain existing movement/look/USE rules and focus/text
input handling. Mouse-look, weapon mouse bindings and rendered view weapons are not
added by this increment. `--start-game` still exits 6 for required missing services.

The 3D view tab reports previous-presented-frame GPU counts, prepared draw lists, source mesh omissions,
current bindings and selected camera. `--viewport-smoke` exercises original SDL
walk/run/jump/crouch, hidden UI, unload/re-entry and resize; its capture excludes UI
and requires at least 25% non-background pixels. Synthetic GPU readback checks real
occlusion, both windings, near clipping, axis orientation, camera/object transforms,
resize, letterbox and reupload. Headless cases cover malformed mesh inputs, base
rings, projection, invisible/inactive/pending lifetime and private posed geometry.
No pixel-identical retail comparison, optimization or additional platform is claimed.

The [DTX continuation](TEXTURE_CONTRACTS.md) supersedes the absence of world textures: opaque unlit base colors now render, with named rejection diagnostics and bounded cache ownership. World vertices are now 32 bytes; the three-million-vertex payload limit is 96 MB. Earlier geometry limits, current transforms and the M2 gate remain unchanged.

The [animated model continuation](MODEL_VIEW_CONTRACTS.md) supersedes earlier model/skin rendering omissions for existing ABC/held/view models. It also corrects the absent-TransformInfo quaternion default at the reader boundary, shared by all pose/node/attachment queries. Explicit zero remains direct and nonzero flags remain unsupported. Close-view projection is replacement presentation; required gameplay/FX and M2 remain open.

The [sprite presentation continuation](SPRITE_VIEW_CONTRACTS.md) now draws supported
original sprites with actual DTX alpha and shared world/model depth, before view
weapons. Earlier blanket sprite-rendering omissions are superseded for this bounded
subset. World/model opacity policy is unchanged; required FX and M2 remain open.

The [world-alpha correction](WORLD_ALPHA_CONTRACTS.md) supersedes earlier blanket world-opacity/transparent-marker statements for resolved bit-3 surfaces. Sky and rejected resources remain opaque markers; ordinary surfaces still ignore alpha. The new sorted world pass precedes the separately sorted translucent FX passes. M2 remains incomplete.

The [sky continuation](SKY_CONTRACTS.md) supersedes blanket sky-rendering omissions
for registered world-model skies. Valid visible skies render behind the world and
replace bit-4 portal markers; unavailable skies retain diagnostics. No retail
lighting, fog or complete M2 acceptance is implied.


The [flashlight continuation](FLASHLIGHT_CONTRACTS.md) adds original item2, L toggle,
active-only charge, local audio, checked child lifetime and forced-inactive
Restore/KeepAlive. One radial point light is rendered over the unlit world/models;
GetPointShade, shadows and general dynamic illumination remain unavailable.
Earlier blanket flashlight omissions are superseded only by this bounded subset.

The [original PolyGrid continuation](POLYGRID_CONTRACTS.md) supersedes the blanket
liquid-surface omission for static VolumeBrush surfaces. Underwater fog, moving
volumes, rendering parity and the actual M2 gate remain open.
