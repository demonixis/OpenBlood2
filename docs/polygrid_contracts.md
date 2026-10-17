# Original PolyGrid surfaces

Status: selected original VolumeBrush / PolyGrid / CPolyGridFX path implemented;
**M2 remains incomplete**. See [evidence](../../Documentation/Implementation/m2_polygrids.md)
and [source ledger](m2_polygrid_sources.json). This extends
[container](container_contracts.md) and [world-view](world_view_contracts.md) contracts.

## Original server and client rules

VolumeBrush reads the authored colors, scale ranges/durations, polygon count,
height, alpha and SPR. Its initial update creates a separate original PolyGrid
publisher at `volume position + (0, half-height - SurfaceHeight/2, 0)`, retaining
rotation. The surface half-extents use the volume X/Z dimensions and SurfaceHeight
as Y. Pan parameters retain `1 + Current.x * .01` and **Current.y**, not Current.z.
ON/OFF/TOGGLE also propagate USRFLG_VISIBLE to a checked surface identity.
ShowSurface=false creates nothing. Invalid parameters or failed resources remain
visible errors, never a successful substitute surface.

PolyGrid retains original CClientSFX ancestry, typed Setup publication, 32767
polygon cap and 5:6:5 color quantization. Runtime children receive Setup after
creation; authored/world and property-string instances read their properties.
Repeated Setup on an already published object is explicitly refused without
changing the existing surface. The original registration's unused zero Dims is
not accepted as drawable geometry. No legacy packet/vtable ABI is emulated.

CPolyGridFX uses the original integer-half polygon-count square root, four-ring
size rounding (2/4/8/16), distance grid, three-period signed sine table, reversed
four-ring X traversal and arithmetic division of negative sums. Colors interpolate
from palette slot 0 toward the exclusive endpoint 256, so slot 255 does not reach
the second endpoint. Normal plasma advances at 50 units/second. Four-ring phases
start at 0/1/4/3 and advance once per updated surface with original ring rates.
They are shared **inside one world/session**, in creation order; a destination
rehearsal cannot modify the source world's phases. Bounded modulo phase storage
replaces out-of-range floating-to-byte conversions. Portable signed conversion
and division avoid platform-dependent char/shift behavior.

Texture scales preserve positive-duration checks, strict endpoint comparisons,
clamp-and-direction reversal and discarded overshoot. Pan values are texture
coordinate offsets; no guessed current-driven texture velocity is introduced.
Hidden surfaces stop plasma/scale/SPR advancement and resume with the new frame
delta, without a hidden-time catch-up burst. Sprite resources use existing bounded
SPR/DTX readers and exact normalized paths.

## Explicit engine/presentation policies

High detail and the original `always update` option are selected explicitly;
the unavailable retail WASDRAWN visibility feedback does not drive simulation.
No low/medium-detail mode is claimed. SPR frame advancement shares that active
simulation clock. These timing policies are deterministic adaptations, not a
verified retail render-culling or animated-texture timing match.

The original VolumeBrush destructor does not remove its surface. Removing a volume
therefore leaves the independent publisher/client surface alive until that publisher
is removed or the world unloads. Pending/removed publishers disappear from rendering
immediately; the next effect update removes their owned client data. IDs do not reuse
across unload. World clear resets phase/history/resource ownership. Static containers
still reject movement: original polling for moving volumes is not silently enabled.

PolyGrid publishers are visual service objects, explicitly excluded from the
replacement engine's generic body integration and volume-damage polling. Otherwise
that broad OT_NORMAL scan would give water current to the surface itself and add
unintended physics callbacks. This is a scoped replacement-engine policy, not a claim
that the later-engine physics implementation exactly describes Blood II. Ordinary
bodies, players, volume queries and deferred callback ordering remain unchanged.

The read-only PolyGridView builds the XZ lattice, height/127 displacement and
height+128 palette lookup. Existing SDL_GPU world shaders supply repeated textures,
original effect alpha, two-sided cells, shared world/model depth tests and no depth
writes. Cells sort far-to-near; the surface pass runs after world/model alpha and
before other client FX, whose sorting remains separate. Later LithTech rendering
corroborates these UV/height conventions, not 1998 pixel parity. No reflection,
refraction, environment map, underwater fog or lighting parity is claimed.

Clocks live outside the inspector. Both Vulkan/SPIR-V and Metal/MSL use the existing
unchanged bundled shaders. NVIDIA execution is verified; this increment has no
native Apple automated test. A world revision clears CPU/GPU surface material
caches. Invalid/missing textures remain named inspection failures and draw no
surface. No raw game/object pointer survives a presentation frame.

Limits: 128 live surfaces, 128 retained reports, up to 128x128 samples for normal
plasma, four-ring grids at most 16x16, 1,000,000 prepared vertices, 4096 nonwhite
materials and 128 MiB decoded images. Counts, finite transforms, colors, scales,
alpha and clocks are validated before indexing or drawing. Update deltas above
10 seconds fail explicitly; normal application deltas retain their 100-ms cap.

## Inspection and remaining work

Containers lists the source volume, independent publisher, SPR, position, dimensions,
alpha, grid size, frame, scale, update count and prepared triangles. Its expanded
PolyGrid list includes hidden/standalone surfaces and retained publication/resource
failures. Previous GPU cell counts are labelled separately from prepared geometry.

CityHub Canal1 and Pool each produce an 8x8 grid / 98 triangles using the external
`Textures\Reds\waterbrown.dtx`. Their centers are (0,135,-704) and (3488,399,2208),
respectively; water collision/swimming still uses the closed volume below. Original
source-entry accounting is unchanged; these are two additional runtime children.
Save/restore of surfaces, moving volumes, underwater presentation, remaining required
services and actual M2 acceptance remain open.
