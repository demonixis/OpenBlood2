# M2 — Original IMA pattern-note records

M2 remains **incomplete**. The portable project now decodes the original IMA 2.5
style patterns and their note records, with in-game inspection and world-owned
lifetime. This prepares sequencing; it does not play the original soundtrack or
make the reached `MUSIC LEVEL 3` command succeed.

## Original evidence

The installed external `IMUSIC25.DLL` identifies `RIFF/AASY`, `RIFF/AAPT`,
`LIST/AACL`, `patt`, `clik` and `note` in its loading path. The note reader at RVA
`0x8d5f` reads a 16-bit record stride, copies eighteen fields/bytes to an owned
linked record, and preserves order. Its caller at `0x8900` appends successive
note chunks to the grid. Pattern loading at `0x85ab` links those grids and copies
the pattern prefix. This is the older IMA format, not a modern DirectMusic struct.

The [isolated probe](evidence/m2-music-notes-probe.py) executes that original note
reader in an x86 emulator, intercepting only bounded memory reads, seeks and
zeroed allocation. It verifies distinct synthetic fields, negative words, empty
chunks and the old loader's extension skipping. Every installed style note chunk
is then compared against the original output, in order. The original DLL is
hash-pinned; no native DLL initialization, OS import or external process is run.
Unicorn is an optional evidence tool, never a project dependency.

[Numeric results](evidence/m2-music-notes-probe.json): **2,836 note chunks and
7,030 notes** match exactly. Across the 39 installed styles there are **131
patterns, 7,166 grids and 42 opaque curve chunks**. Note totals by directory are
A1 2,661; Bloat 894; Cabal 3,049; Crudux 237; Track1 189. This is loader equivalence,
not original musical performance. No original binary, disassembly, note payload,
PCM or commercial resource is committed.

Read-only inspection of the consumer at RVAs `0x55d20`–`0x56167` and
`0x57103`–`0x57595` establishes variation filtering, signed offset/duration,
stored MIDI/encoded pitch and humanization fields. Pitch mode, chord context and
band mapping affect the emitted result. The portable reader consequently
preserves these inputs without pretending that they are final MIDI events.
The four pattern timing fields remain raw; no seconds/tempo interpretation is
claimed. Unidentified byte 7 and header tails stay explicitly unresolved.

## Implementation

[Contracts](../../contracts/music_note_contracts.md) describe the
admitted layouts and limits. The independent C++ reader requires complete record
boundaries and owned output; an unsupported header/stride or malformed range
fails atomically. It preserves signed values, empty grids, duplicate names and
record order. The original short-record uninitialized-stack behavior is not
replicated. Longer note extensions remain an explicit unsupported profile despite
the old reader's skip mechanism, since no installed example exercises them.

The existing survey attaches immutable decoded styles without rescanning during
simulation or UI drawing. A separate decode error remains visible even when the
RIFF structure is valid. The World sounds panel displays each pattern/grid and
all stored note fields, with separate counts for undecoded curves. There is no
new music scheduler, RNG, channel state or audio submission in this increment.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 71/71 passed | 14.78 s |
| release | 71/71 passed | 1.38 s |
| sanitize | 71/71 passed | 62.49 s |
| debug NVIDIA Vulkan | 2/2 passed | 11.66 s |
| release NVIDIA Vulkan | 2/2 passed | 5.33 s |

Commands and raw build/test results are recorded in the
[validation ledger](evidence/m2-music-notes.json). The 71 selected cases cover
music readers/sampler, configuration, object lifetime, world loading/transitions,
world sounds and streaming. The NVIDIA device was verified in each GPU log;
Release runs without the inspector, and Clang ASan/UBSan runs enable leak checks.
Eight changed C++ files pass formatting; 10,923 reference files and six reference
repositories remain unchanged. The reviewed sound capture shows truthful music
status; it does not show the nested note rows. Dummy audio checks submission,
not listening; native Metal/Windows/mobile execution was not tested.
 Synthetic cases exercise field preservation,
negative offsets/durations, all-byte extremes, duplicate names, empty grids,
unknown nesting, truncated inputs, invalid strides/headers and bounded failures.
Private checks re-encode every decoded field and compare all original note
payloads, then exercise session entry, hidden frame stepping, fresh destination
loading and weak-pointer expiry at unload. NVIDIA sound smoke checks the decoded
Cabal notes with hidden inspection and after world reload, alongside the existing
real DLS audition/SDL submission checks.

## Remaining work

Decode section commands/chords/mutes, band routing, pattern selection, curves,
variation and pitch/personality behavior; connect verified timing and original
playlist transitions to the sampler. Existing DLS effective-loop failures in
Crudux/Track1 remain unchanged. Required services, whole-world restoration and the
actual M2 acceptance gate remain open. The user's workspace README is preserved.

Implementation commit: `43246c39bc5e9b592ff142316050c250f8e2ee07`.
