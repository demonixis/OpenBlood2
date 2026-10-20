# M2 — Original IMA style curve events

M2 remains **incomplete**. Style curve points now expand into inspectable timed
messages using the original arithmetic. A developer can set an explicit caller
tick in the curve inspector and examine the prepared events. This is a prerequisite
for controller automation, not live MUSIC playback.

The implementation preserves original shape/flag behavior, signed 16-bit deltas,
three-tick interior spacing, unchanged-value suppression, endpoint replacement,
point order and the pitch-bend high-byte mask. It regenerates the four lookup
curves mathematically. Bounded atomic results replace the original allocation
failure behavior; overflow/unsupported owners remain explicit errors. Existing
curve decoding, cursor admission and note scheduling are unchanged.

See [contracts](../../contracts/music_curve_event_contracts.md) and
[source ledger](../sources/m2_music_curve_events_sources.json).

## Original comparison

The [bounded probe](evidence/m2-curve-evaluation-probe.py) executes hash-pinned
IMUSIC25 RVA `12168`, including original expansion at `11ca9`, in x86 emulation.
Each point has an explicit owner and caller tick. Allocation/free are intercepted,
every allocated event is accounted for, and sends are captured rather than played.
No native DLL, OS or synth executes. No commercial table, record or sample bytes
are committed. The probe separately compares all **772 regenerated table entries**
with the installed executable.

- **1296 generated cases / 39840 messages**, including all four owner types,
  six shapes, six flag combinations, signed extremes, negative/zero durations,
  constant values and channel/controller combinations. FNV64:
  `11599925503900038861`; SHA-256:
  `7cdeccd1461329d5967a7184adf03ad46ec51c08ed61a5a15fd053f83aaa0148`.
- **47 installed points / 3194 messages** across 39 styles, using explicit owner
  records and caller tick 10000. A1 produces 2900 messages, Cabal 206 and Track1 88.
  FNV64: `12221266369440151417`; SHA-256:
  `9286e869a76daa634fb43a4c7ef6f554284547d2f75834010953a8c30d8f61c1`.

The C++ tests compare against these original-output hashes. Additional checks
exercise invalid owner types/channels, emitted-tick overflow, independent copied
results, zero/exact/exhausted budgets, point/message hard limits, same-time endpoint
replacement and deliberately unsorted multi-point output. The original section
curve evaluator differs and is not covered by this style API. Neither original
loader owner fixups nor active variation/tick selection are claimed by the probe.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 123/123 passed | 18.39 s |
| release-tests | 123/123 passed | 1.67 s |
| sanitize-tests | 123/123 passed | 75.97 s |
| debug-gpu | 2/2 passed | 11.10 s |
| release-gpu | 2/2 passed | 3.73 s |

The [validation ledger](evidence/m2-curve-evaluation.json) records commands, source
hashes and raw logs; the [driver](evidence/m2-curve-evaluation-validation.py) selects
music, object/world, audio and configuration suites. All seven changed C++/CMake
hashes match the tested versions; five C++ files pass formatting. Final builds
emit no warnings. Generated and private event tests use original-output hashes.

Both GPU configurations use NVIDIA GeForce RTX 3060 Laptop Vulkan. Hidden-inspector
and inspector-free Release paths verify the 206 Cabal messages, then world
teardown/reload. The reviewed Debug capture shows the music summary; the nested
event rows were not visually exercised. Dummy audio checks existing output
integration, not a listening comparison. Native Metal, Windows and mobile were
not tested; these Linux results do not establish cross-platform libm equivalence.

All 10,923 inventoried reference files and six reference repositories are unchanged.
Local documentation links and source/documentation whitespace checks pass. Raw
logs/XML retain tool-generated trailing whitespace byte for byte. The user's root
README remains untouched.

The next integration step is original curve admission and its ordering relative
to notes. Controller delivery, active voice updates, transport and playlists still
keep required MUSIC and the actual M2 exit gate incomplete.

Implementation commit: `f37dfb1d58c25673e98d984a9ac102d82585ce0a` (`Blood2`).
