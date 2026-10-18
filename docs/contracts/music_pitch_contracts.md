# IMA note pitch contracts

`resolve_ima_note_pitch` reproduces the installed IMA note-mode dispatcher and
harmonic arithmetic before [event preparation](music_event_contracts.md). It takes
an immutable note/pattern and **explicit effective runtime harmony**: activation,
chord/scale masks, separate signed roots and chord flags. An authored section
chord is not automatically that runtime context. The [harmony preparer](music_harmony_contracts.md) now derives effective masks, roots and flags.
Current-chord selection and live scheduling remain unavailable; MUSIC is unsupported.

## Ownership and admission

Only note-body types 8/9 and source channels 0..15 are admitted. All data is
borrowed for the call; output owns a signed pitch, effective mode nibble and a
conversion/bypass indicator. There are no allocations on success, callbacks,
retained pointers, clocks, random draws or world mutations. Failure clears all
output and returns a diagnostic; a successful zero pitch suppresses subsequent
events and is not a service failure. No mute/variation eligibility is decided here.

## Original conversion order

- Disabled harmonic mode or zero `music_value` bypasses conversion and returns
  the sign-extended raw MIDI byte. This also bypasses octave/range processing.
- Otherwise, the note mode inherits the pattern mode if bit 0x80 is absent or
  its low nibble is 7. Zero pattern modes receive the loader default: channel 5
  uses 1, channel 4 uses 2, others use 3. Only the effective low nibble dispatches.
- Mode 1 uses the signed raw MIDI byte but continues through octave/range rules.
  Mode 2 calls the harmonic core with flags zero. Modes 5/6 first transform the
  music word as `(music & 0xf00f) | (((music & 0x0f00) >> 3) + (music & 0x00f0))`;
  mode 5 ignores core flags, mode 6 honors them. Other modes use the core with
  actual flags. Mode 4 follows its distinct scale-only path below.
- The core extracts octave/chord degree/scale degree/chromatic nibbles. Chromatic
  nibbles above 8 subtract 16; 8 remains positive. The base is octave*12 plus
  chord root, wrapped to eight bits. Flags' low nibble increases chord degree
  above threshold 4 when bit 0x20 is set, otherwise above 3.
- Walk the chord mask with arithmetic right shifts. A zero initial mask skips
  the walk. Exhausting a nonzero mask returns zero when the modifier is zero;
  otherwise reset to the base and use the modifier's signed high nibble as a
  byte countdown and its **unsigned** low nibble as the chromatic offset.
- For nonzero scale degree, repeat the scale as `scale | (scale << 12)`, retaining
  low 32 bits. Shift arithmetically by the signed remainder of
  `(unsigned-byte pitch - signed scale root) / 12`, using x86's low-five-bit shift
  count. Walk scale bits; an exhausted mask stops without an invented failure.
  Pitch and degree increments/decrements wrap at eight bits. Return pitch plus
  chromatic offset as an unsigned byte.
- Flag 0x40 lowers the core result one octave only if it remains positive;
  modes 2/5 ignore this flag. A zero result skips remaining conversion. Otherwise
  subtract 12, retaining low signed 16 bits.
- Pitch ranges apply only if the pattern's channel bit and chord flag 0x10 are
  both set. Move down whole octaves to the signed upper bound, then up to the
  signed lower bound, using the original low-word unsigned division. Inverted
  bounds retain this order without rechecking the upper limit.

Mode 4 starts at octave*12 plus the signed **scale** root. Its signed modifier
high nibble is a wrapping 16-bit countdown. Arithmetic-shift the repeated scale
first, decrement on a set bit, increment pitch on every iteration. Add its
sign-corrected low nibble (again 8 stays positive) and 12 before the common
zero/octave/range rules. Once the shift reaches all ones, the implementation
adds the remaining countdown at once with identical low-word output. Once it
reaches zero with a nonzero countdown, it **fails**: the original would loop
forever. This bounds the walk to at most 32 iterations without rejecting finite
negative-countdown cases. Core byte walks are also finite: positive masks exhaust,
negative masks become all ones and finish their byte-sized countdown.

## Integration and evidence

The grid inspector can use these explicit harmonic inputs to feed the resolved
pitch into the existing event probe. It shows raw bypass, effective mode, pitch,
refusal/suppression and unsent note pairs. Manual already-resolved pitch remains
available. Numeric controls retain no asset pointers. UI visibility does not
advance any music state; this is not a selected soundtrack chord or playback.

Original executable comparison covers 33 explicit cases, 2,048 generated cases
and all 7,030 installed notes in three explicit contexts (21,090 decisions).
The two infinite-walk fixtures and 25 generated infinite walks fail instead of
hanging. Private data has no such refusal in these contexts. Result hashes are
from the hash-pinned external DLL in bounded isolated x86 emulation, not a second
implementation of the formulas. No imports, native loading or commercial bytes
are required in the repository. See [evidence](../../../Documentation/Implementation/m2_music_pitch.md).

GPU sound checks feed converted diagnostic pitches through event preparation
while exercising hidden stepping/reload; inspector-free builds run the same
checks. This does not verify live chord selection, full song sequencing or listening
parity. Current-chord/pattern/band selection, personality graphs, curves, live note ownership and
playlist/controller application remain required. M2 is incomplete.
