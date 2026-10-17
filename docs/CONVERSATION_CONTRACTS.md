# Original conversation triggers and client communication

M2 remains incomplete. See [execution evidence](../../Documentation/Implementation/M2_CONVERSATIONS.md)
and [source selections](M2_CONVERSATION_SOURCES.json). This extends
[trigger](TRIGGER_CONTRACTS.md), [player](PLAYER_CONTRACTS.md),
[objective transport](OBJECTIVES_CONTRACTS.md) and [cinematic audio](CINEMATIC_CONTRACTS.md) contracts.

## Original server rules

`ConversationTrigger` derives from the original Trigger subset. It retains eight
WaveFile properties, CharacterType's low eight bits, RepeatLastMessage and
LoopToFirstMessage. Property loading precedes the base precreate callback and
only runs for world/string creation. The first empty wave ends the sequence;
subsequent properties are ignored. The source's count increment after a failed
property lookup is retained. Trigger filters, locks, activation counts, delays,
reset and deferred lifetime remain shared. Default touch/AI/object/relay activation
is false; player activation is true.

The immutable original filename/text table is imported completely. Lookup strips
only backslashes, compares case-insensitively and returns the first match. Forward
slashes deliberately do not behave as filename separators here. Resource lookup
still uses the engine's existing normalized paths. Unknown filenames, including
`default.wav`, produce an inspected failure without advancing the source index.
No new dialogue, guessed alias or resource substitution is supplied.

An accepted send copies file, text and character into the bounded local-client
queue. The source advances after the attempt even when that queue is full or no
client is available. RepeatLastMessage takes precedence over LoopToFirstMessage;
without either, exhaustion stops future sends. Delayed virtual SendMessage ignores
its target/slot argument and invokes this same sequence method, as in the source.
Owned values replace HSTRING/wire ownership. Files must be nonempty and shorter
than 200 bytes, text shorter than 500, with no embedded NULs. Queue capacity remains
256. This is single-player local delivery, not networking or save compatibility.

## Original client rules and actual audio

The SMSG_CONVERSATION branch invokes the numeric-character `CCommLink` path. A busy
communication discards the incoming message without replacing the current one;
the server does not rewind. Character 0–4 selects the original Caleb, Ophelia,
Ishmael, Gabriella and voice portrait path; other bytes select the unknown portrait.
Paths are metadata; the original PCX/font HUD is not rendered.

The tab uses strict `> 0.2` second entry/exit transitions. Its stopped duration is
`max(2.5, text_bytes / 15)`, with the original separate text scroll/hold phases.
Voice playback starts once, on completion of entry. Exact lowercase `default.wav`
suppresses playback. Other files use real buffered local WAV playback, original
misc-high priority intent, no loop and no reverb. The actual original helper call
has bHandle/bTime true and bStream false; a nearby source comment does not turn
this caller into streaming. Failed playback is inspected and is not retried.

The text-derived deadline kills the retained sound even if it is longer; a short
sound completing does not shorten that deadline. Voice handles are released on
replacement, exit or world unload. Trigger removal cannot destroy the owned queued
message or its later player-owned voice. Player removal and client exit clear all
communication state. No new mutable game global or shared table owner is added.

Timing moved from the source Draw method into the client simulation update so it
continues with Dear ImGui hidden or compiled out. This is an explicit adaptation.
The source's client VoiceMgr StopAll/SetNextPlayTime service is still unavailable:
inspection counts stop requests and retains the requested delay, without stopping
unrelated server voices or reporting successful arbitration. Original HUD drawing,
VoiceMgr arbitration, complete save/restore and safe NEW_LEVEL remain pending.

## Inspection and external-data evidence

The Cinematics tab shows client text/file/portrait, busy drops, phase ratios,
text duration, real voice attempts/failures and unavailable VoiceMgr requests.
It lists every conversation controller, including hidden/inactive ones, its
sequence/options, all eight resource/mapping slots and errors. TRIGGER queues
through the existing frame-boundary command path with the player as sender.
Reading inspection does not advance any timer or sound.

All seven authored ConversationTrigger entries in the 57 parseable base worlds
create, but all seven first cues are unmapped and absent from the installed
resource index (four default.wav placeholders and three historical names).
The existing malformed `16_bridge.dat` stays rejected. CityHub's authored
placeholder is preserved and tested as silent; creation does not certify dialogue.
A separately labelled diagnostic invokes original callbacks with the real installed
1ACaleb1 voice for playback/lifetime/GPU verification. It never patches world data.
CityHub is 434 created / 31 transferred sounds / 541 editor-only / 19 unsupported /
one failed, with 466 structurally checked / 19 unknown / zero invalid entries.
Required remaining services and missing fallengrills geometry keep M2 open.
