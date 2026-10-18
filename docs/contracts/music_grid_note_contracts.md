# IMA admitted grid notes

`ImaSectionCursor::step_notes` composes the checked [cursor](music_cursor_contracts.md),
[harmonic pitch](music_pitch_contracts.md) and [event arithmetic](music_event_contracts.md).
It prepares owned note-on/off pairs for one normal-mode section step. It does not
submit MIDI, schedule audio or implement MUSIC.

## Inputs and ordering

The immutable section/style/timeline/harmony bindings and explicit cursor seed
retain their existing validation and ownership rules. The caller supplies the
selected band, an output-clock tick and an optional 128-byte percussion map.
These are borrowed only for the call; no band/map reference escapes. The catalogue
helper uses its selected band, the current section tick and no percussion map.
No implicit original global map, wall clock or RNG is invented.

Normal modes 2/3 set the original local pitch-conversion mode to 2 (IMUSIC25 RVA
`16b38`). Therefore the selected prepared chord is activated in a local copy;
immutable prepared harmony remains inactive. Its effective scale and base root
already include the verified reconciliation. The note's original raw bypass and
zero-pitch suppression still apply.

A single explicit 15-bit draw pool supplies, in order:

1. Variation decisions in ascending channel order, only when the cursor requires them.
2. Admitted note rows in source order: time, duration, velocity for each nonzero range.

Muted rows are rejected before variation-excluded rows. Neither consumes note
draws or invokes pitch conversion. Unsupported types fail only when admitted.
Suppressed zero pitches consume no note draws and remain inspectable rows.
Only the consumed prefix is validated; an unused invalid tail is irrelevant.

Each admitted row uses the selected band's **source-channel** transpose. Optional
percussion mapping applies only to source channel 5, before transposition. The
original MIDI mapping `(source + 4) & 15`, signed jitter, octave folding and
same-velocity note-off remain unchanged. Output clock and section position are
separate: original note time uses the tick routine's argument, not the section's
internal tick. Negative start times and cross-grid ends are retained, not clamped.
Pairs remain in source order, not sorted by their humanized start/end times.

## Atomicity, bounds and inspection

The method advances a candidate cursor, prepares all admitted rows, then commits.
Any failure preserves the prior cursor state, last step and last note inspection;
there is no partially advanced clock, consumed implicit RNG or partial MIDI output.
The previous potentially large inspection vector is not copied into the candidate.
The existing 262,144-note bound also bounds one prepared row vector.

The result owns row indices, resolved pitch/mode and paired event values. It has
separate note-draw and suppressed counts; `last_step.draws_used` still counts only
variation draws. `opaque_curve_chunks` reports grid curves that remain unprepared.
An admission-only `step` clears prior note inspection on success. Start, stop and
world-catalogue reload/unload clear it; failed operations preserve it. Copied result
values remain valid after resources are released. Completion retains results until
stop/replacement, as before.

The inspector's **Step and prepare notes** uses an explicit 4,096-zero draw pool;
exhaustion fails visibly. The clipped row list shows actual pitches and note pairs,
including suppressed rows. Drawing does not advance this cursor. The GPU smoke
also prepares notes with the inspector hidden and in the inspector-free build.

## Limits

This covers prepared pairs from admitted pattern notes. Opaque section events and
curves, live tempo/activation, active-note reharmonization and note-off dispatch,
controller updates, repeats and playlists remain separate. DLS routing/synthesis
is not invoked by this method. The original comparison intercepts MIDI output,
note allocation/registration, notifications and playlists, and discards active
notes after observing their prepared values. It is not a full tick/audio proof.
Required MUSIC remains unsupported and M2 remains incomplete.

The [active-note continuation](music_active_note_contracts.md) now carries pending
off values, applies original chord/serial rules and prepares due stops. This
supersedes the active-lifetime omission above; that paragraph describes the earlier
probe. Live transport, external MIDI delivery and MUSIC remain unavailable.
