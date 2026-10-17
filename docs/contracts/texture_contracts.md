# DTX textures and opaque world materials

M2 remains incomplete. This extends the [world view](world_view_contracts.md) with
bounded external DTX decoding and opaque base-color sampling, independent of ImGui.
No original game rule, source-class accounting or dependency pin changed.

## Data boundary

`read_texture` accepts resource type 0 / signed version -2 only. Header fields are
little-endian: 16-bit width/height/mip count/section count at offsets 8–15, 32-bit
flags/user flags at 16–23, twelve opaque extra bytes at 24–35, and two opaque
serialized slots at 36–43. A 256-entry palette begins at 44; mip indices begin at
1068. Do not interpret the extra bytes as the newer -5 BPP identifiers, append a
128-byte command string, follow serialized pointer values, guess by basename, or
borrow another resource's palette.

The selected retail corpus supports this layout with exact lengths: 3,820 version
-2 images decode, including 1,450 with flag 2 and separate packed four-bit masks.
Five version-0 resources are explicitly unsupported. Names do not decide format.
Palette bytes are converted A,R,G,B to RGBA; the optional mask replaces alpha,
high nibble first, scaled by 17. This mapping follows the reviewed C++ donor and
recognizable retail color previews; the old Python converter contradicts its
channel order. Mask orientation is covered by synthetic tests, not a retail alpha
rendering comparison. World rendering deliberately ignores alpha for this increment.
Flags other than mask presence are retained metadata, not implemented effects.

Dimensions must be powers of two, 1–1,024; mip count 1–8 and no longer than the
image chain. Sections and unknown flags outside the observed 0x2b mask reject.
All stored mip dimensions/indices/masks and the exact ending are checked before
allocation. Input is capped at 3 MiB; output at 4 MiB. Only mip zero is decoded;
other mip/alpha payloads are structurally checked. Failure clears all output and
returns a concrete error. Unknown layouts are not accepted by payload-size guesses.

## World mapping and ownership

The mesh requests exact normalized paths through existing REZ mount precedence.
One owned CPU image per distinct path is shared across all world-model ranges.
At most 4,096 named materials and 128 MiB of RGBA payload are allowed, including
the four-byte white diagnostic slot; failed records retain names/errors. Missing,
oversized, unsupported or malformed materials render an opaque magenta marker.
Malformed geometry/path/UV data and exhausted aggregate budgets fail mesh building
atomically. An intentionally texture-free synthetic mesh retains orientation colors.

UVs are dot(position - surface origin, surface U/V), divided by image width/height.
They use authored positions before subtracting a moving model's DAT pivot. Applying
the current object transform therefore moves the surface and texture together.
There is no global scale, axis flip or guessed fallback UV. Finite values and an
absolute normalized UV limit of one million are checked. Base rings and invisible
surface exclusion remain unchanged. Adjacent triangles with the same material
share a batch within each model range; transforms still resolve from checked objects.

World vertices now occupy 32 bytes (96 MB at the existing three-million limit).
CPU capacities, the separately bounded RGBA payload, upload staging and GPU copies
are additional storage. A single texture transfer buffer batches all image uploads.
Images and vertices upload once per world revision, then draws bind a sampler and
material per batch. Texture lifetime follows geometry clear/unload/reload; SDL owns
deferred GPU destruction. Rejected GPU uploads clear the partial cache. No resource
cache owns an original object or advances simulation. Sampler/pipeline/depth may
persist through unload. The capture blit uses the acquired swapchain dimensions,
which can lag a window resize request; readback dimensions remain independent.

The shared packaged shaders sample mip zero with nearest repeat and output unlit
opaque RGB. Color-space/retail filtering/lightmap parity is not claimed. Transparent
faces and sky retain explicit opaque diagnostic colors; no alpha blending, alpha
test, animated material, fullbright pass, lighting, liquid, sprite or model rendering
is implied. No implicit sun or DirLight behavior is inferred. [Metal artifacts](metal_shader_contracts.md) are now packaged; native Apple execution remains unverified.

## Inspection and verification

The 3D tab distinguishes prepared instances and material batches, decoded/rejected
CPU resources, dimensions, stored mip count, raw flags, byte budget and GPU world
uploads. It does not claim that CPU decoding alone has uploaded or drawn a texture.
Hidden UI and the inspector-free build execute the same material path.

Synthetic tests check channel order/masks, every truncated prefix, invalid versions,
flags/dimensions/counts/endings, exact-path refusal, normalized-path deduplication,
moving-pivot UVs and atomic rejection. Private tests decode the complete mounted DTX
corpus and rebuild/release SubwayA twice. GPU pixels check known texture colors,
repeat, row orientation, alpha-ignored policy, reload replacement and rejected-upload
cleanup alongside the existing depth/camera/resize checks. The real-world smoke
retains movement, hidden UI and unload/re-entry assertions and now requires at least
50 decoded materials and no rejected material. See [run evidence](../../../Documentation/Implementation/m2_textures.md).

The [sprite presentation continuation](sprite_view_contracts.md) now draws supported
original sprites with actual DTX alpha and shared world/model depth, before view
weapons. Earlier blanket sprite-rendering omissions are superseded for this bounded
subset. World/model opacity policy is unchanged; required FX and M2 remain open.

The [world-alpha correction](world_alpha_contracts.md) supersedes earlier blanket world-opacity/transparent-marker statements for resolved bit-3 surfaces. Sky and rejected resources remain opaque markers; ordinary surfaces still ignore alpha. The new sorted world pass precedes the separately sorted translucent FX passes. M2 remains incomplete.

The [surface panning continuation](surface_effect_contracts.md) supersedes the blanket animated-material omission for bounded authored Pan effects. Rates belong to batches independently of shared images; session time controls both world passes. Other effects and M2 remain open.

The [sky continuation](sky_contracts.md) supersedes blanket sky-rendering omissions
for registered world-model skies. Valid visible skies render behind the world and
replace bit-4 portal markers; unavailable skies retain diagnostics. No retail
lighting, fog or complete M2 acceptance is implied.

The [CityHub entry continuation](cityhub_entry_contracts.md) additionally resolves
authored SPR world materials, decodes/deduplicates all same-size DTX frames and
binds frames from the read-only world clock. Manifest/frame errors still block
NEW_LEVEL. Both world passes share the phase; geometry, alpha rules and shader
artifacts are unchanged.
