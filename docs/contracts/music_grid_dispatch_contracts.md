# IMA ordered grid dispatch

`ImaSectionCursor::step_notes` accepts an optional exact-style `ImaCurves` binding.
When present, it prepares an owned dispatch sequence in addition to existing note
pairs, due stops and active-note state. The world catalogue supplies the curves
from the same immutable style used by the section's validated binding. Missing or
failed curve decoding refuses the operation; it never silently drops controllers.
Callers outside the catalogue must likewise pair results decoded from the same
bytes. Offsets/counts detect structural mismatches, not arbitrary modified values.

## Order and selection

Original IMUSIC25 grid loading appends note and curve chunks to one list in file
order (`8900..8a2f`). The grid loop (`17103..1759f`) applies the source-channel
mute test, then selected/authored variation overlap, before dispatching either
kind. The portable cursor recovers that interleaving from retained source chunk
metadata: note rows in each direct grid `note` chunk and curves in each direct
`curv` chunk, including within-chunk ordinal. Unknown/nested chunks stay opaque.
No file reads, sorting or reconstruction from event times occur.

Curves use current selected variations after the existing grid/command/chord
logic. They consume no random draws. Existing note preparation may therefore run
first without changing draw consumption; resulting note-ons are then merged with
expanded curves in source order. Suppressed notes contribute no dispatch message.
All curve points use the explicit output tick, with no second header-offset
addition. Due note-offs follow the entire grid sequence, in their already verified
original partition order. Messages can be out of timestamp order and extend past
the current grid; this is preparation, not immediate audio application.

The report distinguishes curve admitted/muted/variation-excluded counts, expanded
curve-message count, unprepared chunks and `curves_prepared`. With a valid supplied
binding, the last flag is true even for an absent/silent grid; due stops still
appear. Without that binding, the older note-only diagnostic remains available:
its dispatch stays empty, its flag is false and its opaque curve counts remain.

## Bounds and ownership

The supplied immutable resources remain borrowed only for the operation. Results
own message values, never curve pointers. Preflight checks at most 8192 chunk
records and 65536 actual curve-plus-point records, monotonic curve chunk offsets
and contiguous within-chunk ordinals. Selected ranges must match pattern/grid
identities, note counts, complete curve payload sizes and curve chunk counts.
Invalid selected channels fail before shifting; unsupported types fail only when
admitted through the filters. Each curve keeps its own point/event bounds.

The merged sequence is capped at 262144 messages including due stops. Failure in
any late range check, curve evaluation or combined limit preserves the prior
cursor state, active notes and all previous inspection. There is no partial MIDI
send or global RNG consumption. The existing `step`/stop/reload clearing rules
are unchanged. Explicitly switching to note-only `step_notes` clears old merged
inspection while retaining its documented note-lifetime behavior.

World sounds / Grid cursor exposes filter counts and **Ordered grid dispatch
(no playback)** with clipped raw message rows. This is owned cursor state;
opening/closing the tree does not prepare or advance it. Catalogue reload releases
bindings and results. The hidden-inspector/inspector-free smoke checks dispatch
accounting independently of UI, while headless private traversal exercises complete
selected sections.

[Evidence](../../../Documentation/Implementation/m2_music_grid_dispatch.md) compares
9198 grid visits with the original filter, note-event tail and curve-expansion
routines. It supplies previously resolved pitches, variations and explicit ticks;
full harmonic/section selection and due-note partitioning have separate evidence.
Original loader owner fixups are outside that executable probe. Read
[curve events](music_curve_event_contracts.md), [grid notes](music_grid_note_contracts.md)
and [active notes](music_active_note_contracts.md) before extending delivery.
Controller application to active voices, clock/playlist transport and MUSIC remain
unavailable; the actual M2 gate is still incomplete.
