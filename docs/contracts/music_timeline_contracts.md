# IMA style meter and section timeline

`prepare_ima_timeline` prepares owned times and source indices from an immutable
decoded section and style. It reproduces the timing part of installed IMA 2.5
reconciliation, checked against the original executable. It does not choose
patterns, harmonize chord pitches, execute mute/command values or play music.

## Style header and binding

The existing exact 94-byte `styl` reader now retains signed words at offsets
32/34 (beats per measure / beat unit), 36 (grid ticks), 38/40 (authored beat/measure
ticks), and 42 (tempo). Offset 44 is a raw unsigned word, 46–61 the identifier,
and 62–93 an opaque retained tail. These values are decoded even if unusable for
reconciliation. Do not substitute authored ticks for the original meter formula.

After all catalogue requests finish, a section binds to its exact case-normalized
`sref + .sty` request in the same music directory. A nonzero section style
identifier must equal the style identifier. Zero identifiers permit the exact
filename profile. Missing, undecoded or disagreeing styles leave a separate
`timeline_error` while preserving the section data. No basename/cross-directory
search, alias or invented fallback is allowed. This bounded binding policy is
not a port of the original general UUID/resource resolver. In the installed
content, 132 sections match by filename and identifier; 24 Track1 references have
no same-directory file. Their timelines remain unavailable.

## Original timing rules

1. Effective ticks per beat are integer `768 / beat_unit`. A zero beat unit uses
   four, locally; unlike the original, this never mutates the shared style.
   Effective ticks per measure multiply this result by beats per measure.
   Section duration is its measure count times the effective measure length.
2. Command measures retain the original signed division by the **authored**
   section measure length and low-16-bit storage. Reconciliation multiplies that
   signed stored measure by the new measure length. File order, negative and
   beyond-end times are retained; commands are not sorted or clamped.
3. Mutes are resampled at ticks `0, beat, ... < duration`. Seed the value from the
   first authored mute even if its timestamp lies in the future (or zero if
   there are no mutes). At each beat consume consecutive source records whose
   signed time is at most the sample tick; the last consumed value wins. Input
   is traversed in file order, never sorted. Out-of-order and duplicate times
   therefore have the original behavior. Authored `word6` is not copied. The
   output records the value's source index, or the no-source sentinel for an
   empty input. The replacement produces ascending samples directly; the old
   engine prepended nodes and sorted that generated list.
4. Chords with signed beat >= effective beats per measure are discarded; their
   source indices remain inspectable. **Negative beats are retained.** Kept
   times are signed measure index times effective measure length, plus signed
   beat times effective beat length. Keep file order and beyond-end times.
   The original subsequently calls chord harmonization; this helper stops
   before that operation and retains no guessed pitch results.

The portable profile rejects nonpositive section length/authored divisor,
negative beat units, zero computed beat lengths, and nonpositive or signed-16-
overflowing measure lengths. It does not emulate old division faults, wrapped
invalid meters, zero-step loops or allocation failure omissions. The original
null-style default is outside this API: callers must supply a decoded style.
All failures clear output and return an explicit error; raw input is unchanged.

## Bounds, ownership and inspection

At most 4,096 raw command/mute/chord records and 8,192 combined expanded records
per timeline; callers can lower the latter. The world survey allows 32,768
expanded records total, in request order. Expansion is preflighted before any
output is published. Discarded chord indices are bounded by the input limit.
Times and indices are owned values, not pointers into resource buffers. The
catalogue retains immutable sections and timelines together and resets both on
unload/reload. No frame/UI-triggered reads, RNG, clock advancement or MIDI sends.

World sounds shows authored versus effective meter, duration, zero-unit fallback,
source-indexed command/chord times, discarded beats and clipped resampled mute
rows. Failed binding/preparation is visible separately from structural validity
and decoding success. Style meter/grid/tempo fields are also inspectable.

The [harmony preparer](music_harmony_contracts.md) now consumes retained chords.
Effective band/current-chord selection, pattern choice, personality graphs,
curves, live sequence clocks and playlist transitions remain incomplete. `MUSIC`
still reports unsupported; M2 is open. See [evidence](../../../Documentation/Implementation/m2_music_timeline.md),
[sections](music_section_contracts.md), [notes](music_note_contracts.md) and
[bands](music_band_contracts.md).
