# M2: Real PCM streaming and original CityHub emitters

Project commit: `669d1064e1098e3c56da5dbd2ad49b77bf3b7f8d`.

Original Sound and SoundFX FileStream requests now read PCM from their indexed
REZ slices, using two bounded sample windows instead of loading the entire WAV.
The original lifetime, ON/OFF/ramp and duration rules remain unchanged. Streamed
and buffered voices use the same local sample clock and renderer-independent
updates. No commercial asset, reference repository or dependency was changed.

[Contracts](../../contracts/audio_stream_contracts.md),
[source ledger](../sources/m2_audio_stream_sources.json) and
[retained evidence](evidence/m2-audio-stream.json) record the boundaries.

The actual CityHub initialization now produces **418 created objects, 31 transferred
world sounds, 541 editor-only entries, 32 unsupported classes and four failures**.
All five original SoundFX objects create with their unmodified authored properties;
four start at initialization and the fifth is authored off. There are 35 active
streamed voices, sharing **384 KiB** of windows for 12 resources. Two unload/reload
cycles verify these counts and release all objects, voices and cached streams.
This is initialization/audio evidence, not a full CityHub gameplay run. Structural
preparation has 453 schema-checked / 32 unknown / zero invalid entries. The four
remaining creation failures are the existing two brush failures, one Door failure
and one rejected Cultist configuration. The safe original world transition remains
unimplemented; no source session is destroyed to demonstrate these numbers.

The available original Sound/SoundFX and SDK do not consume VoiceType. Across
916 authored Sound/SoundFX entries, 905 contain integer zero and 11 omit it.
Only these classes now accept zero as explicitly retained, unused metadata.
Nonzero values and wrong types reject; no guessed audio category is assigned.
This compatibility choice follows the available original source, not established
older-binary parity. Three TypeAction SoundFX schemas remain unresolved.

The 57 readable-world catalog constructs **121/124 SoundFX**, starts 100 initial
voices with zero playback failures, and retains three TypeAction rejections.
One referenced but initially inactive sound is missing. The built-in Sound catalog
starts **696/792** requests, with zero schema rejections and 96 explicit playback
failures (81 invalid/over-limit requests, 15 missing/empty paths). These failures
remain failures; this work does not claim all campaign sounds work. All successful
streamed Sound assets are sampled against the existing SDL buffered decoder.

The reader accepts PCM8/16 mono/stereo, validates RIFF sizes, alignment, chunk
counts and resource boundaries, and retains the bounded retail terminal padding
exceptions. It reads headers first and payload windows only as needed. Compressed
streaming and region-loop metadata fail explicitly. Whole-clip ADPCM support remains
available only for buffered requests. Stream cache memory counts against the shared
128-entry / 256 MiB budget; voices retain their separate 128 limit.

Read failures latch visible voice errors, preserve the failing cursor, refuse
successful completion and discard partial output. Archive truncation after playback
and recovery after unload are tested; arbitrary same-size asset replacement is not detected. Disk I/O is
synchronous on the simulation thread, so slower storage can stall it. No worker
system or new library was added. Local direct delivery satisfies the shared local
clock; late network synchronization, reverb and priority arbitration remain absent.

World sounds and Objects expose stream/buffered mode, actual bytes read, refills,
fixed window memory, voice timing and errors. Historical metadata remains visible
in the original properties. The GPU probe now exercises streamed PCM through
original SoundFX while the inspector is hidden and after world reload.

Validation on Linux:

- Debug and inspector-free Release: **672/672 headless tests each**, including
  107 private-data cases (104.71 s / 15.20 s).
- Clang ASan/UBSan with leak detection: **80/80 focused tests**, including
  12 private-data cases (81.06 s), covering object lifetime, world preparation,
  services, cinematic/random/world/SoundFX audio and PCM streaming.
- Debug and inspector-free Release: **59/59 Vulkan GPU tests each**, including
  43 private-data cases (191.68 s / 118.63 s). Together with headless tests,
  each configuration passes **731/731** cases, including 150 private-data cases.
  No Vulkan validation or sanitizer diagnostic was found in retained logs.
  The SoundFX streaming probe passes with hidden inspection and world reload;
  the world-sound inspection capture was visually checked.
- Formatting: 17 changed C++ files; 2,869 local Markdown links resolve.
  All 10,923 inventoried reference files match their hashes and the six reference
  repositories remain clean. Dependency revisions are unchanged.

Commands run from `Blood2/`:

```sh
cmake --build build/debug -j4
cmake --build build/release -j4
ctest --test-dir build/debug -L headless -j4 --output-on-failure
ctest --test-dir build/release -L headless -j4 --output-on-failure
ctest --test-dir build/debug -L gpu --output-on-failure
ctest --test-dir build/release -L gpu --output-on-failure
cmake --build build/sanitize --target audio_stream_tests sound_fx_tests sound_tests random_sound_tests cinematic_tests object_contract_tests services_tests world_tests -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize -j2 -R '^(objects|services|world|sound|randomsound|soundfx|audio_stream|cinematics)\.' --output-on-failure
```

Two older world tests initially expected all FileStream/VoiceType requests to fail.
The sound-entry fixture now verifies a real streamed world voice, loops and complete
unload cleanup while retaining nonzero VoiceType rejection. The structural-destination
fixture uses the still-unsupported TypeAction property to keep its schema-failure,
no-construction and source-session preservation checks. No runtime failure was
relabeled as successful playback to make those tests pass.

M2 remains incomplete. The remaining classes/services, original-session transition
and observable exit criteria have not been replaced by class counts or diagnostic
playback. No manual listening assessment, whole-level playthrough, cross-platform
verification or retail mixing parity is claimed.
