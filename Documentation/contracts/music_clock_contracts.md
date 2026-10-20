# IMA musical tick to audio sample clock

M2 and `MUSIC` remain incomplete. `ImaClock` maps explicit nonnegative IMA ticks
to the existing 48 kHz audio clock. `DlsSampler::enqueue_ima` converts a complete
ordered batch before calling the existing atomic sample queue. It does not run
an IMA cursor, select a playlist, seek, retime already queued messages or activate
a game MUSIC command. Read [timed delivery](music_timing_contracts.md),
[section records](music_section_contracts.md),
[evidence](../implementation/reports/m2_music_clock.md) and
[source ledger](../implementation/sources/m2_music_clock_sources.json).

## Original runtime arithmetic

IMRT25 is the installed runtime layer between IMUSIC25's tick-based messages and
MSynth25's millisecond-based receiver. Its normal-speed tempo-node constructor
clamps integer tempo below 10 to 10.0 BPM and at least 350 to 350.0 BPM. Otherwise
it retains the authored fraction. Runtime conversion uses the upper four fraction
bits: `tempo16 = whole * 16 + (fraction >> 12)`. A quarter note is 192 IMA ticks,
independent of the style's beat unit. The MIDI export's 1/64-BPM formula is a
separate path and must not be reused as the playback clock.

The first tempo begins at tick zero and at the caller's output sample origin.
Later tempo boundaries are nondecreasing; duplicate ticks preserve input order,
with the last tempo used at that tick. For each new boundary the original runtime
retains accumulated time in 100-nanosecond units:

```
next.time100ns = previous.time100ns +
                floor((next.tick - previous.tick) * 50000000 / previous.tempo16)
```

For an event, select the last boundary at or before its tick. Its millisecond
value is the boundary's truncated milliseconds plus the delta converted with
Win32 MulDiv's nearest rounding (positive halves upward):

```
ms = floor(boundary.time100ns / 10000) +
     round_nearest((tick - boundary.tick) * 5000 / boundary.tempo16)
frame = origin_frame + ms * 48
```

Retain both rounding stages. A 120-BPM tick maps to 3 ms; a tempo change at that
same tick starts at the accumulated 2 ms. This is the original distinction
between event conversion and tempo-boundary accumulation. The output adaptation
keeps millisecond quantization and uses 48 samples/ms instead of the installed
synth's selectable 44.1/22.05/11.025 kHz conversion. It makes no claim of original
PCM equivalence. Tempo-speed modifiers and operating-system timer jitter are not
recreated.

## Bounds, atomicity and ownership

A map owns at most 4096 value-only boundaries. The first tick must be zero, all
ticks nonnegative/nondecreasing, and authored signed tempo positive. Original
zero/negative unsigned interpretations are rejected. Supported milliseconds end
at INT32_MAX; sample-origin overflow also fails. Intermediate products use checked
bounded 64-bit ranges rather than historical wrapping. Preparation failure leaves
the previous map/origin intact; query failure clears its scalar result. There are
no files, callbacks, device handles or asset references in this object.

Conversion accepts at most the existing 65536-message batch budget, preserving
submission order even when timestamps are unordered or collapse to the same
sample. Any invalid clock/tick/overflow rejects the whole batch before a bank latch,
channel or voice can change. The queue then applies its own time, message, resource
and capacity validation. Successful conversion/admission is not successful future
synthesis. Conversion refusals use the queue's existing durable error/count.

Negative events, retrospective tempo edits, and scheduler lookahead are explicitly
unsupported here. A later section player must submit its complete tempo map before
converting events and must handle note tails, early jitter and transitions without
silently clipping them. No hidden UI transport or automatic bank fallback exists.
World unload still drops the queued immutable banks and live voices; a copied
clock contains no asset and cannot preserve playback across reload.

## Inspection

Section inspection shows the effective runtime BPM and style-reconciled duration
in seconds/samples separately from authored export timing. Selected section bands
can schedule a diagnostic quarter note after one quarter-note delay, using their
actual tempo and setup. The existing queue shows the resulting absolute sample
times and actual delivery/failure counts. Gain remains the labelled 20% audition
value. Rendering this information never changes audio time or cursor state.

Tests compare 98136 clock queries against isolated original runtime routines,
including 156 installed sections. The original constructor, boundary accumulation
and clock lookup run; MulDiv, locking and diagnostic logging are intercepted.
That probe does not execute Windows MulDiv itself or establish listening parity.
Synthetic PCM checks compare full and single-sample blocks. NVIDIA hidden/reload
and inspector-free checks deliver the real section tempo/band with a short
synthetic phrase through SDL audio. No soundtrack playlist is claimed.
