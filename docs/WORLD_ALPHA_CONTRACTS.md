# Transparent world surfaces

This playtest correction extends [world textures](TEXTURE_CONTRACTS.md) without
changing original objects, damage, collision, visibility, resources or dependency
pins. See [evidence](../../Documentation/Implementation/M2_WORLD_ALPHA.md).
M2 remains incomplete.

## Selection and data

SubwayA's 31 visible transparent polygons use the exact normalized resource
`textures/greys/dirtyglass.dtx`. It already decoded successfully before this fix:
64x64, flags 10, four-bit mask alpha 51–102 (20–40%). The old pink was a deliberate
surface marker, not a missing file. Invisible faces on the same brushes still
follow bit 2 exclusion; their `Invisible.dtx` name does not determine visibility.

Surface bit 3 selects blending only when a real texture resolves. Bit 4 sky retains
its opaque sky marker, even with bit 3. Missing/unsupported textures remain named
errors and opaque magenta markers, so failure cannot turn geometry invisible.
Texture-free synthetic diagnostics retain orientation/pink colors. Opaque surfaces
still ignore texture alpha, including when they share an image with a transparent
surface. No filename-based alpha or color-key rule is added. DTX decoding, generation
boundaries, exact-path lookup, resource and vertex budgets are unchanged.

One translucent batch covers one original base-ring polygon. It is never merged
with another polygon, even with an identical material. Its center is the mean of
the authored base-ring vertices, stored relative to the same DAT model pivot as
geometry. UVs remain computed before subtracting that pivot. Degenerate triangles
remain excluded; a wholly degenerate polygon creates no blend batch.

## Ordering, depth and lifetime

Each frame builds a far-to-near order using the current checked object's transform
and camera-forward depth of each face center. Equal depths retain input draw/batch
order. Translation, rotation and camera changes affect the next order; no original
object handle is cached. Hidden, pending and removed models contribute no draw;
inactive visible models still can. Unbound/ambiguous identities retain existing
failures. Ordering validates finite camera/poses, triangle/range bounds, at most
65,536 instances and one million face instances. The reusable vector needs up to
24 MB of element storage on this 64-bit build (capacity can be larger); batches
now occupy 28 bytes each. Existing three-million-vertex mesh limits still apply.

The renderer owns a second pipeline but shares the world's existing vertex buffer,
images, repeat sampler and depth target. There is no duplicate world upload or
second simulation integrator. It reuses the existing alpha fragment shader:
straight source-alpha blending, zero-alpha discard, depth LESS and **no depth
writes**, unlit mip-zero nearest repeat sampling. Generic object tint/alpha is
not implemented by this change; world modulation is 1. Sprites keep their existing
clamp sampler and effect alpha.

The app draws opaque world and opaque character/held models first, then sorted
world alpha, then the existing separate translucent FX passes and close-view
weapon. This gives ordinary opaque objects in front of/behind glass correct depth
and compositing. Sorting is by polygon center; intersecting transparent geometry
and interleaving with separately sorted translucent FX are not globally correct
or retail-verified. Sky, lighting, liquid presentation and full renderer parity
remain later work. No guessed sun or DirLight behavior is introduced.

Alpha draws require `DrawLoad::Load` on a prepared opaque renderer; malformed
orders reject before opening a render pass. Revision changes clear geometry,
images and the prepared order; SDL deferred destruction protects in-flight GPU
work. Pipeline/depth allocations may persist. Format changes invalidate both
pipelines. Hidden UI and inspector-free builds use the same path.

## Inspection and verification

The 3D tab reports authored transparent faces separately from prepared blend faces
and actually submitted alpha batches/passes. Its named face list includes hidden
and unbound models; the existing object visibility and resource errors explain
why preparation need not mean submission or on-screen visibility. The frame count
for the ordinary world pass stays distinct from alpha pass submissions.

CPU tests cover shared opaque/transparent materials, missing-resource markers,
sky precedence, per-face ordering, stable ties, moving/rotating instances, camera
rotation, invalid bounds/camera, inactive/hidden/pending removal and two private
world reloads with 31 bound glass faces. GPU pixels cover zero/partial/full alpha,
opaque black, overlap ordering, opaque geometry between/in front of panes, absent
depth writes, posed cameras/objects, resize/reupload and rejected draw recovery.
A real glass12 pane is checked against independent authored UV/decoded-texel
compositing after two actual loads; full scene captures remain local. Application
smoke requires 31 prepared/submitted faces through hidden UI and reload with no
extra world upload. Other platforms, retail pixel parity and human playthroughs
are not claimed.

The [sky continuation](SKY_CONTRACTS.md) supersedes blanket sky-rendering omissions
for registered world-model skies. Valid visible skies render behind the world and
replace bit-4 portal markers; unavailable skies retain diagnostics. No retail
lighting, fog or complete M2 acceptance is implied.
