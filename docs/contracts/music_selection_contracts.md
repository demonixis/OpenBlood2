# IMA section and pattern selection

`select_ima_section` computes an original authored-section decision at an explicit
signed tick. `select_ima_pattern` chooses a pattern from a decoded style. These
helpers own their results and never advance clocks, activate patterns, consume
RNG, read files or dispatch MIDI. They cover normal section modes 2/3, **not**
seek/restart, arbitrary mode changes or live MUSIC playback.

## Section decision

Use the existing reconciled timeline and its original section. Validate bounded
record counts, positive meter, duration, command mapping and retained chord
mapping before indexing. Mute ordering, source indices and values are checked.
Raw records remain immutable. Keep original file order; never sort chords or
commands to make the results appear more regular.

- The current chord is the first retained chord, then consecutive successors
  whose signed time is <= the supplied tick. The first chord is selected even
  before its time; a future successor stops traversal despite later older entries.
- Mute selection follows the same first-entry/successor rule on prepared samples.
  An empty mute list means zero. Store the selected sample index separately from
  its source record. The original section's first-future mute behavior is already
  preserved by timeline reconciliation.
- Current measure is signed division of tick by effective ticks per measure,
  truncated toward zero and stored in the low signed 16 bits. Do not clamp the
  explicit tick to the section duration. Calls before/after the section are useful
  diagnostics; they do not imply a running section would still be active.
- Initial requested length is low-word `(section measures - current measure)`;
  flags start at `0x200`. Consume commands in order until the first future command
  or the first command exactly at the tick. Each nonzero `flags & 0x13f80`
  replaces the accumulated flags. Only an exactly equal command contributes its
  `flags & 0x63`; stop at the **first** duplicate equality.
- The next command boundary replaces requested length, even if beyond the section
  or earlier than the current measure. At equality use the next list node, if any.
  Command flags are the four original value bytes interpreted little-endian.
- Starting at the selected chord, skip nodes earlier than the tick. Subtract
  current measure from each chord's measure with low-word wrapping, then clamp
  negative deltas to zero. A nonzero delta limits requested length; if bit 8 was
  already set it instead forces length 1 and terminates traversal.
- A nonzero chord beat with nonzero delta selects flags `(flags & ~0x1c) | 4`
  and terminates. With zero delta an odd beat selects `0x10`, length 1, then
  terminates; an even nonzero beat selects `8` and continues. Negative beats keep
  their original parity. Add bit 4 unless bits `0x18` are present. Upper-limit
  requested length to 12; preserve zero and negative results.

An empty retained chord list fails explicitly. The original uses a chord from
global runtime state at this point; the section's prepared base scale is **not**
an established replacement for that object. Do not invent that fallback or mark
its path successful. Prepared harmony stays inactive; the returned chord index
can select its matching prepared record without mutating it.

## Pattern decision

Pattern `timing[3]` (file header offset 18, original runtime offset `0x52`) is its
signed measure count. Eligibility requires all requested flag bits and equality
of category bits `0x63`. Preserve authored pattern order.

For positive requested length, select the first eligible pattern at the largest
positive measure count <= the request. If no such length exists, use the **last**
eligible pattern. If none is eligible, or request length is nonpositive, use the
first pattern even if its flags disagree. An empty style returns no selection,
with an explicit empty result. Inspection distinguishes length match, last
eligible fallback, first-pattern fallback and empty.

The implementation scans once, bounded to 4,096 patterns. This is equivalent to
the original descending-length scans: its weaker flag fallback slots can only be
written after the full-match gate, and the last full match takes precedence over
both. This removes the original length-times-pattern cost without changing ties
or fallback order. Low-word operations are explicit, avoiding signed overflow.
Failure clears both selection outputs and supplies an error.

## Ownership, inspection and limits

The catalogue retains the already validated section/style binding as another
immutable shared reference, only after timeline preparation succeeds. It does
not copy a style or perform another lookup in the inspector. Both references
expire when the world catalogue is replaced/unloaded; mismatch/budget failure
leaves the section binding absent. No pointers into the resource data escape.

World sounds / IMA sections / Section selection probe accepts an explicit tick
and shows measure, mute, retained/source chord, prepared pitch/variation flags,
requested flags/length, selected pattern and fallback reason. It is labelled as
a diagnostic decision, independent of a live clock. NVIDIA hidden/reload and
inspector-free checks also select from actual Cabal data and pass the selected
chord flags into the existing variation helper with explicit diagnostic draws.

[Executable evidence](../../../Documentation/Implementation/m2_music_selection.md)
compares 4,096 generated cases and 1,188 decisions from 132 sections with bound
styles. The other 24 sections retain their missing-style failures. This does not
certify audible timing, band selection, curves, note ownership, live pattern
activation, personality graphs or playlist transitions. MUSIC and M2 remain open.

The [grid cursor](music_cursor_contracts.md) now composes these decisions into one
manual section traversal with original restart/variation/end rules. Live transport,
seeking, notifications, active notes and MUSIC playback remain unavailable.
