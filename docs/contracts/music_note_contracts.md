# IMA 2.5 pattern-note data

`read_ima_style` decodes the installed AASY pattern-note profile. It is an
independent portable reader, informed by the external original `IMUSIC25.DLL`
loader and checked against that loader in an isolated emulator. It is **not a
sequencer**: `MUSIC`, section transitions, personality/pitch conversion,
live variation scheduling, event dispatch and applied controllers remain
unavailable. Band setup data is now available separately under the [band contracts](music_band_contracts.md).
[Event preparation](music_event_contracts.md) supplies the post-harmonization
arithmetic with explicit inputs; it does not resolve song pitches or play notes.

## Record boundaries and ownership

The structural music reader first validates the complete RIFF tree. The semantic
data pass admits one direct 94-byte `styl` header and zero or more direct
`RIFF/AAPT` patterns (initialization styles can have none). Each pattern has one
625-byte `patt` header and ordered `LIST/AACL` grids. Each grid requires one
2-byte `clik` index and can have multiple `note` chunks. Notes are appended in
chunk/record order. Empty grids, repeated pattern names and authored grid indexes
are retained without sorting, deduplication or time repair.

A note chunk starts with a **16-bit record stride**, not a note count. This profile
requires stride 18 and an exact multiple of 18 remaining bytes. The original
loader can skip extensions above 18; no installed example uses them, so this
reader rejects them explicitly. Short records would leave old stack fields
uninitialized; that behavior is deliberately not reproduced. Truncation, duplicate
required headers, missing indexes and incorrect admitted header sizes fail.

The little-endian 18-byte payload is:

| Offset | Width | Stored field |
| --- | --- | --- |
| 0 | u8 | event type (all installed style notes use 9) |
| 1 | u8 | source channel; not the final routed MIDI channel |
| 2 | i16 | signed time offset |
| 4 | u16 | variation bitmask |
| 6 | u8 | pitch modifier, retained without conversion |
| 7 | u8 | unidentified byte, retained |
| 8 | u8 | stored MIDI note, before pitch/mode/band processing |
| 9 | u8 | velocity, before humanization |
| 10 | u16 | encoded music value, before chord/personality processing |
| 12 | i16 | signed duration |
| 14, 15, 16 | u8 each | timing, duration and velocity variation ranges |
| 17 | u8 | play-mode bits |

All bit patterns are retained, including negative/extreme signed values and
unknown event kinds/modes. Reader success means **decoded data**, not safe or
supported playback. A future scheduler must validate its supported semantics
before use; do not send these raw fields directly to the DLS sampler.

The pattern prefix exposes word0, signed length, flags, four raw signed timing
fields and its fixed 16-code-unit name. Timing fields are intentionally not
converted to seconds or inferred tempo. The full pattern header is now decoded
under the [pattern-channel contracts](music_variation_contracts.md); `curv`, `pref` and
other records remain opaque source ranges. Direct style/section bands are decoded
separately under the [band contracts](music_band_contracts.md). Curves are counted separately and
never called played/decoded events. All source chunks, offsets, sizes and nesting
remain available in the owned metadata, including partially decoded headers.
Style/pattern names accept terminated ASCII in their UTF-16LE fixed fields;
non-ASCII and unterminated names fail. Padding after the first NUL is ignored,
as in the original loader; the name is a display label, not a resource path.

The reader keeps the structural 32 MiB / 8,192 chunks / depth-16 limits and adds
256 patterns, 8,192 grids and 65,536 notes per style. A caller may lower the note
budget. Failure clears all output and reports an error; there are no retained
input spans, executable loads or DLL dependencies.

## Session and inspection

The existing one-time music survey decodes style notes alongside DLS data and
keeps an immutable shared style per unique file. A total budget of 262,144 decoded
notes applies across the survey. A valid RIFF container with failed note decoding
stays distinct from a decoded style; neither is a successful music service.
There is no added per-frame file read, music clock, RNG draw or audio output.

World sounds exposes the style/pattern/grid hierarchy, exact field values,
separate opaque-curve count and failures. Inspection is read-only, with clipped
note rows. Hidden/disabled UI does not control loading. Unload releases the
world-owned style; a destination obtains its own survey. No source-world handles
or file streams are retained.

See [evidence](../../../Documentation/Implementation/m2_music_notes.md),
[asset contracts](music_asset_contracts.md) and
[sampler contracts](music_sampler_contracts.md). M2 remains incomplete.

The [section continuation](music_section_contracts.md) now exposes bounded authored
section timing, commands, mute and chord records. It does not reconcile the style
meter, schedule events, resolve personality or implement MUSIC playback.

The [harmonic-pitch continuation](music_pitch_contracts.md) now implements the
mode/chord/scale arithmetic for explicit effective runtime harmony and feeds it
to the diagnostic event probe. Earlier missing-pitch statements are superseded
only for this step; personality/current-chord selection and live MUSIC remain open.

The [curve continuation](music_curve_contracts.md) now decodes style-grid curve
headers and points separately. Note decoding and its opaque chunk counts are
unchanged; cursor curve evaluation and live MUSIC remain unavailable.
