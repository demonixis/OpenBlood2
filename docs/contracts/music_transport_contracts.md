# Repeating IMA sequence transport

`ImaLoopTransport` plays a prepared introduction once and then repeats an authored
playlist suffix through `AudioMixer`. It advances while the inspector is hidden.
This supplies bounded, continuous audio; it does not yet handle game `MUSIC`
messages, live `NEXTSONG` replacement or original variation regeneration. M2 stays
incomplete.

Read [finite playlists](music_playlist_contracts.md), [clock](music_clock_contracts.md),
[timed MIDI](music_timing_contracts.md), [evidence](../../../Documentation/Implementation/m2_music_transport.md)
and the [source ledger](../implementation/sources/m2_music_transport_sources.json).

## Original basis and replacement policy

The original `ClientDE::PlayList` contract specifies ordered songs, an optional
transition before the list, and repetition of the list. The existing original
`CMusic::SelectLevel` chooses the transition/list. The transport omits that
introduction on subsequent passes and preserves authored duplicates and order.
Musical section durations place successors; neither late note-offs nor release
envelopes extend the list. Anticipated notes and late stops cross repeat boundaries.

The prepared events still use the explicitly documented per-section seed 1,
random-channel mask, setup placement and 20% audition gain. Repetition reuses those
prepared variations. It is a replacement transport with verified timing, not proof
of retail random-state/activation behavior or listening parity. The original game
logical level remains unchanged. No success is returned for an unsupported game
MUSIC service.

## Bounded scheduling and clock continuity

Preparation owns two finite numeric templates: introduction plus first pass, and
the repeating suffix. Their source preparation retains the existing 64-section,
65,536-message and aggregate 16-million-work limits. The suffix owns no source
pointers; the transport retains one immutable DLS bank until stop/unload.

The mixer pumps before advancing the sampler through each block, including its
unrendered portion after stalls. A whole subsequent pass is submitted as soon as
its earliest anticipated event falls within that block. Queue admission retains
source submission order and remains atomic. A call permits at most 128 pass
batches and 65,536 input messages, with the existing retained queue bound. A
malformed or excessively dense sequence fails explicitly; it is never truncated.
Idle frames do not allocate event storage. Memory does not grow with elapsed loops.

Two fixed tempo maps represent the prefix and repeating period. Each repeat
boundary carries **truncated 100-nanosecond time**, before the original millisecond
event rounding. `ImaClock::time_at` obtains that boundary time; `sample` accepts an
explicit checked retained offset. It does not multiply rounded sample periods.
Early notes consult the preceding map and late notes the actual future repeating
map. The same authored tempo boundary is retained even when adjacent tempos match.
Signed event-time limits and origin/period overflow remain explicit failures.

`passes_submitted` counts admissions, not completed or audible passes. The next
submission frame is the earliest event of the next pass, not a section boundary.
`sample` inspects the repeated clock without progressing anything.

## Failure, stop and world ownership

Starting requires an idle sampler and inactive repeat transport. Failure preserves
the previous successful report and audio state. `pump` stages complete batches
before queue admission and commits its pass counter only after success. It never
rewinds already delivered audio.

A mixer scheduling or MIDI delivery failure stops the transport, clears only
instrument voices/queued events, advances the shared sample clock and discards the
failed block's PCM. The reason persists in `AudioMixer::music_error`; unrelated WAV
voices are not cancelled. Subsequent normal audio steps may continue. A successful
new repeating start clears this transport error; world clear resets it together
with all templates, bank ownership, pending notes and clocks.

The inspector stop action also stops repetition before cancelling instruments.
It preserves the shared sample clock and accounting. Drawing never admits events
or regenerates a sequence. New finite auditions must not be started over a running
repeat through the inspector. Manual MIDI/instrument controls remain explicit
diagnostic overrides, not live music switches. Audio-device transfer does not
transfer the old world's music state.

## Verification boundaries

Tests compare the compact repeated map with an independently unrolled multi-tempo
map, then compare exact PCM against a fully queued unrolled sequence and different
block sizes. Coverage includes anticipated setup, tails across multiple sections,
transition-once ordering, zero steps, exact boundaries, invalid starts, atomic pump
refusal, bounded work failure, synthesis failure, cancellation and bank lifetime.
External Cabal tests exercise all four lists, duplicate entries, multiple passes,
long headless continuation and catalogue destruction. NVIDIA checks exercise the
ordinary mixer with the inspector hidden and after world reload, leaving active
music for teardown. These checks do not establish the remaining original live
switch/variation semantics or native Metal playback.
