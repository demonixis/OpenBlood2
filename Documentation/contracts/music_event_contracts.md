# IMA note event preparation

`prepare_ima_note_events` implements the original processing **after** note pitch
conversion. It returns a pair of timed MIDI value messages, or a successful
zero-pitch suppression. It does not choose notes, resolve harmonic pitch, select
a band, advance a sequencer or send audio. `MUSIC` remains unsupported.

## Inputs and ownership

The caller supplies an immutable decoded `ImaNote`, an explicit signed grid tick,
a signed 16-bit **already resolved** pitch, the effective band's signed transpose,
an optional explicit percussion map, and up to three original 15-bit random draws.
The pitch is the value after the original play-mode/harmonic/octave/range work,
just before its zero-pitch gate. It must not silently default to `midi_note` or
`music_value`: those fields can still require harmonic conversion.

The function admits the original note-body dispatch for type 8 or 9 and source
channels 0..15. Other event kinds require their separate original path and fail
here. This does not decide mute/variation eligibility or when a grid executes.
A supplied percussion map must have 128 entries. Its values remain raw unsigned
bytes; selection of a real mapping table is outside this helper. Input spans
are borrowed only during the call. Output owns values, with no pointers, file
streams, callbacks, allocations, clocks or RNG state.

## Original event rules

1. Resolved pitch zero suppresses the note **before** transpose, mapping or random
   consumption. A successful suppression has `emitted == false` and zero draws.
   This is distinct from a failure. Other input shape validation still applies.
2. Each nonzero authored time/duration/velocity range consumes exactly one draw,
   in that order. Range bytes and the stored velocity are interpreted as signed
   8-bit values. For signed range `r`, the delta is
   `draw % abs(r) - floor(r / 2)`. This preserves the original nonnegative
   dividend remainder and arithmetic right shift, including odd/negative ranges.
   Range 1 still consumes a draw even though its delta is zero.
3. Start time is grid tick plus signed note offset plus time delta. Duration is
   signed authored duration plus duration delta, clamped to a minimum of one tick.
   Negative start times are retained; there is no invented zero/end-of-section
   clamp. Final start or end outside signed 32-bit range fails atomically instead
   of reproducing the original timestamp wrap.
4. Source channel 5 uses the optional map, indexed by the resolved pitch **before
   transpose**. An out-of-range index fails instead of reproducing an invalid
   memory access. Other channels ignore a well-formed supplied table. Mapping
   to zero is not the initial suppression: the subsequent octave rule lifts it.
5. Add signed band transpose and retain the original low 16 bits as a signed
   value. Below 1, add `((12 - pitch) / 12) * 12`; above 127, subtract
   `((pitch - 116) / 12) * 12`. These are octave folds, not endpoint clamps.
6. Signed velocity plus its delta is clamped to 1..127. Route the source channel
   as `(channel + 4) & 15`. Produce note-on at start and note-off at start+duration,
   with status `0x90 | channel` / `0x80 | channel`, the same final key and **the
   same velocity**. Type 8 reaches this same paired-note body in the original;
   do not reinterpret it as an isolated note-off at this stage.

Draws must be exactly counted, each in 0..32767. There is no internal randomness;
failed preflight cannot advance a random source. Errors clear all output. The
value-only pair is prepared together, so a caller cannot get a successful on
message with a failed/missing off message from this function. Original queue
allocation failures, voice identity/cancellation and note-off ownership metadata
are not recreated by this preparation API.

## Inspection and verification scope

Each nonempty grid's inspector offers an explicitly labelled note-event probe.
The user supplies a resolved pitch, grid tick, transpose and draws. It displays
both resulting events or the actual failure/suppression. It neither interprets
harmonic data nor changes the world/music RNG, playback state or MIDI output.
The UI supplies no percussion map. Closing/hiding the UI has no clock or lifetime
effect; stored controls contain only numbers, not resource pointers.

Private equivalence uses all 7,030 installed notes twice with synthetic explicit
contexts and the raw-pitch branch. It verifies **this tail**, not the actual
song's resolved pitches, times, band selection or output. GPU smoke also uses
labelled fixed pitch/tick inputs while exercising hidden steps and reload. See
[original-code evidence](../implementation/reports/m2_music_events.md),
[note records](music_note_contracts.md), [variation decisions](music_variation_contracts.md)
and [band setup](music_band_contracts.md).

Harmonic pitch processing, pattern/band selection, live note/curve scheduling,
controller application, playlist transitions and actual MUSIC playback remain
required. M2 remains incomplete.

The [harmonic-pitch continuation](music_pitch_contracts.md) now implements the
mode/chord/scale arithmetic for explicit effective runtime harmony and feeds it
to the diagnostic event probe. Earlier missing-pitch statements are superseded
only for this step; personality/current-chord selection and live MUSIC remain open.
