# M2 — Legacy DLS constant-pan compatibility

M2 remains **incomplete**. The five Cabal regions previously rejected by the
diagnostic sampler use an older pan representation understood by the installed
Microsoft synth. The portable sampler now accepts that representation with
explicit bounds and inspection. This does not implement original `MUSIC` playback.

## Evidence and interpretation

The installed, external `MSynth25.dll` contains a source-zero articulation branch
for destination 2 alongside the standard destination 4 branch. Both write the
same internal pan field. Destination 2 takes signed integer units, while
destination 4 takes signed 16.16 units. Connections are processed in file order;
the later pan record replaces the earlier value, including when they disagree.

Read-only static inspection was checked by executing only this original dispatch
in a bounded x86 emulator, with synthetic connection data. Thirteen signed values
produce identical internal pan values in both representations. Three mixed-form
cases verify overwrite order and absence of double application. The input DLL's
SHA-256 is pinned; no initialization routine, imported OS function or native DLL
is invoked. The emulator stops after the connection loop, with both instruction
and time limits. It is an optional evidence tool, not a game dependency.

The [probe source](evidence/m2-music-legacy-pan-probe.py),
[numeric observations](evidence/m2-music-legacy-pan-probe.json) and
[source ledger](../sources/m2_music_legacy_pan_sources.json)
record the input hash and addresses. No DLL, machine-code extract, disassembly,
PCM or commercial asset is committed.

The original dispatch quantizes offsets internally to steps of 1/128. The portable
sampler deliberately retains its existing full-precision DLS square-root pan law,
using the newly identified integer input units. This establishes format/ordering
compatibility, **not identical stereo samples or complete retail synthesis**.
An integer legacy value of 6 means 0.6 percentage points of constant pan, not
6/65536 units and not an additional offset on top of a later standard pan record.

## Implementation and inspection

The selected articulation graph accepts destination 2 only with zero source,
control and transform, and a value in -500..500. Standard destination 4 retains
its fixed-point interpretation. Each record is checked before assignment, so a
later valid record cannot hide an earlier invalid one. Other unknown destinations,
unsupported modulation and duplicate identical edges remain failures. Raw bank
data is never rewritten or filtered.

Each active diagnostic voice exposes its effective constant offset, last-writing
destination, presence of a legacy record and left/right gains. Raw graph inspection
labels the old integer representation. Existing region/global precedence, sample
ownership, shared audio clock, release and unload behavior remain unchanged.
See [sampler contracts](../../contracts/music_sampler_contracts.md).

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 82/82 passed | 90.47 s |
| release | 82/82 passed | 14.14 s |
| sanitize | 82/82 passed | 499.34 s |
| debug NVIDIA Vulkan | 3/3 passed | 41.00 s |
| release NVIDIA Vulkan | 3/3 passed | 11.55 s |

Final build and test results are recorded in
[the validation ledger](evidence/m2-music-legacy-pan.json).

The new synthetic case checks signed output gains for both representations,
paired aliases in both orders, unequal pairs, standard fractional pan, boundary
and integer-extreme refusals, wrong source/control/transform, duplicate/unknown
edges, failure atomicity and local graph precedence. The private Cabal check
requires all 38 region midpoint-key/velocity-100 notes to produce finite nonzero
PCM, including the five formerly refused regions. Hidden-output clocks and final
release cleanup are still checked. No unsupported-service assertion is removed.

The NVIDIA sound smoke explicitly auditions Cabal instrument index 6 / region 4,
whose integer pan 6 is followed by the equivalent standard record. It checks the
effective state, actual shared SDL stream submission and release, while the
inspector is hidden and after world reload. Dummy audio verifies generated/submitted
PCM, not human listening. Inspector-free Release and sanitizer checks are separate;
native macOS/Windows/mobile execution is not inferred from these Linux results.

## Remaining work

Original AASE/AASY event, style and personality semantics, MIDI controller/channel
state, note exclusivity and playlist timing/transitions remain unavailable.
The music command still reports unsupported. Other old articulation destinations
are not assigned meanings from this pan result. Crudux/Track1 effective-loop
errors remain explicit, and whole-world restoration and other required services
still keep M2 open. The user's workspace README edit is preserved.

Implementation commit: `980c842858f493b2312cba08374c21d08434d9a1`.
