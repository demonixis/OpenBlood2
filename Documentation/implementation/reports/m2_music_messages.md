# M2 — Original MUSIC delivery and live playlist changes

The original player MUSIC path now drives actual IMA/DLS audio. Selected server
command parsing, owned local SMSG_MUSIC equivalents and client level decisions are
connected to the existing repeat transport. Fresh world entry starts SilenceList;
authored MUSIC LEVEL commands select the original transition and target playlist.
M2 remains incomplete.

The engine now stages one replacement, switches immediately or at the end of the
current section, cancels the old instrument events/voices at that sample, and
continues on the same clock. A waiting request can be superseded. Rejected
preparation preserves the current and pending streams. Receiver failures stop both
and remain inspectable. WAV playback, world lifetime and rendering stay independent.

The inspector separates the original logical level from the current and pending
stream levels, and displays the exact boundary, delivered/admitted/refused/failed
requests, source no-op commands, committed switches and cancellation accounting.
The continuous SubwayA route now asserts that its authored MUSIC LEVEL 3 starts
playback instead of counting it as unsupported.

## Source and adaptation boundary

See [contracts](../../contracts/music_message_contracts.md) and
[source provenance](../sources/m2_music_message_sources.json).
Original code supplies literal-space parsing, atoi byte semantics, trigger dispatch,
client transition choices, mutation-before-engine-result and shell entry/exit rules.
The supplied original motif/break client methods contain no engine calls; they are
explicit no-ops rather than fake successful sound services.

The portable mixer implements NEXTSONG at musical section boundaries with a single
pending request. Latest accepted request wins; old tails are cut at the switch,
and the new prepared global preroll starts there. These are documented replacement
engine policies, not established retail activation behavior. The existing 20% gain,
per-section seed and repeated prepared variations remain visible limits. Missing or
unsupported banks fail; no required resource is replaced with silent success.

## Validation

Implementation commit: `309ae214afc9f53051076a791138a59da7731644`.

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 188/188 passed | 96.94 s |
| release-tests | 188/188 passed | 20.85 s |
| sanitize-tests | 188/188 passed | 460.56 s |
| debug-gpu | 2/2 passed | 53.30 s |
| release-gpu | 2/2 passed | 20.98 s |

The [validation ledger](evidence/m2-music-live.json) retains commands, source hashes,
complete build/test logs and private counters. Debug, inspector-free Release and
Clang ASan/UBSan agree on the new private and synthetic results. The selected suites
cover music preparation/clock/receiver/transport, object lifetime, world loading,
player entry/restore/keep-alive, voice/fade messages, service coverage and the
continuous train route at 30 and 60 Hz.

Synthetic switching compares exact PCM across 5,001-frame blocks and 17-frame
fragments, verifies forty next-song boundaries, checks the sample immediately
before and on the switch, and covers supersession, invalid replacement, pending
bank lifetime, stalled audio, receiver failure and bypassed deadlines. Message
checks cover parsing, byte wrap, bounds, ownership, dead-player delivery, logical
mutation despite unavailable assets, source no-ops and disconnect cleanup.

Private SubwayA messages exercise actual external Cabal playback, NEXTSONG and an
immediate command while a Silence change is still pending. Clean CityHub entry
starts its own SilenceList. The ordinary train traversal now requires successful
authored MUSIC LEVEL 3 delivery/playback; gameplay input/combat/contact/exit checks
remain intact. This is not a proof of all musical activation or variation paths.

NVIDIA RTX 3060 Laptop Vulkan checks pass with inspection hidden and after reload,
leaving active music, a pending replacement and a future MIDI message for teardown.
Player keep-alive graphics checks pass too. The screenshot was visually inspected;
controls use their normal backend rather than manual UI clicks. SDL dummy output
is not a listening comparison, and this run does not add native Metal evidence.

All 22 changed C++ files pass the formatter. Local Markdown links and owned
whitespace pass. All 10923 reference files and six reference repositories remain
unchanged. The user's root README is byte-identical. Existing float/enum warnings
in the player voice, item pickup and player SOUND paths remain visible in the logs;
none are suppressed. Final results follow a rebuild after review edits; initial
build/test logs are retained separately. The initial Debug/ASan switching check
used an object file built before the bypass guard was added; rebuilding on the
frozen source removes that mismatch. Initial NVIDIA checks also exposed obsolete
assumptions of an idle/zero-counter instrument stream: the authored startup trigger
already delivers MUSIC LEVEL 3 after SilenceList. The final smoke requires both
real startup requests, exactly 4,358 admitted MIDI inputs on the fresh clock,
then checks the unchanged diagnostic message deltas. No duration or behavioral
assertion was dropped to admit the live stream.

## Remaining work

Original cross-section random/variation continuity, detailed native activation and
band lookahead, configurable music volume and saved client music state remain open.
The current selected music path is real playback, but it does not establish full
retail soundtrack parity, all-bank support or the complete M2 acceptance gate.
The roadmap's goal, work and exit requirements remain unchanged.

The next milestone check should consolidate the selected session's class/service
coverage, original shell entry/exit, malformed-input and sanitizer evidence against
that gate. Remaining soundtrack fidelity work alone is not evidence that every
M2 criterion is unmet, nor does this increment establish that every criterion passes.
