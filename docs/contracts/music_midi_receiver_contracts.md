# Immediate DLS MIDI receiver

M2 and MUSIC remain incomplete. `DlsSampler::receive` applies explicitly delivered
messages at the current audio sample boundary. It does not submit future grid
messages early, run a playlist, or reinterpret IMA ticks as sample times.
See [live controls](music_live_control_contracts.md),
[routing](music_routing_contracts.md), [sampler](music_sampler_contracts.md),
[evidence](../../../Documentation/Implementation/m2_music_midi_receiver.md) and
[source ledger](../implementation/sources/m2_music_midi_receiver_sources.json).

## Channel and note ownership

The receiver has 16 value-owned channels, addressed by the MIDI status low nibble.
Channel 9 is percussion. Current default bank/program and controllers match the
existing diagnostic snapshots; original IMA band setup supplies the actual authored
values through its existing ordered 80 messages. Arbitrary original channel aliases,
percussion remapping, download residency and OS GM fallback are not implemented.

Supported messages are note-on, note-off (including note-on velocity zero), bank
MSB/LSB, program, CC7/10/11 and raw pitch bend. Ordinary data bytes must be 0..127;
program's unused second byte is ignored and the original raw pitch combination is
retained. Sustain, modulation, pressure, RPN, system messages and controller reset
fail explicitly. Bank/program changes affect future note lookup only. Controllers
update current and released voices in that channel, preserving immutable note-on
provenance. A diagnostic `play`/`play_channel` voice has no receiver slot and cannot
be selected by MIDI messages even when its patch/key values happen to match.

Note-on resolves the supplied immutable bank through the existing checked lookup.
It holds the note until a matching stop, sample exhaustion, envelope completion,
replacement, explicit diagnostic release or world clear. It has no invented one-
second/ten-second cutoff. Only note-on requires a bank; other messages require no
file access. The sampler owns every active bank alias; no pointers into a voice
vector or mutable catalogue are retained. IDs stay monotonic across clear.

## Original stop and repeat rules

The original note-off scans its head-inserted list and stops only the newest held
matching channel/key. Released notes are skipped; an unmatched stop is an accepted,
separately counted no-op. Stop on the start boundary is clamped to the next sample.
This receiver has no sustain state because sustain messages are rejected.

A newly selected region with `SELFNONEXCLUSIVE` (options bit 0) allows same-key
voices to overlap. Otherwise a new note fast-releases all previous voices of that
channel/key, including ordinary release tails and voices from another program.
The old region's options do not override the new region's choice. Already fast-
released voices keep their existing bounded endpoint rather than restarting it.
Unsupported option bits and nonzero exclusive key groups are refused explicitly.

Original isolated execution checks target selection and the fast-release integer
cap `rate / 70`. The replacement rebases its existing full-precision logarithmic
amplitude envelope continuously, capping its remaining duration at 685 frames at
48 kHz. Pitch, sample position and loop clocks continue. This is not bit-identical
retail PCM or the original quantized envelope implementation. Voice stealing,
mono mode and nonzero key-group behavior remain unavailable.

## Atomicity, limits and inspection

A note is fully validated and allocated before any old note is fast-released.
Failure leaves all old voices and channel values unchanged. The existing 32-voice
limit also counts release tails; exhaustion refuses the note instead of stealing
or silently cutting off a voice. Controller fan-out preflights every affected
voice's pitch budget before changing any voice or channel. Each receive failure
has separate accounting; note-creation refusals also retain the sampler's existing
refusal history. Successful commands clear the last receiver error.

World clear resets channels, counters and errors and drops all banks. Existing
AudioMixer timing/output ownership stays unchanged. Hidden and rendered updates
advance the same state. Drawing does not generate events automatically or advance
music clocks. Per-voice manual controls remain diagnostic overrides; the next
channel controller reapplies the complete channel state to all its voices.

The sound inspector shows current channel bank/program/controllers, receiver-slot
identity, held/released/fast-release state, accepted/refused messages, stops and
unmatched stops. The selected-band button explicitly sends that source channel's
five setup messages and starts a held diagnostic note. A note-off button addresses
its channel/key and therefore follows the newest-held rule; direct Release still
addresses an individual voice ID. Diagnostic gain remains 20%, not game volume.

Tests cover channel isolation, current release-tail controls, unchanged note-on
snapshots, newest-held ordering, self-nonexclusive overlap, cross-program fast
stops, long holds, start-boundary stops, limits, atomic fan-out, bank ownership and
clear/reuse. Private sequences exercise all 38 Cabal regions with actual finite
PCM and identical hidden clocks. NVIDIA smoke sends the selected band's 80
messages, starts overlapping notes/stops the newest then the oldest, and verifies actual output and cleanup
with the inspector hidden and after reload. These are labelled diagnostics, not
scheduled soundtrack or listening-equivalence tests.
