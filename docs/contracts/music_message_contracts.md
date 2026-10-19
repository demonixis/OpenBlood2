# Original MUSIC messages and live playlist replacement

Original player `MUSIC` triggers now reach the client music helper and the real
instrument stream. Read [playlist selection](music_playlist_contracts.md),
[repeat transport](music_transport_contracts.md), [player delivery](player_contracts.md),
[evidence](../../../Documentation/Implementation/m2_music_messages.md) and the
[source ledger](../implementation/sources/m2_music_message_sources.json).
M2 remains incomplete; prepared variation repetition is still a documented limit.

## Source rules and typed ownership

`CPlayerObj::ProcessTriggerMsg` releases its incoming HSTRING once, then passes the
owned suffix to the selected original server `CMusic::HandleMusicMessage` logic.
Commands and arguments split on literal spaces only. Command names compare without
case. LEVEL without a number means zero; numeric conversion preserves the decimal
prefix, optional sign, C whitespace and byte wrap of defined 32-bit `atoi` inputs.
Non-numeric input becomes zero. Overflow fails explicitly instead of invoking the
source's undefined behavior. Extra tokens are ignored. MOTIF/BREAK require a name;
the adaptation rejects names of 50 or more bytes instead of risking the original
client's unterminated 50-byte copy. Embedded NUL and overlong triggers fail.

The existing bounded local event queue owns a `MusicCommand` value. No borrowed
string, player pointer, callback reader or source handle survives delivery. All
byte levels reach the client; values outside 0..3 are original client refusals.
Commands have no dead-player guard. Invalid typed messages are refused at queue
admission. Disconnect clears queued messages before another player can enter.
Delivery is counted separately from playback. A missing music resource records a
client error without ejecting the player or cancelling gameplay callbacks.

Client `PlayMusicLevel` retains the verified original transition matrix, including
Harddriving -> Silence using **cts.sec**. A repeated nonzero level makes no request.
Silence -> Silence requests SilenceList at NEXTSONG. Other accepted changes assign
the logical level **before** the fallible engine request. Silence -> nonzero requests
IMMEDIATE, even if an earlier Silence request is still waiting and another level
is audible. Failed preparation does not restore the old logical level. The client
tracks attempted, admitted, refused and failed requests independently.

The supplied original client PlayMotif, StopMotif and PlayBreak bodies make no
engine call. Their messages therefore increment a clearly labelled source-no-op
counter; they do not create fake sound or claim an implemented motif engine.
CD audio and enable/disable settings are outside this IMA-only path. Unavailable
assets fail explicitly; the original disabled/uninitialized-success shortcut is
not used to disguise a missing service.

## Replacement engine timing

The client uses the same bounded, immutable section preparation as the inspector:
original list order and duplicates, one transition before the list, one DLS bank,
64 sections, 65,536 prepared messages and 16 million preparation work units.
There is no file loading or cursor preparation in ordinary audio steps.

An IMMEDIATE request replaces the instrument stream at its current sample clock.
NEXTSONG chooses the next **musical section end**, including an introduction in
progress. It does not wait for the entire playlist or for a release tail. A request
exactly on a boundary selects the next section's end. An idle stream starts now.
The repeated tempo map is searched in bounded logarithmic time, then at most 64
part boundaries are inspected; elapsed loops never grow retained state.

One pending prepared replacement is retained. The latest accepted request replaces
it; malformed preparation/admission preserves both the running stream and the
previous pending request. New messages are preflighted before cancelling anything.
After admission, old queued events dated at or after the switch frame are removed
once; later admissions omit those events. The mixer splits its instrument step at
the exact switch frame, silences old voices/tails, and starts the new transition/list
without resetting the shared sample clock. WAV voices are
untouched. Events before the boundary retain their established source ordering.
A large stalled frame still processes the boundary and all elapsed musical time;
the existing 250 ms PCM work bound remains unchanged. Drawing never advances music.

This is an explicit **replacement-engine policy**, informed by the original
PlayList boundary interface. Latest-request replacement, hard tail cancellation
and placement of the new preparation's preroll are not verified retail IMUSIC
activation semantics. The first new event starts at the boundary; its musical
zero follows the prepared global preroll. Sections still use seed 1 and mask
0xffff and repeats reuse prepared variations. Live requests render at full synth
gain; only inspector auditions keep the labelled 20%. The player's music volume is
applied by the mixer (see the menu contracts).
No retail PCM/listening or cross-section variation-continuity claim is made.

A scheduling or receiver failure stops the active and pending transport, releases
bank ownership, advances the complete audio step once and retains a durable error.
A caller bypassing the mixer past a pending deadline fails explicitly. Stop also
cancels pending replacements. Counters distinguish committed switches, submitted
passes and discarded queued events; none are assertions of audible completion.

## World lifetime and inspection

Original client world entry rebuilds music state and requests SilenceList. Exit
stops immediately and resets the logical level. Fresh and NEW_LEVEL destinations
own separate catalogues, clocks and pending state; rejected level changes preserve
the source session. Full saved client music state is not serialized/restored by
this increment; existing player-only restoration does not claim a whole-game save.

The World sounds inspector shows logical level, active stream level, pending level
and exact sample deadline, delivered commands, source no-ops, admission/failure
counts, committed switches, cancelled MIDI and actual audio errors. Diagnostic
preview selectors remain separate from the player's logical state. Diagnostic
stop intentionally interrupts this shared instrument stream.

Synthetic tests verify parsing, queue ownership, failure-after-logical-mutation,
exact next-song boundaries, immediate supersession, pending-bank lifetime, receiver
failure, teardown and exact PCM equality across different block sizes. Private
SubwayA tests use actual player messages and external Cabal data, then check clean
CityHub entry. The continuous train route now requires its authored MUSIC LEVEL 3
to reach active playback with no music failure instead of accepting an unsupported
trigger. NVIDIA sound checks exercise typed live switching hidden and after reload.

A world whose WorldProperties author no music (every music property absent or
empty, e.g. Drop and BB_DeathCanyon) inspects as `unauthored`. Level requests then
keep the original logical-level assignment, set a diagnostic error and count as
`unauthored` no-ops instead of failures, matching the original client's ignored
PlayList result after InitPlayLists fails. Startup is not refused for it. Any
authored but invalid or missing plan remains a counted failure that refuses
startup; a catalogue that was never inspected is not `unauthored`.
