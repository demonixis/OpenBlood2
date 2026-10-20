# M2 — timed MIDI delivery on the audio clock

M2 remains **incomplete**. Explicit sample-timed note, program and supported
controller messages now reach the existing DLS receiver at their actual audio
boundary. The inspector shows pending messages, current time and failures, with
a delayed-note diagnostic. Original IMA tick conversion, running sections,
playlists and the required `MUSIC` service remain unfinished.

See [contracts](../../contracts/music_timing_contracts.md) and
[source ledger](../sources/m2_music_timing_sources.json).

## Original behavior

The [isolated executable probe](evidence/m2-midi-queue-probe.py) runs original
MSynth25 sorted insertion, due-note extraction, controller/patch lookup and bank
capture against synthetic queues. It checks 700 note horizons, 1,400 control/patch
queries and 900 bank/program operations/queries. Equal times retain submission
order; deadlines are inclusive; value lookup selects the last due entry. Bank
selectors latch at submission and program messages capture them. The decision
hash is `6ea8812d82b9553a57fac66e5a56a5b4d7ffd27abb05740ed546e056b11f994f`.

Only the external verified PE image is mapped into the optional emulator. A
synthetic bounded free pool replaces allocation, and only the original pool-head
data page is writable. No native DLL entrypoint, operating-system imports, full
synth or tick-to-sample conversion runs. No commercial binary/sample/disassembly
bytes are copied into the repository. The portable scheduler retains these queue
rules while using the existing full-precision replacement voice synthesis.

## Integrated behavior

The existing AudioMixer owns the time progression. Blocks split at actual event
boundaries, including beyond the rendered prefix after a stall. Control/program
values due at a note's timestamp are applied before that note, with duplicate
values coalesced; note order remains stable. Scheduling admission is atomic and
separate from eventual playback success. A delivery failure is consumed once,
remains inspectable and prevents submission of that partially failed PCM block.
World unload drops both live and future bank ownership.

Synthetic tests check exact silence/mute/start/stop boundaries, equal-time and
cross-batch order, captured banks, hidden/fractional clocks, saturation, atomic
failure and eventual release. All 38 Cabal regions produce identical replacement
PCM with one 10,000-frame block or chunks of 137 frames, including timed controls
and note-offs. This compares the replacement against itself under different
update sizes, not against original retail PCM.

The first Debug test link exposed the existing engine/legacy base-class symbol
dependency. The new test directly uses both libraries and now declares `engine`
before `blood2_game`, allowing the static linker to resolve the real original
callbacks. No placeholder callbacks or production behavior changes were added.
The initial link log is retained.

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 137/137 passed | 20.71 s |
| release-tests | 137/137 passed | 1.98 s |
| sanitize-tests | 137/137 passed | 82.85 s |
| debug-gpu | 2/2 passed | 11.02 s |
| release-gpu | 2/2 passed | 5.35 s |

The [validation ledger](evidence/m2-midi-queue.json) records frozen source hashes,
commands and raw logs; the [driver](evidence/m2-midi-queue-validation.py) selects
music, world/object lifetime, audio and configuration suites. Formatting and owned
source/documentation whitespace pass. Existing enum/float warnings in unchanged
PlayerVoice, ItemPickups and PlayerContainers remain visible in broad rebuilds;
modified files have no compiler warnings.

Both GPU runs report NVIDIA GeForce RTX 3060 Laptop Vulkan. The sound test checks
actual sample/SDL output advancement, a silent lead-in, timed mute/restore and
note-off, with the inspector hidden and again after reload. Each pass submits
84 messages (32 bank latches and 52 timed deliveries), then leaves one far-future
note pending for unload; the next world must begin with empty scheduling state.
The Debug capture was reviewed for actual queue/counter display. Release runs
without the inspector. The delayed-note widget itself was not manually clicked.
Dummy audio does not prove listening equivalence; native Metal, Windows and mobile
were not tested in this increment.

All 10,923 reference files and six reference repositories remain unchanged.
Local Markdown links pass. Historical logs/XML preserve their original bytes and
whitespace. The user's root README stays untouched and outside the commits.


Next work is the explicit musical tick-to-sample mapping and a running original
section feeding this queue, followed by playlist/transition semantics and MUSIC
acceptance. Unsupported controllers, resource failures and the unchanged M2 gate
remain visible.

Implementation commit: `603ac0c8c5036a8803a2092beaaeb3e9b7e81178`.
