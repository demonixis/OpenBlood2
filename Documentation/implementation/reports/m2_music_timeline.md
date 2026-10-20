# M2 — Original style meter and section timeline

M2 remains **incomplete**. Music sections now bind to decoded styles and expose
the original meter reconciliation, relocated commands/chords and beat-sampled
mutes in the inspector. This is a sequencing prerequisite, not soundtrack
playback; the reached `MUSIC` service still reports unsupported.

## Evidence and behavior

The installed `IMUSIC25.DLL` copies six signed style-header words into its runtime
style (`0xeb4c..0xeb9c`). Reconciliation at `0x19441` recomputes beat ticks as
`768 / beat_unit` (zero means four), then measure ticks and section duration.
It resamples mute records (`0x194e3`), recalculates chord times (`0x19589`) and
relocates commands from their signed stored measure indices (`0x19620`).

The initial mute value comes from the first record even if it lies in the future.
Input is consumed in authored order; duplicate/out-of-order records are not
sorted. Each output beat retains the last consumed value. Chords outside the
upper beat boundary are dropped, while negative beats survive. Command/chord
times preserve file order and can lie before zero or beyond the section end.
The original chord harmonizer runs next; its pitch work is not part of this
increment. [Contracts](../../contracts/music_timeline_contracts.md)
spell out these rules, unsafe-meter refusals and bounded ownership.

The [isolated original-code probe](evidence/m2-music-timeline-probe.py) checks
seven synthetic cases (six timelines plus extreme signed style-header fields),
the six numeric fields of all **39 installed style headers**, and all **132
sections with a matching style**.
Its original-generated timeline contains **132 commands, 3,030 mute samples and
132 chords**. The independent C++ private test matches its canonical digest:
FNV64 `4011438205292846879`, SHA256
`d051b363a130222fe560a1a9d95bfc198ca17e58bf70df6be841662260bfe356`.
[Numeric results](evidence/m2-music-timeline-probe.json) retain only synthetic
outputs, resource paths, counts and hashes, not commercial event payloads.

The hash-pinned original DLL runs only inside bounded x86 emulation. Allocation
and free are intercepted with bounded memory; chord harmonization is explicitly
intercepted and excluded. No native DLL initialization, imported OS call or
external I/O runs in the emulator. Unicorn remains optional evidence tooling,
not a project dependency. No DLL, disassembly or commercial assets are imported.

## Integration and limits

The catalogue binds the exact same-directory, case-normalized style filename,
then checks a nonzero section identifier against the style identifier. All 132
installed filename matches also match identifiers. **24 Track1 sections reference
absent Track1-a/b/c/d style files** and remain unavailable. This strict profile
is not the original general UUID resolver; no aliases or global search were added.

The catalogue prepares immutable timelines once after decoding all requests,
with 8,192 expanded records per timeline and 32,768 per world survey. Resource
failure, decode failure and timeline failure stay separate. Input records stay
unchanged; unload releases the timeline with its section. The inspector exposes
authored and reconciled times, source indices, dropped chord beats and failures.
Hidden/disabled inspection does not change preparation or audio behavior.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 83/83 passed | 15.63 s |
| release | 83/83 passed | 1.40 s |
| sanitize | 83/83 passed | 64.54 s |
| debug NVIDIA Vulkan | 2/2 passed | 11.33 s |
| release NVIDIA Vulkan | 2/2 passed | 5.36 s |

The [validation ledger](evidence/m2-music-timeline.json) records commands and raw
build/test output. The 83 cases cover timeline/style/section/band readers,
DLS decoding/synthesis, music catalogue lifetime, object/world lifecycles,
world audio/streaming and configuration. Clang ASan/UBSan enables leak checks.
NVIDIA device identity was verified in both GPU tests for each build; Release
runs without the inspector. Eleven changed C++ files pass formatting;
10,923 reference files and six repositories remain unchanged. Local Markdown
links were checked. The sound capture shows music status; nested timeline rows
were not captured. Dummy audio verifies submission rather than listening.
Native Metal, Windows and mobile execution were not tested.


Synthetic tests cover the zero-unit default, nondivisor integer beat units,
signed command division/wrap, negative and beyond-end chord times, discarded
beats, future/duplicate/unsorted mutes, strict end boundaries and empty mutes.
Malformed tests cover unsafe meters, lengths, divisors, overflow, raw/expanded
budgets and atomic output clearing. Catalogue tests exercise missing/undecoded
styles, identity mismatch, zero-identity binding, aggregate expansion limits,
refresh and weak-pointer expiry. Private SubwayA/CityHub checks retain immutable
timelines across hidden steps and release them on unload. The GPU sound smoke
checks loaded Cabal timelines while hidden and after reload.

Remaining music work includes effective bands, pattern/variation selection,
chord/personality pitch processing, curves and actual sequence/playlist playback.
The missing Track1 styles and invalid effective DLS loops remain explicit.
Whole-world/dead-player restoration and other required services still keep the
actual M2 gate open. The user's README edits are preserved.

Implementation commit: `500fe12b948eca0cab64da4962fecd8a2e328a19`.
