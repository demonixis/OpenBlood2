# IMA active note lifetime

`ImaSectionCursor::step_notes` now carries pending note-off values between the
[connected grid-note steps](music_grid_note_contracts.md). It follows the installed
IMUSIC25 normal-mode active-list rules. These are prepared event values, not MIDI
submission or an audio transport; required MUSIC remains unsupported.

## Original ordering

New non-suppressed note pairs register their off value with the **pre-step**
16-bit grid serial. The source prepends each new record to the existing active
list. Creation order therefore reverses before chord processing. No duplicate
key/channel coalescing occurs.

A chord change still means a changed chord mask or root, not a scale/flag change.
For each active record whose serial differs from the current low-word serial,
check the emitted MIDI key against the current chord. Matching serials are exempt,
including old records whose serial matches again after wrap. The membership test
preserves the original unsigned-word root comparison, low-word subtraction,
repeated chord mask and x86 shift-count masking; do not normalize signed roots.

A nonmember's deadline is assigned `output_tick + 1`, **without** taking a minimum
with its old deadline. An explicit diagnostic clock jump can therefore extend a
deadline. The `shortened` field counts these assignments; the UI calls them
chord-adjusted notes. It does not count only strict reductions.

Partition the list at `output_tick + grid_interval`. Deadlines equal to the
horizon are due. Both due and retained partitions reverse their encountered order,
as the original builds both by head insertion. Return due stops in that order,
without timestamp sorting. The next grid starts with the retained order; this
order matters on later grids. Humanized note-on timestamps remain unchanged.

## Atomic ownership and bounds

The candidate step prepares notes, applies chord changes and partitions lifetime
before committing. Failure retains prior state, pending notes and inspection.
The output horizon must fit signed 32-bit ticks. At most 65,536 records may be
pending plus newly introduced **before** partition; reaching that bound is an
explicit error, even if some records would be due in this step. This is a portable
resource bound, not a claimed original capacity.

Pending records own MIDI values and their serials, with no pointers to notes,
bands or maps. Copied inspection survives stop/world unload. Immutable section
bindings retain their existing ownership. Successful start/stop, world-catalogue
replacement and admission-only `step` clear pending records; failed start/step
preserves them. Mixing admission-only and connected-note traversal is an explicit
diagnostic discontinuity, not a transport flush.

Single-traversal completion retains terminal pending notes for inspection and
refuses another step. It neither flushes those notes nor pretends they played.
Repeat/playlist transport must eventually handle these tails. The source's global
player/epoch registration is represented here by one explicitly owned cursor and
its serial; concurrent transport epochs are not implemented.

## Inspection and evidence

The existing **Step and prepare notes** action advances these rules outside drawing.
The **Active notes and due stops** tree lists pending deadlines/serials followed by
due messages in their respective order. Counts expose capacity and chord assignments.
The ordinary **Step grid** control explicitly discards active-note tracking.
GPU smoke verifies note conservation while hidden, after reload and without ImGui.

[Evidence](../../../Documentation/Implementation/m2_music_active_notes.md) compares
retained list order, serials, deadline changes and due dispatch values against the
hash-pinned original tick. The probe executes original serial registration and
retains its active lists; allocation and external MIDI/synth sinks are intercepted.
This supersedes the previous grid-note probe's active-list exclusion for these
bounded paths only. Opaque section events/curves, live clocks, controller delivery,
playlists and synthesis integration remain unimplemented; M2 stays incomplete.
