# IMA 2.5 bands and channel setup

`read_ima_bands` reads the installed 202-byte `band` records in direct
`RIFF/AABN` children of AASY styles and AASE sections. `ima_band_setup` prepares
the original ordered channel-configuration messages. These two functions do not select
an effective band, load an instrument collection, send MIDI or play music.
The separate [selection stage](music_band_selection_contracts.md) now chooses a
section override or the marked default in its referenced style.
`MUSIC` remains unsupported until the original sequencing path is connected.

## Ownership and decoding

The existing structural reader validates the entire RIFF tree first, including
section style references. Each direct AABN requires exactly one direct `band`
header of exactly 202 bytes. Missing/duplicate headers and other sizes fail.
Zero bands is valid data, not evidence of a ready-to-play style or section.
Repeated names and multiple bands retain file order; raw decoding does not merge or override entries. The separate selection stage
uses original header guards, section override and style-default order. Bands in nested/unknown containers remain opaque.
All source chunk ranges are retained, including partial and undecoded records.

| Payload offset | Stored field |
| --- | --- |
| 0–31 | Fixed 16-code-unit UTF-16LE display name |
| 32–39 | Eight reserved bytes, preserved |
| 40–55 | Sixteen legacy/GM program bytes |
| 56–71 | Sixteen volume bytes |
| 72–87 | Sixteen pan bytes (64 is the original getter's center) |
| 88–103 | Sixteen signed transpose bytes |
| 104, 105 | Signed byte104 (style default flag) and raw byte105 |
| 106–137 | Sixteen little-endian packed bank/flag words |
| 138–153 | Sixteen custom program bytes |
| 154–169 | Sixteen raw channel bytes; meaning not inferred |
| 170–201 | Fixed 32-byte collection display label |

Names require a NUL terminator and printable ASCII before it. Original padding
after the first NUL is ignored. Empty names/collection labels are admitted.
**Collection labels are not paths**: do not append extensions or open files from
them. The original game asset plan remains the source of external DLS requests.
Every numeric bit is retained, even if it cannot safely become a MIDI value.
Reader success is a data claim, not playback or instrument availability.

Limits remain 32 MiB, 8,192 chunks and depth 16, plus 256 decoded direct bands
per file. Callers may lower the band budget. Any failure clears all output.
Results own their strings, channel arrays and source metadata; no input span,
file stream, executable code or native ABI structure is retained.

## Original setup arithmetic

For source channel `i`, the loaded patch starts as `(bank << 7) | program`.
An empty collection adds bit `0x400000` unless bit `0x200000` is set. Source
channel 5 additionally receives `0x80000000` (percussion). Keep these flags
separate from DLS bank identifiers and effective collection selection.

The original apply routine routes channel `i` to `(i + 4) & 15`, preserving
source order 0 through 15. Each channel emits, in this order:

1. CC7 volume.
2. CC10 pan.
3. CC0 bank MSB.
4. CC32 bank LSB.
5. Program change.

When `(patch & 0x600000) == 0x400000`, both bank controllers are zero and the
legacy program byte is used. Otherwise the bank values are `(patch >> 14) & 127`
and `(patch >> 7) & 127`, and the program is `patch & 127`. Program-change
`data2` is zero. This is the verified band-apply routing, not a blanket rule for
all original note/section consumers. Transposition is retained, not applied here.

`ima_band_setup` returns exactly 80 value messages or fails atomically with a
zeroed output. Unlike the old routine, it refuses volume, pan or the selected
legacy program above 127. It does not clamp or silently repair malformed data.
Unused legacy programs can contain any byte when the custom branch is selected.
The original lower-seven-bit packing is preserved, including raw program bits
that overlap bank bits. No collection resolution, synth fallback or MIDI send
is claimed by preparing these messages.

## Session and inspection

The once-per-world survey stores immutable bands for each unique style/section,
with a separate band-decoding error. The aggregate limit is 4,096 bands. A failed
band decode never changes structural validity or implies success elsewhere.
There is no added per-frame I/O, RNG, music clock or audio submission. Unload
releases these values; the destination loads its own catalogue.

The World sounds inspector separates style and section assets, exposes decoded
channels, raw/derived patch values, signed transpose and unknown bytes, and can
show the ordered setup messages. It labels these messages **not sent** and
reports invalid MIDI values. Hidden/disabled inspection does not affect decoding
or lifetime. Initial effective-band selection is covered by the separate contract. Remaining
controller application, curves, live scheduling and playlist semantics must be
connected before the soundtrack is available.

See [evidence](../implementation/reports/m2_music_bands.md),
[note contracts](music_note_contracts.md) and [asset contracts](music_asset_contracts.md).
M2 remains incomplete.

The [section continuation](music_section_contracts.md) now exposes bounded authored
section timing, commands, mute and chord records. It does not reconcile the style
meter, schedule events, resolve personality or implement MUSIC playback.
