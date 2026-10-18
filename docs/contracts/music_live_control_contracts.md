# DLS live voice controls

M2 and MUSIC remain incomplete. Prepared grid events can now be decoded into
supported controller values, and those values can be applied to an explicit live
DLS voice. The [immediate receiver](music_midi_receiver_contracts.md) now supplies
channel dispatch. Timed transport remains separate; manual diagnostic control is not a running original soundtrack.

## Original behavior and replacement synthesis

`apply_dls_controller` accepts CC7 volume, CC10 pan, CC11 expression and pitch bend.
The caller selects the destination state; the low status nibble does not route a
message to a voice. Unsupported controllers, pressure, notes and program changes
fail without changing that state. These are not silent successful no-ops. CC values
above 127 fail instead of permitting the original table's unsafe indexing.

The installed MSynth25 dispatch uses raw unsigned data bytes. Preserve its pitch
combination `(second << 7) | first`, including values above the usual 14-bit MIDI
range. The bounded field accepts 0..32767, not arbitrary integers; normal wheel
controls use 0..16383. The original default is 8192 with a 200-cent range. Its
pitch contribution is `floor((bend - 8192) * 200 / 8192)`: 8191 becomes -1 cent,
8192 is zero, 16383 becomes +199 cents, and raw 32767 becomes +599 cents.
RPN range/tuning changes, modulation and pressure remain unsupported.

A read-only isolated executable probe verifies the original controller branches,
raw pitch decoding, default initialization and all 32768 pitch values. Queue
insertion is intercepted. It does not exercise timestamp-list interpolation,
synthesis, native DLL initialization or OS APIs. The replacement retains the
previous DLS squared volume/expression and square-root pan law; it does not claim
bit-identical retail PCM or the old synth's gain/pan table quantization.

## Mutation, clocks and lifetime

`DlsSampler::set_controllers` requires a live monotonic voice ID and validated
values. The existing conservative eight-octave total pitch bound includes the
current bend as well as the note, pitch envelope and LFO. Invalid/stale IDs and
out-of-budget changes fail atomically. Every failure updates separate control
refusal accounting, without changing notes or note-start refusal counters.

Each voice retains an immutable nominal gain (sample attenuation, velocity and
caller gain). Live gain is recomputed from that base; restoring volume/expression
after silence never divides by zero, accumulates rounding drift or unmutes an
intrinsically silent sample. Pan includes the original constant articulation
before clamping. Pitch bend multiplies the existing sample step; changing it does
not reset sample cursor, loop count, envelopes, LFO phase, elapsed frames or gate.
Changing controls during release cannot reopen the note. Other voices are untouched.

`DlsVoice::controllers` describes current values. A routed voice's `channel` keeps
its original bank/program/controller snapshot for provenance, not a mutable alias
of the catalogue. Existing immutable bank ownership, non-reused IDs, release and
world clear remain unchanged. Updates allocate no voices or per-voice resources.
Control counters/errors reset with the sampler on world unload.

Updates take effect for subsequent samples at the current AudioMixer boundary.
They do not modify already queued SDL PCM. Rendering/hidden advances share the
same state; UI drawing never advances time. The immediate receiver now maps channels
to owned voice identities. Future transport must apply events at their deadlines, rather than apply future grid events immediately
or infer a channel from matching instrument values.

## Inspection and tests

World sounds shows current controls, raw bend/cents, pitch multiplier and separate
update/refusal counts. Live voice sliders are explicitly manual diagnostics;
ordinary region/band audition still creates the same one-second note. Failed
updates remain visible. The NVIDIA smoke decodes control messages, mutes/pans/bends
a routed voice, advances the existing output clock, restores its initial controls
and verifies release across hidden-inspector and inspector-free world reloads.

Headless tests cover post-silence PCM restoration, sustained and release clocks,
peer isolation, invalid state, pitch bounds, raw byte behavior, stale IDs and the
original pitch fingerprint. Private tests apply labelled synthetic controls to
all 38 Cabal regions and compare rendered/hidden clocks. This is not an authored
song timing test or a listening comparison. See the
[evidence](../../../Documentation/Implementation/m2_music_live_controls.md) and
[source ledger](../implementation/sources/m2_music_live_controls_sources.json).
Immediate channel dispatch and same-key selection are extended in the receiver
contracts. Nonzero key groups, timed delivery, playlists and actual MUSIC acceptance
remain open.
