# M2 — Original IMA bands and channel setup

M2 remains **incomplete**. Original style/section bands now have a bounded typed
reader and an original-order channel setup plan, visible in the in-game inspector.
This supplies instrument-selection/controller inputs for sequencing. It does not
select an effective band or make `MUSIC LEVEL 3` play the soundtrack.

## Original evidence

Read-only examination of the installed external `IMUSIC25.DLL` identified the
202-byte band loader at RVA `0x15fe`, signed transpose access at `0x1c10`, volume
and pan setters at `0x186d`/`0x1902`, and channel setup at `0x1e9d`. The latter
emits volume, pan, bank MSB/LSB and program changes for each channel, with the
source-to-MIDI offset of four. The loader independently marks source channel 5
as percussion and retains legacy/custom program choices. Collection labels and
section/style scope must not be collapsed into a guessed DLS filename or override.

The [isolated probe](evidence/m2-music-bands-probe.py) executes the original load
arithmetic from `0x16c2` to `0x17d3` and the original setup routine at `0x1e9d` in
bounded x86 emulation. Intercepts provide memory copy/fill, empty-string setup,
a discarded debug trace and an in-memory MIDI sink. Name conversion, collection
loading and band selection are outside this probe. No original entrypoint,
OS imports, native DLL initialization or external I/O execute inside the emulator.
The DLL is hash-pinned; Unicorn remains an optional evidence tool, not a build
or runtime dependency. No original binary, disassembly, event payload or
commercial asset is committed.

Eight synthetic cases cover all packed flag combinations with and without a
collection, distinct channel/program/controller values and signed bytes. All
**195 installed bands (39 styles, 156 sections)** are checked for original field
copies and exact emitted message order. The probe observes **15,600 setup
messages**, with a digest independently checked by the portable private-data test.
[Numeric results](evidence/m2-music-bands-probe.json) preserve that evidence without
exporting the underlying soundtrack data.

## Implementation and limits

[Contracts](../../contracts/music_band_contracts.md) specify the layout,
packing, fallback rules, validation and ownership. The new independent C++ reader
preserves every numeric field and source range. Fixed names stop at their first
NUL; unknown padding is not interpreted. Header sizes, direct nesting, duplicate
headers and whole-file/whole-survey budgets are checked transactionally.

The setup planner reproduces original routing and controller order, but explicitly
rejects out-of-range MIDI controller/selected-program values instead of emitting
invalid messages. It preserves packed flag arithmetic without claiming DLS
instrument resolution. It has no device, audio, game clock or RNG side effects.

The existing catalogue stores immutable per-file band records once per world,
with failures separate from structural validity and from note/DLS decoding.
World sounds shows both section and style bands, actual raw channel values,
derived packed patches and an expandable list of messages clearly marked
**not sent**. No source-world state is transferred through these records.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 75/75 passed | 15.11 s |
| release | 75/75 passed | 1.36 s |
| sanitize | 75/75 passed | 63.43 s |
| debug NVIDIA Vulkan | 2/2 passed | 11.39 s |
| release NVIDIA Vulkan | 2/2 passed | 5.36 s |

Commands and raw build/test results are recorded in the
[validation ledger](evidence/m2-music-bands.json). The 75 selected cases cover
music readers/setup/sampler, configuration, object lifetime, world loading/transitions,
world sounds and streaming. The NVIDIA device was verified in each GPU log;
Release runs without the inspector, and Clang ASan/UBSan runs enable leak checks.
Eight changed C++ files pass formatting; 10,923 reference files and six reference
repositories remain unchanged. The reviewed sound capture shows truthful music
status; it does not show the nested band/channel rows. Dummy audio checks submission,
not listening; native Metal/Windows/mobile execution was not tested.

Synthetic tests cover exact field preservation, signed extremes, padding,
ordered duplicate names, direct versus nested scope, empty collections, all flag
branches, channel wrap/percussion, atomic setup failure and malformed/budgeted
files. Private tests re-encode all decoded numeric fields and check the complete
setup digest against the original executable probe. Session tests check immutable
hidden-step ownership and weak-pointer expiry at unload. NVIDIA smoke checks all
44 Cabal style/section bands, setup validity, existing DLS/SDL submission and reload;
Release runs without the inspector.

The remaining sequence/section commands, chords/mutes, effective band selection,
variation/pitch/personality processing, curves and original playlist transitions
remain to be connected. Existing invalid DLS loops in Crudux/Track1 are unchanged.
Whole-world restoration and other required M2 services remain open. The user's
workspace README is preserved.

Implementation commit: `b7c6c53976e4651d13b64f1e4d6fdbd9a23dfae6`.
