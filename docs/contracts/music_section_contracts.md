# IMA 2.5 section records and authored timing

The independent `read_ima_section` reader decodes the installed AASE profile.
Original `IMUSIC25.DLL` reader equivalence is checked in isolated x86 emulation;
no binary or native ABI becomes a project dependency. These are **authored input
records**, not a running sequence or a successful MUSIC service.

## Record profile

The structural reader validates all RIFF bounds, padding, nesting and the direct
style reference first. Exactly one direct `secn` of 120 bytes is required. Direct
`cmnd`, `mute`, `chrd` and `prnm` are optional, but duplicates fail. Event chunks
before `secn` fail explicitly: the old loader ignored them before allocating its
section. Nested records in unknown containers remain opaque. No records are
sorted, merged, deduplicated or repaired.

| `secn` offset | Stored field |
| --- | --- |
| 0 | Raw u32 word0; not used as an invented start/duration |
| 4–35 | Fixed 16-code-unit UTF-16LE name |
| 36, 38 | Signed tempo integer and raw signed word38 |
| 40, 42, 44 | Signed measure count, ticks per measure, ticks per beat |
| 46 | Unsigned fractional tempo word |
| 48 | Raw u32 word48 |
| 52–55 | Default chord byte and three retained padding bytes |
| 56–71, 72–87 | Style and personality identifiers, retained bytes |
| 88–119 | Retained uninterpreted tail |

`prnm` is a terminated UTF-16LE display label of at most 80 bytes. It is not a
path and does not load a personality file. Fixed names and this label admit
printable ASCII and stop at the first NUL, ignoring subsequent padding. Empty
labels are admitted; unterminated, odd-size or unsupported text fails.

All three event chunks begin with a **u16 stride**, followed by whole records:

- `cmnd`, stride 8: signed i32 time, four raw command bytes.
- `mute`, stride 8: signed i32 time, raw u16 value and raw u16 word6.
- `chrd`, stride 117: fixed UTF-16LE name (32 bytes), u8 flags, signed i8 beat,
  signed i16 measure, u32 chord pattern, u32 scale pattern, raw u32 word44,
  signed i8 root, and 68 retained uninterpreted bytes.

The original chord loader maps flag bits 0–3 individually; the data reader keeps
**all** bits, including unidentified ones. Masks/root are not converted into
final MIDI pitches. Command bytes are not assigned guessed intensity semantics;
mute records are not applied to channels. The installed files have no direct
section `note`/`curv` chunks; these remain explicitly counted opaque ranges.

The old readers accepted larger strides and skipped extensions; this profile
rejects them. Short records could leave old stack values uninitialized and are
also rejected. Bounds are 32 MiB, 8,192 chunks, depth 16 and 4,096 combined decoded
commands/mutes/chords per file. Callers may lower the record budget. Output owns
all decoded values and source metadata, and is cleared completely on failure.

## Checked authored timing

`ima_section_timing` reproduces the header's measure-count multiplication and
original MIDI-export tempo conversion:

```
duration_ticks = measures * ticks_per_measure
tempo64 = (tempo << 6) | (tempo_fraction >> 10)
microseconds_per_quarter = 3840000000 / tempo64
```

The fractional tempo is quantized to 1/64 BPM before integer division, matching
the examined exporter. Nonpositive tempo, measure count or meter explicitly
fails and clears the result. All positive signed-16 products fit signed-32.
This is not an elapsed-time clock and does not claim the exporter is the original
live synth's exact timing implementation. The original default meter is 768 ticks
per measure and 192 per beat; the loaded style can override the meter and rebuild
tracks. Therefore the inspector labels these results **authored**, never effective
playback duration or current musical position.

`ima_section_command_measure` reproduces signed time division by positive header
ticks-per-measure, truncation toward zero and the original low-16-bit storage.
Explicit unsigned conversion plus bit-cast preserves wrap without implementation-
defined narrowing. Nonpositive divisors fail. The original style reconciliation,
command execution, chord/personality processing and mute resampling are separate.

## Session, inspection and remaining work

The one-time world survey stores an immutable section per unique file, with a
separate decoding error and a total limit of 16,384 decoded section records.
Structural validity, section decoding and timing validity remain distinct.
Frame updates and hidden/disabled inspection add no file reads, RNG, clock or
MIDI/audio output. Unload releases the world-owned data; a destination receives
its own survey. There are no retained input spans, object handles or streams.

World sounds exposes header tempo/meter, style/personality labels, raw unknown
header fields, authored duration/export tempo, separate command/mute/chord rows,
opaque-track counts and failures. Original playlist transitions, effective band
selection, pattern/variation choice, pitch/personality processing and actual
sequence playback remain unavailable. M2 remains incomplete.

See [evidence](../../../Documentation/Implementation/m2_music_sections.md),
[asset contracts](music_asset_contracts.md), [notes](music_note_contracts.md) and
[bands](music_band_contracts.md).
