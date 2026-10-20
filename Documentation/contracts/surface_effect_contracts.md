# Authored world surface panning

This extends the [world texture](texture_contracts.md) and
[alpha](world_alpha_contracts.md) paths. M2 remains incomplete. Original objects,
collision, camera paths, resource precedence and dependency pins are unchanged.
See [implementation evidence](../implementation/reports/m2_surface_effects.md)
and [provenance](../implementation/sources/m2_surface_effect_sources.json).

## Compatible convention

The original SDK exposes SurfaceData O/P/Q and surface-effect Init/Update/Term
callbacks. The compatible Pan implementation is absent from the supplied game
source. Read-only analysis of the installed original Client.exe establishes:
zero-initialized state; first and second numeric arguments as P/Q speeds; each
update accumulates frame time times each speed; O becomes original O plus P times
the first accumulator plus Q times the second. P/Q remain unchanged. No copied
commercial executable, disassembly or extracted payload is committed.

The retail conversion is atof. Authored words `up`, `down`, `left`, `right` convert
to **zero**, rather than selecting a direction. Preserve this even when a label
looks misleading. SubwayA's 42 negative-Q and 18 positive-Q effects have opposing
Q bases: all 60 texture origins move approximately (0,0,-1024) units/second. This
is tunnel/rail texture motion; do not activate the later happytrain object path or
move the player/collision world to imitate it. The sublight controller is separate.

The supported subset accepts case-insensitive exact `Pan` (no trimming of its
name), zero to two whitespace-separated parameters, finite complete decimal
numbers of absolute value at most 65,536, or the four observed labels above (zero).
Missing parameters default to zero. Unlike permissive retail atof, junk suffixes,
other words, quotes, excess arguments, nonfinite and excessive values are rejected
explicitly. Rotate/Warble and unknown effect names remain unavailable. Rejection
retains raw metadata and a named error, leaving the authored mapping static.

For velocity V=P*sP+Q*sQ, texel rates are -dot(V,P) and -dot(V,Q), divided by the
resolved image width/height for normalized UV rates. Use the complete basis dot
products, including scale and nonorthogonality; do not assume unit P/Q or multiply
the textual direction into the numeric sign. Finite texel rates are bounded to
one billion per second. Original UVs are computed before world-model pivot removal;
the panning stays in that same local material basis when the object moves/rotates.

## Clock, ownership and bounds

The DAT reader retains the presence flag, exact effect name and parameters for
every surface, including unused/invisible ones. Each string keeps the existing
4,096-byte/NUL/section checks; aggregate retained text is at most 8 MiB across all
models. Existing file/item budgets still apply. Parse failures clear the world.

The mesh owns at most 65,536 effect records, including invisible, hidden, unbound
and unsupported surfaces, and a separate 8 MiB text budget. Each record identifies
its source model/surface and keeps raw parameters, parsed speeds, origin velocity,
texel rates and error. A malformed geometry/budget failure clears the mesh atomically.
Unknown effect metadata alone does not discard valid world geometry.

Batch UV rates belong to surfaces, never to a shared image. Adjacent opaque faces
merge only when their material, blend mode and rates agree. Translucent faces keep
one batch per polygon. Missing materials, sky markers and texture-free diagnostics
remain static; invisibility rules are unchanged. No image or vertex data changes
per frame. World vertices retain their 32-byte layout; batches now occupy 48 bytes
on the tested 64-bit build. Strings, effect records and temporary per-model index
arrays are additional bounded CPU storage. Revision changes release all owned
metadata/geometry/images without retaining object handles.

Both world passes receive `server.world_time()` explicitly. Rendering and inspection
never tick or mutate the effect/game. This const accessor shares the existing
GetWorldTime origin without adding inspector/render service-trace calls. The
underlying object scheduler stays monotone across levels. Waiting, pause or repeated draws at the same
session time preserve the same phase; unloading/reloading resets the world clock.
The CPU computes rate*time in double, checks finite nonnegative time and finite
products, then reduces modulo one before uploading a vec4 offset with each batch.
Negative offsets are valid under the existing repeat sampler. A bad clock/rate
rejects before opening a render pass; no stale uniform may animate a static batch.

This absolute session clock and modulo reduction are deliberate replacements for
retail's per-update float accumulation. They preserve constant-speed mapping while
avoiding large GPU UV offsets; bit-exact long-duration phase, retail visibility
scheduling, interpolation/filtering and retail renderer parity are not claimed.
An unrepresentable product fails instead of submitting invalid coordinates.

## Inspection and checks

The 3D inspector lists every authored effect, source model/surface, raw name and
arguments, unsupported reason, P/Q speeds, origin velocity and texel rates. Prepared
animated batches show exact texture paths, normalized rates and current UV offsets.
Actual opaque/alpha Pan submission counts and their session time are separate;
preparation does not establish on-screen visibility. Hidden UI and inspector-free
builds run the same rendering path.

Synthetic tests cover bounded/truncated metadata, exact name handling, numeric
parameters and zero-valued labels, scaled/skew bases, malformed inputs, aggregate
budgets, independent shared-material batches, hidden/unsupported effects, clock
repeat/reset/wrap and invalid clocks/rates. Private tests exercise actual SubwayA
metadata, session entry and reload. NVIDIA GPU readback checks both scrolling axes,
static neighbors sharing an image, opaque/alpha passes, repeated/restarted phase,
resize/reupload and fixed-camera intro pixels. Application viewport smoke checks
that real session time reaches the shader while inspection is hidden and after
reload. Private scene captures remain outside both repositories.
