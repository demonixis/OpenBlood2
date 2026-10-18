# Original music level requests and finite playlist audition

`CMusic::SelectLevel` exposes the original client's playlist/transition choice.
`MusicCatalog::play_level_once` plays that transition followed by one authored
playlist pass through the existing sample queue. This is an explicit development
audition, **not** a successful game `MUSIC` service, running repeat transport or
live `NEXTSONG` switch. M2 remains incomplete.

Read [finite sections](music_program_contracts.md), [runtime clock](music_clock_contracts.md),
[timed MIDI](music_timing_contracts.md), [asset plan](music_asset_contracts.md),
[evidence](../../../Documentation/Implementation/m2_music_playlists.md) and
[source ledger](../implementation/sources/m2_music_playlist_sources.json).

## Original decision and authored ordering

The source's four levels are Silence, Ambient, Cruising and Harddriving (0..3).
The selected list always requests looping. Silence to a nonzero level requests
IMMEDIATE with sta/stc/sth; other accepted changes request NEXTSONG. Repeating a
nonzero level makes no engine request and returns false. Silence to silence does
request SilenceList, with no transition, at NEXTSONG. Harddriving to silence uses
**cts.sec**, not hts.sec. Invalid byte values make no request.

Selection is pure and returns owned scalar values plus static string views. It
must not be confused with original `TransitionToLevel`: that method changes its
logical level *before* checking initialization or the engine result. This increment
does not run that mutation or update any live client music level. An independently
compiled copy of the unchanged original decision methods matches all 65536 byte
pairs (13 accepted requests). Its recorded PlayList sink is only a test observer.

Audition uses the original asset plan's role-tagged request order, not the unique
asset cache order. It prepends the selected transition, preserves list duplicates
and resolves complete filenames with the existing ASCII case-folding rules.
An empty target list fails even when its transition exists. Missing/ambiguous
sections, unavailable exact style/curve/harmony/band bindings or differing bank
identities fail before any queue submission; no partial introduction is played.

## One finite timeline

`prepare_ima_playlist` composes prepared section programs. Successors start at the
sum of **musical section durations**, never at the latest note-off or after release
envelopes. Each section's local audition preroll is removed, then one global
preroll moves the earliest event to nonnegative time. Band setup is moved with its
section's earliest event so anticipated notes retain setup-before-note ordering.
Every section's original event submission order and terminal note dates survive.
Overlapping tails remain in the shared receiver and follow its channel/key rules.

The first tempo extends through global preroll. Each later authored tempo begins
at that section's musical start. All events are converted only after preparing
the complete shared tempo map, retaining the existing original clock's fractional
boundary arithmetic. Thus a tail crossing a tempo change uses the new map; earlier
anticipations use the preceding tempo. No independent per-section sample rounding,
extra silence or tail truncation is introduced.

This finite composition is a replacement engine policy based on documented list
order and the already verified section/clock services. It does **not** establish
original IMUSIC activation, band lookahead, cross-section variation continuity or
native listening parity. Each section deliberately starts a fresh local seed-1
cursor with random channels 0xffff, as the single-section audition did. The gain
is explicitly 20%. The requested loop/boundary is inspected but not executed.

## Bounds, admission and lifetime

Admit 1..64 prepared sections and at most 65536 combined MIDI input messages,
including every setup. Require positive section durations and checked signed tick
ranges. Catalogue preparation shares a conservative 16-million record/draw budget
across every section; a bounded section cannot multiply that budget by list length.
`ImaSectionProgram::work_units` reports consumed work so the next section receives
only the remainder. The existing per-section and MIDI queue limits still apply.

All preparation and conversion complete before one atomic queue admission.
A busy instrument sampler refuses the audition; it never replaces a running list.
Any failure preserves the previous successful report and all sampler state except
existing queue refusal diagnostics where applicable. Reports retain value-only
programs and filenames. Only the queue/voices retain the immutable bank after the
catalogue disappears. Stop remains explicit instrument-only cancellation without
clock reset; world unload releases the report and queue independently of the UI.
Single-section play clears the prior playlist report on success, and vice versa.

## Inspection and remaining work

World sounds offers explicit source/target level selectors and a one-pass audition.
It shows the original request's loop and IMMEDIATE/NEXTSONG flags separately from
the actual finite operation, ordered filenames, nominal start/duration ticks,
authored tempos, note counts, sample clock, cancellation and delivery failures.
Drawing never advances playback. Selection widgets are developer preferences;
no source-world object or live music state is retained by them.

Game command delivery, logical level changes, initialized state, repeat selection,
active-stream cancellation/transition timing, original cross-section state and
music volume remain separate work. Unsupported game MUSIC calls stay unsupported;
this increment does not relax the M2 exit gate.
