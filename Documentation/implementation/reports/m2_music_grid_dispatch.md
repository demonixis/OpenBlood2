# M2 — Original ordered note and curve dispatch

M2 remains **incomplete**. The world-owned music cursor now selects curve records
using the original mute/variation tests and prepares their messages interleaved
with note-ons in source chunk order. Due stops follow the grid sequence. The
inspector exposes that order and separate curve filter/message counts. Actual
controller delivery and live MUSIC playback remain unfinished.

The catalogue requires curve data from the section's exact validated style.
Preparation is atomic with existing cursor/active-note state; missing decoding,
inconsistent ranges and event-budget exhaustion preserve the previous state.
The older note-only diagnostic is explicitly distinguishable and remains usable.

See [contracts](../../contracts/music_grid_dispatch_contracts.md) and
[source ledger](../sources/m2_music_grid_dispatch_sources.json).

## Original comparison

Read-only grid-loader analysis shows original note/curve lists appended in file
order. The [bounded executable probe](evidence/m2-grid-dispatch-probe.py) executes
IMUSIC25's grid loop at RVA `17103..1759f`, including actual note-event tail and
curve expansion/send routines. It supplies explicit resolved pitches, variations,
point owners and ticks from the already tested preparation paths. Random returns,
allocation/free and sends are intercepted; no native DLL, OS or synth executes.

The transient exported fixtures contain installed content and stay outside Git.
The committed probe contains no commercial bytes; only aggregate results and
hashes are recorded:

| Input | Grid visits | Ordered note-on/curve messages | Curve admitted / muted / excluded |
| --- | --- | --- | --- |
| 64 generated traversals | 1024 | 4348 | 336 / 1280 / 432 |
| 132 installed sections | 8174 | 9063 | 42 / 0 / 2 |

Generated FNV64 is `15334764557161491361`, SHA-256
`56091cd26e828656bee948a47152d59e9584d99e78a26f362b00eca0afa2f62e`.
Private FNV64 is `10183239741431151107`, SHA-256
`ef746052f946f07bbe678fec188271810e5b4189e095d9512a8cf03a86eb1dd9`.
The 24 Track1 sections without their referenced styles remain explicitly unbound.

The C++ suite pins these original-output hashes. Generated grids place a curve
chunk between two note chunks; differing channels, variations, shapes, flags and
note randomness exercise filtering and ordering. Additional checks cover malformed
ordinals/payload sizes/channels/types, late failure preserving active notes and
prior inspection, the combined message cap, missing catalogue curve data, switching
back to note-only inspection and unload. Due-stop suffixes and message conservation
are checked during every traversal. The executable probe stops before that phase;
its original ordering is covered by the earlier active-note evidence.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 126/126 passed | 21.06 s |
| release-tests | 126/126 passed | 1.71 s |
| sanitize-tests | 126/126 passed | 84.65 s |
| debug-gpu | 2/2 passed | 10.86 s |
| release-gpu | 2/2 passed | 3.71 s |

The [validation ledger](evidence/m2-grid-dispatch.json) records commands, source
hashes and raw logs; the [driver](evidence/m2-grid-dispatch-validation.py) selects
music, object/world, audio and configuration suites. All seven changed C++/CMake
hashes match the tested versions; six C++ files pass formatting. Final builds
emit no warnings. Generated and private dispatch tests use original-output hashes. Private fixtures
remain outside Git; the export log records aggregate counts only.

Both GPU configurations use NVIDIA GeForce RTX 3060 Laptop Vulkan. Hidden-inspector
and inspector-free Release paths verify merged-dispatch accounting for the first
eight steps per section, the separate 206 Cabal curve messages, and world
teardown/reload. Headless private tests traverse all 132 bound sections. The
reviewed Debug capture shows the music summary; nested dispatch rows were not
visually exercised. Dummy audio checks existing output
integration, not a listening comparison. Native Metal, Windows and mobile were
not tested; these Linux results do not establish cross-platform equivalence.

All 10,923 inventoried reference files and six reference repositories are unchanged.
Local documentation links and source/documentation whitespace checks pass. Raw
logs/XML retain tool-generated trailing whitespace byte for byte. The user's root
README remains untouched.


Live controller application, voice updates, tempo/section activation and playlist
transport remain. This prepared dispatch does not satisfy required MUSIC or close
the actual M2 exit gate.

Implementation commit: `5c825dc1f8897e44ea1dc0d1cca12fb4bdad1acf`.
