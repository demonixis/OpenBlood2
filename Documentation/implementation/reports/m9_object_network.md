# M9 — Network owner-bound ObjectFX

Protocol **23** now delivers the existing 25 owner-bound particle recipes through
reliable object metadata and initial/late baselines. Independent clients receive
projectile trails, finite spells and corpse effects without reading server pools.
A recipe list preserves multiple simultaneous publications on one object: original
Voodoo can publish two effects, shield casts can repeat, and Death Ray publishes
paired rings. A single optional recipe would lose these source behaviors.

Each publication has a monotonic 64-bit ID, creation clock, type, scale, offset and
bounded source name. World and object generations come from its enclosing frame.
The receiver rejects duplicate IDs, payload replacement, owner migration and replay
of retired IDs before committing assets/collision. Both wire and merged worlds
retain a global 128-recipe limit and the existing 1 MiB snapshot bound. Validation
uses a fixed 128-entry ID workspace; no unbounded retired-identity history grows.

ServerSession captures recipes before world initialization, checks the exact required
texture index like the original local runtime, and records values without initializing
particles or consuming cosmetic random numbers. Finite recipes leave the registry
only when original float elapsed time strictly exceeds duration; dead owners and
world rotation retire their recipes. Long-lived trails remain available to late joins.

The independent NetworkEffects runtime starts each ID once, including failed or
already-expired starts. It preserves finite authored age without replaying historical
particles, starts untimed trails fresh, and keeps original first-update/emission
rules and multiplayer stop/resume. Its monotonic particle clock follows presentation
time and accepts newly created owners ahead of interpolation. Existing particles
finish under their own source duration after metadata expires; checked owner removal
ends them immediately. Multiple IDs interleaved across object slots are processed
before advancing the publication high-water mark. Solo producers and update order,
CMoveMgr, logical audio and original game RNG remain unchanged.

## Evidence

The [source ledger](../sources/m9_object_network_sources.json) records the original
publish/receive/recipe references and implementation hashes. The
[validation manifest](evidence/m9_object_checks_2026-10-31.json) lists exact test
cases, raw logs, artifact hashes, independent linkage and preservation results.

Tests compare all 25 pure recipe descriptors against the original runtime, mutate
12,500 encoded frames and reject every truncated prefix. They exercise endian
encoding, invalid settings/timestamps, global capacity/duplicates, immutable
membership, retired IDs, delayed presentation and world/object generations.
Independent runtime tests cover same-owner spells, finite age/no late restart,
metadata retirement, untimed trails, stop/resume and world cleanup. Server tests
publish all recipes and repeated spells, compare late baselines, advance finite
expiry, fill the global limit, disconnect owners and rotate worlds. Server cosmetic
pools stay empty; logical voice counts and subsequent game RNG draws match control.

Dedicated and graphical-host Napalm process tests additionally require decoded
recipe-2 trails, actual emitted particles, checked initial owner identity and clean
teardown alongside their existing lethal combat, explosion, respawn and audio checks.
A synthetic decoded baseline with paired Death Ray rings, shield and trail feeds
real external textures into independent Original/Enhanced GPU rendering.

Final results: **164 native tests, 57 ASan/UBSan tests and
157 inspector-free Release tests pass** (unique names per configuration).
Native client/dedicated, sanitizer dedicated and Release client targets build. All
five fresh solo traces match the previous lot byte for byte; all **6,952
protected hashes** remain unchanged. Independent ObjectFX test binaries contain
no server adapter/session/player/weapon/inventory symbols.

The native GPU was **NVIDIA GeForce RTX 3060 Laptop, Vulkan**: the 640×360 decoded
scene changes **1,693 pixels**, and Original → Enhanced → Original is stable.
Both captures were visually reviewed. Dedicated Napalm clients each observed two
trail owners with a 502-particle peak; graphical-host clients each observed four
with a 725-particle peak. The known absent retail `gib_impact0.wav` remains explicitly
accounted in process logs; no substitute or suppression is added.

Retained initial failures: a test called an overloaded get with an ambiguous brace
initializer; its explicit ObjectId fixes compilation. The first scene fixture ran
before its chosen recipes' emission deadlines; it now uses an established world
clock, retaining the original absolute first-emission behavior. An initial process
assertion compared FX owners against newly received membership instead of the
preceding displayed frame used by the effect update; the corrected check recognizes
that ordering. Original production clocks were not changed to satisfy these tests.

The first sanitizer regression run passed 53/54 cases: the existing stream-process
observer asserted at its six-second minimum before the slower visitor completed
content preparation and its weapon/zoom sequence (391 discarded server ticks under
concurrent build load). The harness now waits for the actual arrival and departure
within its unchanged 18-second deadline, retaining all membership/command checks.
The shared admission/movement/fire/stream process scenarios are rerun after this
fix; the initial failed run remains in the raw evidence. Production deadlines and
server catch-up bounds are unchanged.

No renderer/shader/PBR assets or immutable captures are changed. Remaining network
PolyGrid/WarpGate/light families, presentation/UI and final server-library extraction
stay in M9. This lot does not qualify human LAN, cross-platform play, 16-client load,
all-map combat or impaired multiplayer. M2-GPU-001 and M3 acceptance stay independent.
