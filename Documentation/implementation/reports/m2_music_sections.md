# M2 — Original IMA sections and authored timing

M2 remains **incomplete**. The section data needed to connect the original music
sequencer now has a bounded reader, checked authored timing and in-game inspection.
The reader exposes commands, mutes and chord inputs; it does not execute them or
make `MUSIC LEVEL 3` play a soundtrack.

## Original evidence

The external installed `IMUSIC25.DLL` section loader (`0xb2d0`) reads a 120-byte
`secn` header. Its numeric-copy body and measure-count multiplication establish
the stored tempo/meter fields and authored length. The original command reader
(`0xba4b`), mute reader (`0xbb79`) and chord reader (`0xb899`) establish distinct
8/8/117-byte installed record profiles. Their two-byte prefixes are strides,
not record counts. File order and signed values are preserved.

The command loader's caller divides signed time by header ticks-per-measure and
stores only the low 16 bits (`0xb417..0xb429`). The original MIDI exporter computes
microseconds per quarter with fractional tempo quantized to 1/64 BPM
(`0x18ced..0x18d10`). These formulas are reproduced with checked positive divisors.
The later style reconciliation (`0x19441`) can replace meter and rebuild tracks:
**authored header timing is not the effective playback timeline**. No guessed
pattern-intensity or mute-channel interpretation is introduced.

The [isolated probe](evidence/m2-music-sections-probe.py) checks **31 synthetic
cases**: command/mute record order and extension handling, all low chord-flag
combinations, signed fields, fractional/extreme tempo and command-measure
truncation/wrap. It also checks all **156 installed sections**, including header
copies and the mapped fields of **156 commands, 156 chords and 3,290 mute records**.
The portable private-data test matches the independently observed event digest.
[Numeric evidence](evidence/m2-music-sections-probe.json) contains counts/hashes,
not extracted commercial event payloads.

The original DLL is hash-pinned and mapped into bounded x86 emulation. Intercepts
supply bounded reads/seeks/allocation, ASCII name conversion and a discarded debug
trace. No entrypoint, native DLL initialization, imported OS call or external I/O
executes in the emulator. Unicode/name conversion and personality resolution are
not certified by this probe. Unicorn is optional evidence tooling, not a project
dependency. No original binary, disassembly or commercial assets are committed.

## Implementation and inspection

[Contracts](../../contracts/music_section_contracts.md) describe every
field, strict record profile, failure policy, authored-timing formulas and lifetime.
The reader preserves numeric bits and unknown tails, rejects short/extended or
partial records, rejects duplicate interpreted chunks and refuses tracks before
the required header. These are explicit bounded-profile adaptations; old short
reads, overwrite behavior and pre-header omission are not reproduced silently.

The catalogue owns immutable decoded sections with separate errors, once per
world. Structural validity, decoded records and usable header timing are distinct.
The existing survey has a combined section-record budget; unload releases its
ownership, and the destination builds its own catalogue. Frame updates and UI
visibility do not introduce file reads, RNG, music-clock advancement or MIDI sends.

World sounds now exposes section/style/personality labels, header tempo/meter,
raw fields, authored duration and the original export tempo. Expandable command,
mute and chord rows retain source order. Opaque section-note/curve counts and
failed decodes remain explicit. The user README is preserved.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 79/79 passed | 17.34 s |
| release | 79/79 passed | 1.38 s |
| sanitize | 79/79 passed | 62.96 s |
| debug NVIDIA Vulkan | 2/2 passed | 11.42 s |
| release NVIDIA Vulkan | 2/2 passed | 5.28 s |

Commands and raw build/test results are recorded in the
[validation ledger](evidence/m2-music-sections.json). The 79 selected cases cover
music readers/authored timing/setup/sampler, configuration, object lifetime, world loading/transitions,
world sounds and streaming. The NVIDIA device was verified in each GPU log;
Release runs without the inspector, and Clang ASan/UBSan runs enable leak checks.
Eight changed C++ files pass formatting; 10,923 reference files and six reference
repositories remain unchanged. The reviewed sound capture shows truthful music
status; it does not show the nested section/track rows. Dummy audio checks submission,
not listening; native Metal/Windows/mobile execution was not tested.

Synthetic cases cover retained signed/unknown values, labels and padding, opaque
versus interpreted scope, resource release, header/stride/order errors, duplicate
chunks, all truncation prefixes, total record budgets and atomic output clearing.
Timing tests cover positive extremes, fractional quantization, signed division,
16-bit wrap and zero/negative guards. Private session tests check immutable
hidden-step ownership and weak-pointer expiry at unload. The NVIDIA world-sound
smoke checks every loaded Cabal section while the UI is hidden and after reload,
alongside existing DLS audition/SDL submission. Release disables the inspector.

Effective band/style selection, pattern variations, chord/personality pitch
processing, curve handling, original playlist transitions and actual sequencing
remain to be connected. Existing invalid effective DLS loops in Crudux/Track1
remain failures. Full world/dead-player restoration and other required M2 services
are still open; this increment does not close M2 or certify retail music parity.

Implementation commit: `f1292b0f94763baa7896182556d6417969399f2b`.
