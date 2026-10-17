# Animated model presentation

> [Prepared model geometry](model_preparation_contracts.md) extends the per-frame
> rebuilding policy below with bounded exact reuse for immutable shared assets.
> CPU skinning, current visibility and world-revision teardown remain explicit.

Continuation: [original explosion models](explosion_model_contracts.md) now supplies
selected model components with independent lifetime and alpha rendering. The earlier
model omission is superseded only for that subset; complete composite FX remain unavailable.

> Extended by [node visibility and player model restoration](character_node_contracts.md):
> original named resets now affect rendering/hit candidates, and the selected Load
> model/flags/velocity tail is available before client association. Earlier blanket
> node-hide/Load-tail omissions are historical. Full engine/client restore, limb/gib
> behavior and the M2 transfer gate remain incomplete.

This M2 continuation renders existing ABC objects, held attachments and first-person
Beretta/knife tracks through SDL_GPU. It adds no NPC class, AI behavior or successful
missing service. The 17 unsupported SubwayA entries and M2 gate remain unchanged.
See [implementation evidence](../../../Documentation/Implementation/m2_model_view.md).

## Poses and original placement

A frame snapshots checked Alive OT_MODEL records, including inactive and hidden
records. Visible geometry samples the existing animation index/time, deformation,
object scale and current object/attachment pose. It never advances animation clocks,
emits keys, refreshes attachments, integrates physics or invokes game callbacks.
Pending records disappear immediately; independent held children remain alive until
the original owner's deferred destructor removes them. Unresolved attachments stay
listed but do not draw. Missing models/poses and skins retain named diagnostics;
missing skins use magenta, never another asset with the same basename.

The original client visibility policy hides the local player and attached held
models in first person, without changing server flags. Scripted views honor the
existing player/weapon visibility selection. Client weapons retain their owned ABC
tracks; no fake server HOBJECT is introduced. Selected original
CViewWeapon::AdjustAngleAndSetPos computes eye + rotated WeaponData offset, including
left-hand X reflection. Bob inputs remain zero. Muzzle/firing origin, recoil, zoom,
character-specific skins and full source UpdateGun are not implemented here.

Visual inspection exposed an older ABC reader defect: omitted TransformInfo was
being treated as explicit zero. Reviewed ABC v6 readers default to inverse node
rotations. The data boundary now conjugates normalized serialized quaternions when
that section is absent. Explicit zero keeps rotations unchanged; explicit nonzero
flags still reject because that optional file variant has not been validated. All samplers,
node queries and attachments see the same converted poses. Constructed in-memory
models already use engine quaternions and are unaffected. No world/object quaternion
convention changes. A nonidentity synthetic file checks both cases and parent/node/
deformed-vertex composition; the retail Beretta's own static-key bounds independently
reject the old pose. This is not a claim of complete retail model-rendering parity.

ABC vertices and the Beretta muzzle/hand ordering face -Z, while the existing
original camera/fire boundary uses +Z. Decode now reflects Z for geometry, normals,
all bounds, node translations, packed-deformation scale/translation and uses S*R*S
for node quaternions. This basis conversion is separate from quaternion inversion
and TransformInfo's winding flag. Metadata is also in engine coordinates; animation
dimensions stay nonnegative. Root translation is retained, and no per-weapon
rotation/scale workaround is introduced. Face corner order and UV associations
remain authored; rendering stays two-sided. Native coordinates must not be mixed
with the normalized sampler output. Synthetic files encode their intended +Z
deformation in file -Z, preserving previous engine-space assertions. The private
Beretta test requires its muzzle to lie ahead of its hand along firing +Z.
This convention is inferred from real geometry/source firing axes and visually
checked; exact retail pose/attachment/projection parity remains pending.

## Resources and GPU lifetime

ModelView accepts already validated immutable models from the bounded reader.
It rebuilds bounded geometry/draw lists each frame, retaining vector capacity
and exact case-folded skin results until the world revision changes. At most 4,096
entries, 4,096 named skins, 128 MiB decoded RGBA, 65,536 vertices/200,000 faces per
model and one million expanded frame vertices (32 MB) are accepted. The shared
DTX resource helper retains the three-MiB input bound. Cache failures remain visible;
reload retries them. Invalid geometry/transform/UV or budget failure clears the
frame and returns failure. Shader/texture/GPU storage is additional bounded memory.

The existing opaque world shader samples authored face UVs and mip-zero nearest
skins. Dynamic vertices upload each frame through reusable, cycling GPU/staging
buffers; texture uploads happen only when the skin-cache revision changes. Growing
the vertex buffer replaces it with a checked allocation. World revision teardown
clears CPU skins/geometry and GPU buffers/textures; SDL owns deferred GPU release.
No original object pointers survive snapshot construction.

World models load the depth stored by the DAT pass, so walls occlude them. Close
weapons preserve color and use a separate cleared depth target with near 0.01,
far 65,536 and the existing 90-degree horizontal projection. This is replacement
presentation for the source FLAG_REALLYCLOSE intent, not verified retail projection.
It cannot overwrite world depth for later world geometry. Resize recreates targets.
Both paths work with ImGui hidden or compiled out. Current artifacts remain
Vulkan/SPIR-V only. Lighting, animated normals, alpha/chrome effects, LOD selection,
sprites, liquids, sky and required client FX remain pending.

## Inspection and evidence

The 3D view panel lists each source model or view weapon, visibility, triangles,
skin path and decoding/pose/attachment failure, alongside resource counts. Hidden
PathPoints remain listed; missing NPCs are still unsupported World entries, never
invented model instances. In SubwayA, the diagnostic third-person snapshot has
Caleb (941 triangles) and his held Beretta (68); first person has one view weapon
(878) and hides those two. Seven PathPoint models stay hidden. The private GPU
Caleb image explicitly overrides presentation visibility for inspection; it does
not add a production NPC or alter game rules.

GPU pixel checks cover shared-depth front/back occlusion, changing vertex positions
without texture reload, resize and close-view depth isolation. CPU tests cover
bounds, pose conventions, exact cached paths, source offsets, read-only clocks,
hidden/removal state, weapon selection and two unload/re-entry cycles. The normal
viewport smoke captures a real first-person model with ImGui absent from the image.

The [sprite presentation continuation](sprite_view_contracts.md) now draws supported
original sprites with actual DTX alpha and shared world/model depth, before view
weapons. Earlier blanket sprite-rendering omissions are superseded for this bounded
subset. World/model opacity policy is unchanged; required FX and M2 remain open.

The [Gideon fade/teleport continuation](gideon_teleport_contracts.md) adds the original selected states, live model color/alpha, real transparent model presentation and supported player color preservation. Earlier blanket omissions are historical; production boss lifecycle and M2 remain incomplete.
