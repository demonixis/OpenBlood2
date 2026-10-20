# M2 — original level choices and finite playlist audio

M2 remains **incomplete**. The inspector can now audition the original transition
choice followed by one ordered pass through the requested authored playlist.
Playback continues on the existing audio clock while the inspector is hidden.
The original request's looping/boundary flags are visible but are not yet executed
as game `MUSIC` commands or live playlist switching.

Implementation commit: `a6ca134dd0e2e0e1849f4d2e99ea780db43fdd1b`.

See [contracts](../../contracts/music_playlist_contracts.md) and
[source ledger](../sources/m2_music_playlist_sources.json).

## Original behavior and implementation

The unchanged original `PlayMusicLevel` and `TransitionToLevel` decision methods
were compiled in isolation with a recording PlayList observer. Their 65536 byte
pairs match the new pure selector: 13 produce requests. The [reference probe](evidence/m2-ima-playlist-probe.py)
records source/compiler identity and decision FNV-1a64 `17078631686421749893`.
Repeated nonzero levels make no request; silence to silence does. The original
Harddriving-to-silence transition is cts.sec, even though hts.sec exists externally.
Logical-level mutation before original engine failure is documented but not run
by this stateless selector.

The original request plan supplies transition/list order; unique-resource caching
never removes authored duplicate occurrences. Case-folded exact filenames resolve
to their existing immutable binding. No playlist resource is reread during play.
The selected bank must be the same throughout this bounded audition.

Sections meet at their musical durations. A shared tempo map converts all events,
including anticipations and note tails, without adding envelope-length gaps or
rounding each section into an independent sample origin. One global preroll keeps
early events. Preparation shares a 16-million traversal/draw budget across the
list, with 64-section and 65536-message caps. Missing late entries, unsupported
bindings, overflow or capacity errors refuse the whole audition atomically.

Finite composition, setup timing and fresh seed-1 cursors per section are explicit
replacement audition policies. They do not establish original activation/lookahead,
repeat-state or listening parity. Actual MIDI delivery failures remain distinct
from successful preparation. Cancellation and reload retain the existing clock
and immutable-bank ownership rules.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 148/148 passed | 33.36 s |
| release-tests | 148/148 passed | 6.57 s |
| sanitize-tests | 148/148 passed | 131.68 s |
| debug-gpu | 2/2 passed | 25.76 s |
| release-gpu | 2/2 passed | 9.54 s |

The [validation ledger](evidence/m2-ima-playlist.json) records frozen implementation
inputs, commands, complete logs and exact private accounting. All 13 accepted
level choices play against external Cabal data using explicitly constructed test
lists, including an authored duplicate with differing filename case. The tests
require every scheduled note to start, no delivery failures, all notes released
and eventual empty voices/queue. A complete Harddriving audition renders finite,
nonzero PCM. Debug, Release and Clang sanitizer totals agree.

Synthetic composition verifies exact overlapping-tail/anticipated-note positions,
a shared two-tempo fractional boundary and sample-for-sample block independence.
Invalid durations/tempos, message/section/tick limits and the exact shared-work
boundary fail atomically. A missing late list entry preserves the previous report
and idle sampler. Queue-held bank lifetime survives catalogue destruction and ends
on cancellation. Empty lists and unavailable bindings remain explicit failures.

The NVIDIA sound smoke uses the actual SubwayA HarddrivingList, preceded by sth.sec,
and checks full completion through the ordinary mixer. It repeats hidden and after
world reload; the new report must start empty. The existing player keep-alive GPU
scenario passes too, in Debug and inspector-free Release. Capture inspection shows
the actual finite playback report. UI actions were invoked through their backend,
not manually clicked. SDL dummy output and nonzero PCM do not establish listening
parity or native macOS support.

Final builds emit no compiler warnings. Formatting, owned whitespace and local
Markdown targets pass. All 10923 reference files and six reference repositories
remain unchanged; the user's root README is preserved.

## Remaining scope

Original game command delivery, client logical-level lifecycle, repeating transport,
active NEXTSONG changes, cross-section cursor/variation state and game music volume
remain. Other DLS bank limitations and native macOS/Windows/mobile verification are
unchanged. Required MUSIC calls still fail explicitly and the M2 gate is unchanged.
The user's root README is preserved outside these commits.
