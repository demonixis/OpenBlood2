# IMA style curve event preparation

`prepare_ima_curve_events` expands typed style curve points with an explicit owner
and signed caller tick. It produces owned `ImaTimedMidiMessage` values in source
point order. The shared `key`/`velocity` fields hold raw MIDI data bytes here.
It does not select a variation, apply the curve header's time offset, merge notes,
send controllers, advance a clock or synthesize audio. The original dispatcher
passes the caller tick to this helper without adding that header offset again.

## Original arithmetic

This is the style-point allocation/expansion path at IMUSIC25 RVA `11ca9`, with
message delivery at `12168`. The separate section-curve path has different endpoint
rules and is not implemented by this API. The owner is explicit: the comparison
does not claim to reproduce the original loader's point-owner pointer fixups or
its overwritten header summaries.

- Types 3/4/5/6 map to pitch bend/control change/channel pressure/poly pressure.
  The output channel is `(source_channel + 4) & 15`. Other types and channels >=16
  fail. Raw controller/value bytes are preserved; success is not safe MIDI routing.
- Shape 1 emits one value at point start: target normally, value with flag bit1.
  Flag bit0 has no effect on this branch. Other shapes sample 193 table positions:
  shape2 is reversed logarithmic complement, shape3 logarithmic, shape4 cosine
  ease, and every other shape uses linear values. Upper flag bits are ignored.
- Tables are regenerated mathematically, not copied from the executable. The
  logarithmic table is `floor(19200*log10(1+9*i/192))`; its reverse complement is
  `19200-log[192-i]`. The cosine table is `floor(9600*(1-cos(pi*i/192)))`, with
  its exact middle value 9600 set explicitly to avoid libm boundary rounding.
  Linear entries are `100*i`. All 772 entries match the installed original.
- Value and time deltas, interpolated values/times and flag-bit1 inversion wrap
  as signed 16-bit quantities. Division truncates toward zero. Bit0 reverses the
  value table, not time. Do not repair descending or zero-duration segments.
- Emit the initial value. For interior samples 1..191, require at least three
  signed ticks since the last emitted event and a changed 16-bit value. Compare
  values before truncating to message bytes; duplicate byte messages can survive.
- Emit the final value only when it differs from the last emitted value. If its
  time equals the last event time, replace that event; otherwise append it.
  Do not sort points or deduplicate across them. A flat curve has one event.
- Pitch bend uses `value & 127` and `(unsigned16(value) >> 7) & 254`, preserving
  the original low-bit clearing in its high byte. Other types use the raw owner
  controller and low byte of value. These may exceed standard MIDI data ranges.

Original allocation failure could yield partial lists. The portable API instead
fails atomically: clear output/error on entry, publish only a complete result,
reject signed 32-bit emitted-tick overflow, at most 4096 points and 65536 messages
(the caller can lower the message budget). Replacing an endpoint does not consume
an additional slot. Empty valid curves succeed with a zero message budget. There
are no retained resource pointers, callbacks, random draws or playback side effects.

## Inspection and checks

World sounds / IMA curve records / each curve / **Prepared curve events (no playback)**
shows an explicit caller tick, complete event count and clipped ordered rows, or
actual failure. Preparation only occurs while that subtree is open; its local
results disappear each frame, while the only retained UI input is an integer.
Closing/reloading never leaves a borrowed resource or stale displayed result.

The NVIDIA sound smoke prepares 206 Cabal messages independently of the inspector,
including hidden and inspector-free execution and world teardown/reload. The
[original comparison](../../../Documentation/Implementation/m2_music_curve_events.md)
covers 1296 generated decisions and all 47 installed points with explicit contexts.
Read [curve records](music_curve_contracts.md) and [note events](music_event_contracts.md)
before connecting dispatch. Admission, curve/note order, section tracks, controller
application to voices, playlists and live MUSIC remain pending; M2 is incomplete.
