# DLS diagnostic sample voices

M2 remains incomplete. This voice engine is a prerequisite for original music,
not a MIDI sequencer or a successful `MUSIC` service. Read the
[bank contracts](music_dls_contracts.md), [music catalog](music_asset_contracts.md),
[audio ownership](world_sound_contracts.md),
[source ledger](../implementation/sources/m2_music_sampler_sources.json) and
[verification record](../implementation/reports/m2_music_sampler.md).

## Selection and synthesis

`DlsSampler::play` takes an immutable decoded bank, instrument index, key,
nonzero velocity, finite gate duration and gain. Exactly one matching key/velocity
region is required. It uses that region's already-resolved sample settings, never
an invalid overridden wave default. The present region articulation replaces the
global graph; only an absent graph falls back. An explicitly empty local graph
selects defaults. This rule follows Microsoft's later `DMUS_REGION` documentation;
its use for these older assets is a declared compatibility adaptation, not a trace
of the 1998 executable. No instrument/drum bank bit is rewritten.

The independent implementation supports constant amplitude/pitch ADSR parameters,
velocity-to-attack and key-to-decay scaling, pitch-envelope depth, constant pan,
LFO frequency/delay and direct LFO pitch/gain depths. Constant pan supports both
standard destination 4 (signed 16.16, in 0.1-percent units) and the installed
MSynth25 legacy destination 2 (signed integer, in the same units). Both require
source/control/transform zero. They overwrite the same parameter in authored
connection order; they are not added or unconditionally given standard precedence.
Every individual pan value must lie in -500..500 before assignment, including a
value subsequently overwritten. Identical-edge duplicates still refuse the note.
The explicit DLS1 default velocity edge (2,0,1,1,-960*65536), used by Track1,
selects the existing squared velocity curve exactly once. Other connections,
transforms, scales on that edge and control sources refuse the complete note.
The bank reader preserves raw records, including both pan forms.

The legacy interpretation is supported by a bounded isolated execution of the
installed MSynth25 constant-pan dispatch. See the
[compatibility evidence](../implementation/reports/m2_music_legacy_pan.md)
and [source ledger](../implementation/sources/m2_music_legacy_pan_sources.json).
This is not a whole-synth/retail playback comparison. The replacement keeps its
existing full-precision DLS pan equation: it does not reproduce the old synth's
internal truncation to a 1/128 offset, which maps both integer and fixed-point
representations to the same stored value. Unknown legacy destinations such as
0x208 remain unsupported; one identified alias is not permission to ignore others.

The implementation follows DLS Level 1 with its 1.1 errata: signed fixed-point
centibel gain, fractional timecents, absorbing absolute-zero time, `/128` time
scaling, linear amplitude attack and full-scale logarithmic decay/release rates.
Sustain is an envelope percentage, not a direct amplitude multiplier. Release
starts at the actual current envelope level, including during attack. Zero-time
attack starts at peak on the first sample. The pitch envelope operates in cents.

Pitch combines key minus unity note, signed fine tuning, pitch envelope and LFO.
Resampling is linear at 48 kHz. Forward loops wrap within their authored interval,
including the interpolation neighbor at the seam, and continue through release;
post-loop tail samples are not substituted. One-shot samples end at their actual
frame count. The sine LFO starts at zero pitch bias after its delay; the unipolar
gain path ramps its depth over the first cycle to avoid a start-bias step.

Direct diagnostic notes default to volume 100, expression 127 and pan 64.
[Band-routed notes](music_routing_contracts.md) now capture supplied CC7/CC11/CC10
values. [Explicit live controls](music_live_control_contracts.md) add volume, expression,
pan and original default-range pitch bend; modulation remains unavailable. Velocity, volume and expression use the
squared gain law; stereo pan uses the DLS square-root equal-power law. Constant
pan adds to `CC10/127` (default `64/127`), so the default is slightly right of the exact mathematical
center. The [immediate MIDI receiver](music_midi_receiver_contracts.md) adds channel updates
and original same-key stop/repeat selection. [Timed delivery](music_timing_contracts.md)
adds explicit sample-boundary scheduling; original musical transport remains absent.
The UI's additional 20% gain is an audition setting, not a game music-volume rule.

## Ownership, work bounds and inspection

Checked live updates retain voice clocks and gates; their current controller values
are separate from the original routed note-on snapshot. The mixer owns up to 32 voices. Every voice retains the immutable bank until its
sample/envelope finishes or the world is cleared. IDs are never reused within a
sampler, including after clear; they are local to that mixer, not engine handles.
A failure allocates no voice and changes no existing note. A received note whose instrument has no
region for its key (the DLS Level 1 rule), or whose bank/program names no instrument (a replacement
policy), is silent and counted, not a failure; see
[silent notes during live delivery](music_routing_contracts.md). That supersedes the missing-key and
missing-program failures for received notes only; the original synth's fallback (General MIDI or
silence) is unconfirmed. Explicit-ID note-off
only shortens the gate; stale IDs fail. The immediate receiver owns 16 fixed channels
and held note gates, but there is no voice stealing or dynamic channel allocator.

Diagnostic gate duration is at most 10 seconds; receiver notes remain held until
stop or natural completion. Release is at most 60 seconds (Track1 authors a roughly 40-second release), attack at most
20 seconds and decay at most 40 seconds. Total possible pitch modulation is bounded
to eight octaves in either direction. Phase-linked/stereo sample records, exclusive
key groups and overlapping regions refuse playback. The selected graph's legal
parameter ranges are checked before publication. The public API requires banks
produced by the checked reader and kept immutable, not arbitrary mutable PCM.

All voice clocks advance on the existing AudioMixer sample clock. Rendering adds
to the current stereo block before the existing final clamp and SDL submission.
Headless/hidden-inspector updates and dropped output prefixes advance the same
oscillator/envelope state. No new output device, audio callback, RNG or file read
is introduced. The 250 ms rendered-prefix limit is retained; muted frames still
advance bounded voice state. Output transfer moves only the SDL device; old-world
notes remain with the old mixer and are cleared on unload. The existing unload
clears queued PCM and releases banks independently of inspector visibility.

World sounds exposes note IDs, instrument/region, key/velocity, cursor, loops,
age, envelope parameters, LFO frequency, effective constant pan, its last-writing
destination, presence of a legacy pan record, stereo gains, start/completion/refusal counts and the
last failure. The region audition button starts a one-second midpoint-key note;
Release shortens that diagnostic gate. Drawing never advances or reloads audio.
Counters clear with their world; they are not inferred from visible widgets.

## Explicit gaps

All 38 Cabal regions now produce finite nonzero PCM at midpoint key/velocity 100;
five use the legacy pan alias. This supersedes the earlier 33 accepted/five refused
sampler evidence, not its historical results. Full MIDI state/key-group exclusivity, timed authored note events,
styles/personality semantics, original playlists/transitions, reverb and retail
listening comparisons remain absent. Do not turn `MUSIC` into success on the strength of these auditions.
The [MSynth25 loop rules](music_dls_contracts.md#installed-synth-loop-rules) supersede the
Crudux/Track1 effective-loop refusal: a region loop may now end on the guard frame one past
its wave, which plays the loop start sample before wrapping; loops may not end further out.

The [session-cost continuation](session_cost_contracts.md) removes redundant work while preserving these ownership, timing and inspection rules.
