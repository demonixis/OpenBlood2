# M2 — DLS live voice controls

M2 remains **incomplete**. The sampler now applies volume, expression, pan and
pitch bend to existing checked voices while preserving their current sample,
loop, envelope and release state. The in-game inspector exposes current values,
manual adjustment and separate control failures. This prepares the synthesis side
for musical curves; timed channel dispatch and MUSIC playback remain unavailable.

See [contracts](../../contracts/music_live_control_contracts.md) and
[source ledger](../sources/m2_music_live_controls_sources.json).

## Original comparison

The [isolated executable probe](evidence/m2-live-controls-probe.py) reads the
installed MSynth25 binary and executes its controller dispatch, pitch default
initialization and signed pitch calculation. Queue insertion is intercepted;
no native DLL entrypoint, operating-system import or synthesis executes.
645 decoded controller cases and all 32768 representable raw bend values match.
The pitch-only FNV64 is `10930475445258237893`; combined SHA-256 is
`a6d7e0d61c5774764656402d8fa4c8ff8eda73bc58fe2886e3fecec0e85f9847`.

The raw-byte behavior matters for previously prepared IMA curves: pitch bytes are
combined without masking to seven bits. The original 200-cent range uses signed
floor division, including -1 cent immediately below center and +199 at the normal
MIDI maximum. RPN/tuning changes remain unsupported. Out-of-range CC gain/pan
values fail safely instead of reproducing the old table's unsafe indexing.

The [DLS Level 1 specification](https://midi.org/dls-level-1) remains the source for
the existing squared volume/expression and equal-power pan equations. The new
live updates recompute gain from its immutable base and retain the selected
constant-pan articulation. These are replacement synthesis equations; no retail
PCM equivalence or gain/pan-table quantization is claimed.

## Behavioral validation

Tests check exact synthetic PCM after mute/unmute, changing pan and expression,
pitch step changes, 100 repeated mute/restore cycles, continuing attack/release
clocks, independent voices, immutable note-on controller snapshots and stale IDs.
Invalid controls and the total pitch budget fail without changing a voice.
The private variant applies explicit synthetic control sequences to all 38 Cabal
regions; it checks finite PCM, rendered/hidden clock equivalence and cleanup.
Those probes do not claim authored song playback or listening equivalence.

The first targeted run exposed an incorrect test assumption about how quickly a
peer voice completes release during attack. Release starts from the logarithmic
envelope level, not the linear amplitude fraction. The test now advances the full
configured half-second release before checking cleanup; production envelope logic
was unchanged. The corrected targeted sampler suite passes 10/10.

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 129/129 passed | 19.69 s |
| release-tests | 129/129 passed | 1.77 s |
| sanitize-tests | 129/129 passed | 80.48 s |
| debug-gpu | 2/2 passed | 11.52 s |
| release-gpu | 2/2 passed | 5.30 s |

The [validation ledger](evidence/m2-live-controls.json) records commands, frozen
source hashes and raw logs. The [driver](evidence/m2-live-controls-validation.py)
selects music, world/object, audio and configuration suites. Six changed C++/CMake
hashes match the tested versions; five C++ files pass formatting. Modified code
emits no warnings. The broader Release/Clang rebuilds each expose five existing
enum/float comparison warnings in unchanged PlayerVoice, ItemPickups and
PlayerContainers sources; their contents match the implementation parent.
Private controls make 444 successful updates across 38 regions.

Both GPU configurations use NVIDIA GeForce RTX 3060 Laptop Vulkan. The smoke
applies messages to a routed voice, advances the actual sample/output clock,
restores controls and releases voices with hidden inspection and after reload.
Release builds without the inspector. The reviewed Debug capture shows control
counters and the music summary; transient voice sliders were not visually
exercised. Dummy audio is not a listening comparison. Native Metal, Windows and
mobile were not tested in this increment.

All 10,923 inventoried reference files and six reference repositories remain
unchanged. Local Markdown links and owned source/documentation whitespace checks
pass; raw logs/XML retain tool-generated whitespace byte for byte. The user's
root README is unchanged and excluded from these commits.


Remaining work includes channel-to-voice assignment, timed controller/note
application, exclusivity, section/playlist transport and actual MUSIC service
acceptance. The M2 exit gate is unchanged.

Implementation commit: `cbe97b7af9ea75cfd3693ec72ed1d8417fe9b835`.
