# IMA style curve records

`read_ima_curves` decodes the installed original style-grid curve profile into
owned typed records. It is independent of pattern-note decoding, so an invalid
curve remains visible even when a style's notes decode successfully. These are
authored records, not evaluated controller messages or audible automation.

## Format and scope

The shared structural music reader validates the complete RIFF tree first:
32 MiB, 8,192 chunks and depth 16. This semantic pass accepts only `curv` directly
inside a `LIST/AACL` grid directly inside a `RIFF/AAPT` pattern in an AASY style.
Other curve ranges, including section tracks and unknown nested containers, remain
opaque. This pass does not certify style headers or usable playback; the existing
style decoder retains its separate status.

Each chunk begins with **two u16 strides: 7 and 10**. It then repeats a seven-byte
curve header, a u16 point count and that many ten-byte points until the exact end.
It is not a single fixed-stride array of notes. Header fields are:

| Offset | Field |
| --- | --- |
| 0, 1 | u8 type and source channel |
| 2 | i16 time offset |
| 4 | u16 variation mask |
| 6 | u8 raw controller field |

Each point retains u8 shape/flags followed by i16 start, value, end and target.
Signed extremes, unknown type/channel/shape/flag values, empty curves and repeated
records survive decoding. Success does not authorize sending them to MIDI. No
sorting, deduplication, time repair or interpolation occurs. The original loader
later overwrites some header storage with derived summaries; the typed reader
keeps the authored controller byte independently and does not claim those summaries.

The original reader can accept larger strides and skip extensions. The installed
profile uses exactly 7/10; other strides fail explicitly rather than reconstruct
uninitialized short fields or guess extensions. Truncated headers/counts/points,
trailing partial records and budget exhaustion clear all output. A complete
four-byte empty chunk is valid, including with a zero record budget.

## Bounds, lifetime and inspection

Count curves **plus points** against at most 65,536 records per asset (the caller
can lower this). The world catalogue has a total 131,072-record budget. Check the
complete point count against remaining bytes and budget before allocating. Per-file
failure publishes neither partial curves nor partial counts. Immutable successful
records keep pattern/grid/chunk offsets and within-chunk ordinals, so later work
can recover their position relative to notes without inventing an event order.

The catalogue reads the existing in-memory file bytes once at world survey time;
this adds no file access or clock advancement per frame. It stores a separate
`curves_error` and immutable `curves` owner. Reload/unload releases that owner even
when the replacement fails. Copies of decoded curve values remain valid.

World sounds / **IMA curve records (data only)** exposes each asset's success or
failure, chunk/curve/point counts, source offsets, raw header and clipped point
rows. Existing note inspection now labels curve *chunks* separately. The cursor
still counts unprepared curves; decoding is not connected controller execution.

[Evidence](../../../Documentation/Implementation/m2_music_curves.md) compares the
original reader's raw field copies and linked-list order against generated and
installed records. Allocation/reads/seeks are bounded emulated facilities; the
original DLL never executes natively. Evaluation tables, interpolation, controller
scheduling, curve/note merge order, section tracks, transport and MUSIC remain
unfinished. The actual M2 gate remains incomplete.

The [style event continuation](music_curve_event_contracts.md) adds original point
expansion with explicit owner/tick inputs. Earlier evaluation omissions are
superseded only for this helper; admission, merged dispatch and MUSIC stay open.
