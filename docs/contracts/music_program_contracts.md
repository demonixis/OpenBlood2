# Finite IMA section playback

`prepare_ima_program` composes the existing original grid selection, variation,
harmonic notes, active-note lifetime, ordered curves and runtime tempo into one
bounded, finite section. `MusicCatalog::play_section` submits its events to the
ordinary DLS/audio sample queue. This plays actual authored music while inspection
is hidden; it does not select a game playlist or implement the required `MUSIC`
service. M2 remains incomplete.

Read [cursor](music_cursor_contracts.md), [ordered dispatch](music_grid_dispatch_contracts.md),
[active notes](music_active_note_contracts.md), [clock](music_clock_contracts.md),
[timed MIDI](music_timing_contracts.md), [evidence](../../../Documentation/Implementation/m2_music_program.md)
and [source ledger](../implementation/sources/m2_music_program_sources.json).

## Preparation and original end behavior

Prepare against the exact immutable section/style/timeline/harmony/band/curve
bindings already checked by the catalogue. Reject section-level opaque note or
curve chunks instead of silently dropping them. Preserve every grid's source
submission order and the cursor's chord-adjusted stops. At finite completion,
append the remaining active-note list in its current order, with each stored
note-off date unchanged. Original IMUSIC25 `17996..179c2` drains that list through
`4e49`; it does not cut tails to the section boundary or reverse the list again.
This branch was checked in isolated original-code execution. Repeat/next-section
branches remain outside this operation.

The selected band's 80 setup messages precede the grid stream. The program stores
at most 65536 messages including setup and 65536 grids. A conservative 16-million
combined record/draw budget further limits repeated source traversal, including
silent grids. The draw pool is sized to the largest grid plus sixteen channel
variation draws, capped at 4096 values. Preparation is synchronous and bounded;
no files or synthesis are performed
inside it. Output is cleared on any failure. The sampler is unchanged until the
entire program and its clock conversion have passed atomic queue admission.
Actual future synthesis can still fail and is recorded by the existing receiver;
successful scheduling is not a claim that every instrument played successfully.

Negative humanization is retained by placing the section at a positive output
epoch: `preroll_ticks = max(0, -earliest_event_tick)`. Add that offset to all grid
messages before conversion, leaving setup at output tick zero. The earliest note
can therefore coincide with setup; existing queue ordering applies controls first.
This is an explicit finite-audition scheduling policy, not a recreation of the
original operating-system lookahead or playlist epoch. No event is clipped. The
last boundary is the maximum of shifted section duration and all event dates;
release envelopes may continue beyond it. Tick/clock overflow fails explicitly.

## Explicit audition randomness and lifecycle

Callers provide cursor state and a 32-bit draw seed. The local deterministic
15-bit generator uses `state = state * 214013 + 2531011` and `(state >> 16) & 32767`.
Only values actually consumed by variation/note preparation advance its retained
state; unused pool suffixes do not. This is isolated audition randomness, not a
claim about the retail process's global seed, draw history or playlist policy.
The catalogue explicitly chooses zero initial variation/chord state, random mask
`0xffff`, seed 1, no percussion remap and gain 20%. The inspector labels this mode.

One program uses the existing sampler clock, without a second per-frame scheduler
or an inspector callback. Start refuses an already occupied instrument queue or
live instrument voice; it neither overwrites nor silently mixes another audition.
A failed start preserves the previous successful report. The report owns plain
messages/counters; queued note-ons and live voices retain their immutable bank.
World unload clears all of these through existing catalogue and AudioMixer cleanup.

`DlsSampler::silence` explicitly cancels **all instrument** queued messages and
voices, releasing their banks without rewinding the sample clock or erasing
channels, bank selectors and failure/counter history. It does not stop world WAV
sounds. PCM already submitted to the shared device is allowed to drain; clearing
that stream would also discard world sounds. This abrupt developer stop is not a retail fade/playlist stop rule. The
UI names its full scope. Later diagnostics can share the receiver's state; there
is no independent concurrent music-stream ownership claim.

## Complete channel initialization

This integration exposed a compiler-dependent initialization failure: with the
installed GCC 16.2.1 at `-O3`, the nested aggregate/lambda constructor followed by
writing percussion slot 9 zeroed controller values for slots 9..15. Those voices
could be accepted yet silent because expression was zero. A standalone C++20
reproduction confirms the failure without SDL or game data; explicit array fill
passes both tested GCC/Clang optimization levels. Construction and reset now fill
all sixteen channels explicitly before setting the percussion flag. This preserves
the intended default volume 100, expression 127, pan 64 and pitch bend 8192; no
compiler flags or musical rules change. Regression checks cover every channel and
require nonzero finite PCM from each authored nonsilent Cabal section.

## Inspection and limits

World sounds exposes a finite-section play button, stop action, selected filename,
prepared grids/notes/curves/tails, draw count, preroll, audio start/end and actual
pending count. Delivery failures are shown separately from cancellation and an
elapsed sequence. The play button invokes preparation; drawing never progresses
the sequence. The latest report survives completion for inspection, and reload
clears it. Program clocks and stop behavior also work in inspector-free builds.

Full-section preparation is appropriate for this finite diagnostic. Do not reuse
it as an unbounded playlist allocator. Dynamic section selection, transition
quantization, repeats, tempo changes across sections, game music volume, global
variation continuity, voice stealing, broader DLS failures and required `MUSIC`
remain explicit work. Existing synthesis is a replacement, not bit-identical
original PCM; automated nonzero output does not establish listening parity.
