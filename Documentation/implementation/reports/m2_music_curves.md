# M2 — Original IMA style curve decoding

M2 remains **incomplete**. The music survey now decodes the installed style-grid
curve records and exposes them in the in-game inspector. This closes a data-reader
gap needed before controller automation can join the existing note preparation;
it does not introduce live MUSIC playback.

The original format has separate curve-header and point strides, followed by a
point count for each curve. The portable reader retains signed values, raw unknown
fields, duplicates and source ordering. Pattern/grid/chunk offsets locate every
record. Parsing is atomic, bounded and separate from note-decoding status. The
world catalogue owns immutable results and releases them on unload or replacement.

See the [contracts](../../contracts/music_curve_contracts.md) and
[source ledger](../sources/m2_music_curves_sources.json).

## Original comparison

The bounded [probe](evidence/m2-music-curves-probe.py) executes the hash-pinned
installed IMUSIC25 reader at RVA `8a30` in isolation. Reads, seeks and zeroed
allocation are intercepted. It observes original header copies before derived
summaries overwrite them, then the original ordered point lists. This validates
raw authored values and ordering, not interpolation or playback. No native DLL,
OS callback or synth executes, and no commercial resource bytes are committed.

Across 39 installed styles, all **42 chunks / 47 curves / 47 points** match the
original reader: A1 has 40 curves, Cabal 2 and Track1 5. The decoded-record SHA-256
is `2e676bf6e4d1f8f4dd7a07c013e0349906058a091c601d103a4c1ddfdd2692a5`;
FNV64 is `4777673514653972974`. Three isolated synthetic comparisons cover repeated
and empty curves, multiple points, signed extremes and original extension skipping.
The portable profile deliberately rejects extensions rather than assuming support.

Cabal's two curves are in `cabal5.sty`, on source channel 10, with signed point
start -1 and nontrivial value/target pairs. They remain visible as unprepared
curves in the section cursor; the new reader does not silently treat them as played.
Direct section events and curves are absent from the examined installed sections
and remain outside this decoder's scope.

Additional behavioral checks cover every truncated fixture prefix, invalid strides,
point-count overflow, exact and exceeded budgets, late failure after a valid record,
unknown/nested scope exclusion, preserved invalid semantic fields, independent
style/curve failures, immutable owner release and copied result lifetime.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 120/120 passed | 20.35 s |
| release-tests | 120/120 passed | 1.64 s |
| sanitize-tests | 120/120 passed | 81.15 s |
| debug-gpu | 2/2 passed | 10.90 s |
| release-gpu | 2/2 passed | 5.45 s |

The [validation ledger](evidence/m2-music-curves.json) records commands, source
hashes and raw logs; the [driver](evidence/m2-music-curves-validation.py) selects
music, object/world, audio and configuration suites. All nine changed C++/CMake
hashes match the tested versions; seven C++ files pass formatting. Final builds
emit no warnings. The private reader test checks exact re-encoding of source
records and the original-reader aggregate hash.

Both GPU configurations use NVIDIA GeForce RTX 3060 Laptop Vulkan. Hidden-inspector
and inspector-free Release paths verify the two Cabal curves and their points,
then world teardown/reload. The reviewed Debug capture shows the music summary;
the nested curve rows were not visually exercised. Dummy audio checks existing
output integration, not a listening comparison. Native Metal, Windows and mobile
were not tested.

All 10,923 inventoried reference files and six reference repositories are unchanged.
Local documentation links and source/documentation whitespace checks pass. Raw
logs/XML retain tool-generated trailing whitespace byte for byte. The user's root
README remains untouched.

Controller-curve evaluation and ordering with notes are the next integration step.
Live tempo/activation, section tracks, controller delivery, playlists and synthesis
integration remain unfinished. Required MUSIC keeps the M2 exit gate open.

Implementation commit: `0b15de19e023a3cbf2b8ecb906c436d61b27c4d3` (`Blood2`).
