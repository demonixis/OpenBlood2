# DLS diagnostic sample voices

M2 remains incomplete. This voice engine is a prerequisite for original music,
not a MIDI sequencer or a successful `MUSIC` service. Read the
[bank contracts](music_dls_contracts.md), [music catalog](music_asset_contracts.md),
[audio ownership](world_sound_contracts.md),
[source ledger](../implementation/sources/m2_music_sampler_sources.json) and
[verification record](../../../Documentation/Implementation/m2_music_sampler.md).

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
LFO frequency/delay and direct LFO pitch/gain depths. Other connections, transforms,
control sources and duplicate edges refuse the complete note. Unknown destination
2 is not guessed to be pan or silently discarded, even where the same installed
graph also contains standard destination 4. The bank reader retains these records.

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

Controller values are fixed for these explicit diagnostic notes: volume 100,
expression 127, pan 64, no modulation or pitch bend. Velocity and volume use the
squared gain law; stereo pan uses the DLS square-root equal-power law. Constant
pan adds to `64/127`, so the default is slightly right of the exact mathematical
center. No dynamic MIDI controller or same-key exclusivity behavior is claimed.
The UI's additional 20% gain is an audition setting, not a game music-volume rule.

## Ownership, work bounds and inspection

The mixer owns up to 32 voices. Every voice retains the immutable bank until its
sample/envelope finishes or the world is cleared. IDs are never reused within a
sampler, including after clear; they are local to that mixer, not engine handles.
A failure allocates no voice and changes no existing note. Note-off only shortens
the gate; stale IDs fail. There is no voice stealing or MIDI channel allocator.

Gate duration is at most 10 seconds, release at most 20 seconds, attack at most
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
age, envelope parameters, LFO frequency, start/completion/refusal counts and the
last failure. The region audition button starts a one-second midpoint-key note;
Release shortens that diagnostic gate. Drawing never advances or reloads audio.
Counters clear with their world; they are not inferred from visible widgets.

## Explicit gaps

Five Cabal regions currently refuse unknown articulation destination 2; 33 produce
nonzero PCM in private tests. Full MIDI state/exclusivity, authored note events,
styles/personality semantics, original playlists/transitions, reverb and retail
listening comparisons remain absent. Crudux/Track1 effective-loop failures remain
unchanged. Do not turn `MUSIC` into success on the strength of these auditions.
