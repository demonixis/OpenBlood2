# Timed DLS MIDI delivery

M2 and `MUSIC` remain incomplete. `DlsSampler::enqueue` accepts explicit absolute
48 kHz sample boundaries and `advance` delivers through the existing AudioMixer
clock/output. These are not IMA ticks. The later [IMA clock adapter](music_clock_contracts.md)
converts explicit musical ticks before queue admission. Neither API implements a
running section cursor, playlist, seek or game MUSIC activation.
Read [receiver contracts](music_midi_receiver_contracts.md),
[grid dispatch](music_grid_dispatch_contracts.md), [sampler](music_sampler_contracts.md),
[evidence](../implementation/reports/m2_music_timing.md) and
[source ledger](../implementation/sources/m2_music_timing_sources.json).

## Original order and bank semantics

Original MSynth25 inserts timestamped values in ascending order, retaining
submission order for equal timestamps. Its note queue extracts events with
`time <= horizon`; controller and patch lookup returns the last value with
`time <= query`. The portable queue therefore sorts stably, also across batches.
At each boundary it applies the last program and each supported control value for
each channel before delivering notes in their original relative order. Superseded
same-control values are counted, not briefly applied to sounding voices. Notes
submitted before a same-time controller/program still see that value. The receiver's
newest-held stop and same-key region replacement rules remain unchanged.

Bank CC0/32 are submission-time selectors in the original synth. They change a
separate pending bank latch immediately; a subsequent program captures those
selectors in submission order and publishes that patch at its timestamp. A bank
selector alone does not change the sounding patch. A future program submitted
before a later bank change retains its captured bank. Queued bank commands still
undergo the same timestamp/data validation as other inputs. Immediate diagnostic
`receive` retains its existing behavior and updates these latches when directly
receiving bank commands. Mixing manual overrides with a running queue is a
supported diagnostic operation, not a seek or replay mechanism.

## Clock, ownership and failure

`sample_clock()` is the sampler's current AudioMixer boundary. Enqueue never
advances it or creates a voice. Advance splits its existing rendered prefix at
actual event boundaries; all remaining unrendered frames and events also advance.
Events exactly at the end boundary are delivered before returning; their first
sample belongs to the next block. `advance(0)` can deliver current-boundary events
without progressing any oscillator. Already-delivered history is never rewound
when a later batch is submitted at the current boundary. Block-size invariance
applies to messages admitted before their boundary is processed.

A batch admits at most 65,536 input messages and the queue retains at most 65,536
non-bank messages. Time before the current boundary, unsupported commands,
invalid bytes/gain, missing note-on bank or capacity overflow rejects the entire
batch, preserving previous pending messages, bank latches, channels and voices.
The existing 60-second advance bound, 250-ms rendered prefix and checked integer
clock addition remain. This is a bounded sorted vector, with no callback, thread,
file reads, device or persistent per-message heap identity.

Only pending nonzero note-ons retain immutable reader-produced banks. Program
messages retain bank selectors by value; other messages need no asset. Dispatch
resolves the actual patch/key and current voices, so successful admission is not
successful synthesis. A missing patch, unsupported region or live pitch budget
failure consumes that message once, records its sample boundary/error and makes
that advance return false. Other due messages and the clock still progress;
there is no false rollback of rendered time. AudioMixer discards that block's PCM
and reports the error instead of submitting partial failed output. Later success
does not erase the queue's delivery-failure history. Existing per-receiver counters
still count actual receiver calls, not bank-latch submissions or superseded values.

World clear resets queue, sample clock, latches and accounting, dropping queued
banks as well as live voices. Voice IDs remain non-reused. Output-device transfer
does not move old-world scheduled events. No new event may survive world reload
through inspector-owned state.

## Inspection and validation

World sounds shows the current sample boundary, pending count, submission/refusal,
delivery/coalescing/failure counts, durable failed boundary, raw pending messages,
retained-bank presence and submission bank latches alongside current channel state.
The selected-section band diagnostic now uses its actual IMA tempo for a one-quarter
delay and one-quarter note duration. Drawing never advances this queue. The gain is
an explicitly labelled 20% audition value, not the game's music volume.

Tests cover out-of-order input, equal-time controls/programs, cross-batch note
order, program bank capture, exact start/stop/mute samples, fractional AudioMixer
steps, zero steps, dropped prefixes, bounds, atomic admission, failed delivery and
bank lifetime. All 38 Cabal regions compare exact replacement PCM across block
sizes and hidden clocks. The NVIDIA smoke schedules a synthetic phrase after the
actual selected-band setup; a far-future note remains queued for unload and must
be absent after reload. These are transport diagnostics, not original soundtrack
or listening-parity evidence. Unsupported receiver controllers, dynamic residency,
voice stealing, running section/playlist transport and MUSIC remain open.
