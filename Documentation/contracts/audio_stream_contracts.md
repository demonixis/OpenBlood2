# Disk-backed PCM audio and historical audio metadata

This extends [SoundFX](sound_fx_contracts.md) and [world Sound](world_sound_contracts.md).
Read [source selections](../implementation/sources/m2_audio_stream_sources.json) and
[implementation evidence](../implementation/reports/m2_audio_stream.md).
M2 remains incomplete. This is replacement engine code implementing the source
FILESTREAM contract; it is not recovered retail mixer code.

## Real resource streaming

Original Sound and SoundFX FileStream requests now select an actual disk-backed
PCM reader. They do not use the full-file resource read, SDL_LoadWAV, or a whole
float clip. The reader owns one open archive descriptor and a copied indexed REZ
resource slice. Every read checks both the slice and current archive extent.
No lookup by basename or extraction to loose commercial files is introduced.
The lifetime is independent of later resource-index remounts; unload clears the
cache after original objects release their voices.

Preflight reads only RIFF/chunk headers, the first 16 bytes of PCM format and
optional LIST type. Payload ranges are validated without reading their contents.
At most 1024 chunks / 128 MiB per resource are accepted. PCM8/16, one or two
channels, 8–96 kHz, exact block/rate/data alignment and the existing bounded final
odd-data padding exceptions are supported. Missing/duplicate fmt/data, malformed
padding/ranges, and compressed streams fail. MS/IMA ADPCM retain buffered support,
but are not silently decoded wholesale for a streaming request.

Samples come from two 4096-frame windows, at most **32 KiB per cached stream**.
A refill reads only its aligned PCM block. Two windows preserve interpolation
across block and loop seams. Conversion is unsigned PCM8 / signed little-endian
PCM16 to float; tests compare against the existing SDL buffered decoder.
Resources shared by voices share immutable format metadata and this simulation-
thread-only read cache; each voice still has an independent cursor/loop count.
Inspection exposes actual bytes read and refill counts. Offline clocks need not
read inaudible/unrendered samples; they still validate archive extent each advance.

Buffered and streamed forms are distinct cache entries. Their combined limit is
128 entries / 256 MiB (streams reserve their actual fixed windows), with the
existing maximum 128 voices. No worker thread, background prefetch or dependency
is added. Reads occur synchronously in bounded windows on the simulation thread;
slow storage can still stall that thread. This is not a latency guarantee.

## Clocks, errors and lifetime

Both paths use the same sample clock, fractional carry, resampling, attenuation,
panning, loop count and 250 ms output-work limit. Source TIME/TIMESYNC requests
start on the same local simulation timeline as their voice: local direct delivery
has no network delay. Late network-client synchronization and streaming compressed
codecs are not implemented. No separate guessed duration or wall-clock completion
is used; duration is the validated PCM frame count divided by sample rate.

A failed read latches an inspectable stream/voice error. The failing voice does
not advance its cursor, IsSoundDone refuses to report successful completion, the
mix buffer is discarded and no partial output is submitted. Other voices already
visited may have advanced: an error is not a transactional rewind. The failure
persists until its voice/cache is removed; callers must not retry the same step as
if it had been rolled back. World unload clears the prior output diagnostic; a
new SDL queue-clear failure replaces it with that actual failure. Archive checks detect absence/truncation/read failures,
not arbitrary same-size byte edits. Resources must remain stable during playback.

Original ownership remains unchanged: built-in Sound transfers its loop to the
world and removes the source; SoundFX retains its checked voice until replacement
or destruction. Stream resources are cached until unload like buffered clips.
Source-independent sounds, shared caches and temporary destination rehearsals do
not retain raw references into another session. No new mutable game global is
introduced. Reverb, priority arbitration and region-loop metadata remain unsupported.

## Historical VoiceType policy

Available Blood II Sound/SoundFX sources and PlaySoundInfo do not declare/read
VoiceType. The private catalog contains 905 such properties, all integer zero;
11 audio entries lack it. The later-engine property lookup and Shogo AdvSound are
read-only corroboration (AdvSound reads/saves the field but does not pass it to
PlaySound). Their generation is not claimed retail Blood II parity.

Only Sound and SoundFX accept the observed integer **VoiceType=0** as explicitly
retained, unused historical metadata. No guessed voice category, priority mapping
or audio behavior is attached to it. Nonzero values reject precreate; wrong types
and unrelated unknown properties remain schema failures. This bounded compatibility
choice follows the available original code, not a claim about the older binary's
meaning. No alias to TypeActionMode is introduced; the three TypeAction SoundFX
entries are now created under the retired-property rule of the
[object contracts](object_contracts.md#retired-world-properties). Properties remain visible in the inspector.

## Verification scope

Tests cover actual read budgets, PCM sample equality at window boundaries and
random seeks, mono/stereo, 8/16-bit, malformed ranges/headers/padding, loop seams,
shared caches with independent clocks, delayed output, source removal, stale IDs,
archive truncation after playback, cache cleanup and original CityHub emitters.
Catalog tests retain authored properties; they do not strip streaming flags or
rewrite commercial data. Private sampled PCM is compared with SDL decoding.
GPU Sound uses real streamed SubwayA PCM through original SoundFX, with hidden UI,
unload/reload and inspector-free Release. Full level playthrough, audible listening,
retail mixing parity, safe NEW_LEVEL and actual M2 acceptance remain open.
