# M2 — original IMA tempo clock connected to audio

M2 remains **incomplete**. Explicit musical tick messages now reach the existing
sample queue using the installed IMA runtime's tempo rules. Section inspection
shows effective tempo/duration, and a selected-band diagnostic uses that tempo
for a delayed quarter note. Running original sections, playlist transitions and
the required MUSIC service remain unfinished.

See [contracts](../../contracts/music_clock_contracts.md) and
[source ledger](../sources/m2_music_clock_sources.json).

Implementation commit: `457b0c5369ed25ca1e9baa6e1f37c44d175b792a`.

## Source evidence and implementation

Tracing the installed libraries distinguishes three timing paths: IMUSIC25's
MIDI-export calculation, IMRT25's live musical clock, and MSynth25's
millisecond-to-sample conversion. The new code uses the second path, preserving
10..350 BPM clamping, 1/16 BPM quantization, retained 100-nanosecond tempo-boundary
fractions, and rounded millisecond event dates. The existing 48 kHz output is
an explicit adaptation. Equal-tick changes use the last submitted tempo.

The [isolated probe](evidence/m2-ima-clock-probe.py) executes the verified external
IMRT25 constructor (`29b2`), chained time accumulation (`2a3e`) and event lookup
(`1c12`). Synthetic tests include fractional thresholds, tempo clamps, boundary
carry and duplicate dates; all 156 installed sections also participate. The probe
checks 324 maps and 98136 queries; decision SHA-256 is
`5478b171fae6a18e0854a0938212fac5a76af37638bbd607dbf64dfbeef0c69d`.
C++ fixtures independently reproduce the probe's synthetic/private FNV digests.

The PE is read-only in an isolated emulator, with synthetic object/stack memory.
Locks/logging are bypassed and MulDiv is represented by its signed nearest-rounding
contract. Its native Windows implementation is not tested. No DLL is loaded into
the application and no commercial code, disassembly, samples or content is copied.

`ImaClock` is a small value object, with bounded sorted tempo boundaries and
binary-search queries. `enqueue_ima` converts the entire batch before the existing
atomic queue admission, leaving bank capture and note/controller ordering intact.
Invalid ticks, unsupported negative dates, oversized batches and clock overflow
fail explicitly. This work does not advance a section cursor or convert pending
notes again after a later tempo change.

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 142/142 passed | 21.24 s |
| release-tests | 142/142 passed | 1.90 s |
| sanitize-tests | 142/142 passed | 82.69 s |
| debug-gpu | 2/2 passed | 12.41 s |
| release-gpu | 2/2 passed | 5.35 s |

The [validation ledger](evidence/m2-ima-clock.json) records frozen source hashes,
commands and raw logs. Relevant music, audio, world/object lifetime and configuration
suites pass in Debug, inspector-free Release and Clang ASan/UBSan. Formatting and
owned source/documentation whitespace checks pass. Existing enum/float warnings
in untouched PlayerVoice, ItemPickups and PlayerContainers remain visible in
rebuild logs; modified files introduce no compiler warnings.

Both GPU runs report NVIDIA GeForce RTX 3060 Laptop Vulkan. The sound smoke
converts a real selected section tempo and band setup, schedules a synthetic
four-event phrase, and checks silence/start/mute/restore/stop on actual audio
sample boundaries with the inspector hidden and again after reload. It submits
84 batch messages (32 bank latches, 52 timed deliveries), then leaves one future
note for unload; reload must begin with no queued messages. The Debug capture
was inspected for actual queue/counters. The new quarter-note button and expanded
section timing panel were not manually exercised. Dummy audio does not establish
listening parity; native Metal, Windows and mobile were not tested in this run.

All 10923 reference files and six reference repositories remain unchanged.
Local Markdown links pass. Historical evidence preserves its original bytes.
The user's root README stays untouched and outside the commits.


Next is the running section cursor feeding this clock/queue, including lookahead,
active-note tails and transition lifetime, then original playlist/MUSIC activation.
The unsupported required MUSIC calls continue to keep the unchanged M2 gate open.
