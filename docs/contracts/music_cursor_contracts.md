# IMA section grid cursor

`ImaSectionCursor` traverses one prepared section using the original normal-mode
2/3 grid rules. It composes [selection](music_selection_contracts.md), prepared
[harmony](music_harmony_contracts.md) and [variation](music_variation_contracts.md).
It is an explicit diagnostic cursor, **not** a soundtrack player or implemented
MUSIC service. It neither runs on wall time nor schedules MIDI or audio.

## Binding and ownership

Start with the exact immutable section/style/timeline/harmony binding produced
by the catalogue. Callers must not mutate these resources through another alias.
The cursor checks timeline provenance, retained chord indices/times, nonempty
patterns, source channels and bounded counts before retaining shared ownership.
Limits are 4,096 patterns, 65,536 grid records and 262,144 note rows per style;
section/timeline limits remain those of selection. The catalogue's validated
binding remains responsible for style identity and harmony preparation.

Every pattern must have a positive interval and length and a positive signed
low-word `length / timing[0]`. Zero/negative or wrapped nonpositive grid counts
are refused rather than allowed to stall or run a broken clock. Pattern duration
uses this count, not the number of decoded grid records. Missing grid records are
valid silent steps. Extra records do not extend the pattern.

The seed explicitly supplies current variation masks, random/cycle channel masks
and previous chord mask/root. The diagnostic UI uses zero previous state and all
channels in random mode with explicit zero draws. This does not establish the
original playlist's initialization policy. A new traversal starts at tick/grid/
serial zero. Failed start preserves the prior cursor. Stop/replacement releases
all bindings; a finished traversal retains them for inspection until stopped.
The owning world catalogue clears the cursor, label and error on reload/unload.

## One atomic step

1. Capture the **old grid ordinal**, then look up current chord and mute using
   the original first-entry/file-order rules. Chord change compares only mask
   and root; scale/pitch/variation flags alone do not trigger reevaluation.
2. Scan authored commands until the first future entry. An exact-tick command
   resets the current grid to zero, even when its flags are zero. No sorting or
   deferred delivery of a command skipped by the interval occurs.
3. At grid zero choose the pattern with the original section request/chooser.
   Otherwise retain the current pattern. The step's `pattern_selected` flag
   distinguishes a new choice; its match reason is meaningful only then.
4. Take the interval from the selected pattern's `timing[0]`. Locate its grid
   by the **old ordinal**, ignoring the authored `clik` index. Therefore a command
   can choose a new pattern but visit a later (or absent) grid in it on that first
   step. Do not repair this source quirk by resetting the saved ordinal.
5. Reevaluate variations when the reset/current grid is zero or mask/root changed.
   `variations_changed` means evaluated, not necessarily different resulting bits.
   Otherwise hold every channel and consume no draws. Only the required prefix
   of an explicit 15-bit draw pool is consumed, in original channel order; extra
   unused values are ignored. Insufficient/invalid consumed draws fail atomically.
6. For each present grid note, test its source-channel mute bit first, then overlap
   of selected and authored variation masks. Count admitted, muted and excluded
   rows separately. Admitted rows are **not** emitted/playing notes: type, pitch,
   timing, controllers and voice ownership still belong to downstream work.
7. Remember mask/root even for absent grids, add the interval to section tick,
   increment low-word serial and grid. At section end (including overshoot), reset
   tick/grid/serial to zero as the original does and mark traversal finished,
   stopping before its repeat/playlist branch. Otherwise wrap grid at the signed
   pattern count. No remaining-time clamp or final synthetic grid is inserted.

The state and last successful report commit together after all checks. A failed
step changes neither. It consumes no global RNG, performs no I/O or callbacks,
and cannot partially dispatch anything. Each call has bounded work and storage;
positive intervals bound a single traversal. Stored duration plus one interval
fits signed 32-bit arithmetic under the existing meter limits.

## Inspection and verification

World sounds / Original music assets / Grid cursor exposes start/step/stop, current
and processed ticks, pattern, old ordinal, grid availability, interval/count,
restart and reevaluation reasons, draws, note filters and channel masks. The cursor
exists outside ImGui; opening/collapsing the UI does not advance it. GPU smoke
explicitly steps actual Cabal sections while hidden and after reload; the manual
probe never changes game rules or claims audio playback.

[Evidence](../../../Documentation/Implementation/m2_music_cursor.md) compares the
original tick routine's isolated control flow with complete portable traversals.
Prepared fixtures come from already verified readers and helpers. Emulation keeps
section-event and active-note lists empty and intercepts notification, metronome,
MIDI emission and playlist side effects. Those paths are explicitly **not** covered
by the equivalence claim. No full scheduler, seeking, tempo clock, band selection,
curves, note lifetime, playlist transitions or successful MUSIC call is supplied.
The actual M2 gate remains open.
