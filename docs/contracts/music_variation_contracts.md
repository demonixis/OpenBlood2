# IMA pattern channels and variation decisions

`read_ima_style` now retains the entire admitted 625-byte pattern header.
`prepare_ima_pattern_channels` reproduces original loader defaults without
changing authored data. `choose_ima_variations` reproduces one channel-variation
decision with explicit random draws. These are sequencing primitives, **not a
live sequencer or a successful MUSIC service**. Pattern scheduling, generation
of required chord flags, pitch conversion and audio dispatch remain unavailable.

## Stored pattern fields

The prefix described in [note contracts](music_note_contracts.md) is unchanged.
The remaining fields are little-endian:

| Offset | Type | Field |
| --- | --- | --- |
| 52, 56 | u32 each | Chord / scale masks; not harmonized here |
| 60–75, 76–91 | i8[16] each | Upper / lower pitch bounds by source channel |
| 92 | u16 | Pitch-range channel mask |
| 94–605 | u16[16][16] | Channel-major variation flags |
| 606 | u16 | Unidentified word, retained without inferred meaning |
| 608 | i8 | Root |
| 609–624 | u8[16] | Authored channel play modes |

All bit patterns survive decoding, including negative pitch bounds, inverted
ranges, unknown mode bits and flags. Data validity is separate from supported
playback. Bounds and modes are inspectable but are not used as final MIDI pitch.
The reader's existing exact sizes, budgets, atomic failure and ownership apply.
No additional allocation depends on an untrusted count: every channel table is
fixed at 16 by 16.

## Loader defaults

For each channel independently, start with missing bits `0x1800`. Clear a bit
if any of that row's sixteen flags already contains it. OR the remaining bits
into every flag in that row. Thus absence of bit `0x0800` or `0x1000` throughout
a row enables it throughout that row; presence in even one variant prevents that
bit from being added to the others. Other bits remain unchanged.

Zero authored play modes become 1 on source channel 5, 2 on source channel 4,
and 3 elsewhere. Every nonzero value is retained. The prepared values are owned
copies; the shared immutable style retains the original fields for inspection.
This does not resolve the downstream meaning of every mode or flag.

## One variation decision

The caller supplies the prepared table, an optional 16-bit required flag word,
sixteen current variation masks, random/cycle channel masks and explicit draws.

1. With required flags present, a variant is eligible when its flags contain
   **all** required bits. Candidate order is ascending variant index 0..15.
2. If no variant qualifies, or the original current-chord input is absent,
   admit all sixteen variants. `fallback_channels` records this branch. A
   present zero requirement also admits all variants, without the fallback bit.
3. A random channel consumes exactly one 15-bit draw, even for one candidate.
   Select `draw % candidate_count`, producing a one-bit variation mask.
   Draws are consumed in ascending source-channel order. Random mode takes
   precedence over cycle mode when both bits are set.
4. A cycle-only channel doubles its current mask; if the old high bit is set,
   replace it with 1. Zero stays zero; multi-bit values are not sanitized.
   This preserves the original low-word behavior without signed overflow.
5. Otherwise hold the current mask. **Cycle and hold do not consult candidate
   eligibility.** Do not add a filter that changes the original decision.

The explicit draw span must contain exactly one value per random channel, each
in 0..32767 (the original local rand output range). Preflight rejects mismatch
or out-of-range values and clears the output, with an error. No partial state
commit, allocation, global RNG consumption, I/O or callback occurs. The helper
returns candidates, selected masks, fallback mask and draw count as values.
It does not decide *when* the original player would enter this decision block.

## Session, inspection and limits

Pattern fields follow existing world-owned immutable styles. The in-game World
sounds tree exposes pitch masks/bounds/root, raw word606 and authored versus
prepared play modes and variation flags. Opening it never selects a live
variation or consumes RNG. Hidden steps and unload/reload use the existing
catalogue ownership; GPU smoke exercises explicit diagnostic decisions with the
inspector both enabled and absent. No gameplay services are newly reported as
implemented by this data/algorithm work.

[Original executable evidence](../../../Documentation/Implementation/m2_music_variations.md)
covers the loader and selection block with controlled state. Full MUSIC playback,
pattern/band choice, chord/personality pitch handling, live event dispatch, curves
and playlist transitions are still required. [Event preparation](music_event_contracts.md)
now reproduces the post-harmonization arithmetic with explicit inputs. M2 remains incomplete.

The [harmonic-pitch continuation](music_pitch_contracts.md) now implements the
mode/chord/scale arithmetic for explicit effective runtime harmony and feeds it
to the diagnostic event probe. Earlier missing-pitch statements are superseded
only for this step; personality/current-chord selection and live MUSIC remain open.

The [section/pattern continuation](music_selection_contracts.md) now supplies original
explicit-tick current chord/mute and pattern decisions. Earlier missing-selection
statements are superseded only for normal section decisions; live activation,
seek/restart, band selection and MUSIC remain unavailable.
