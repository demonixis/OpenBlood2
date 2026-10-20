# M2 — Original section and pattern decisions

M2 remains **incomplete**. The music preparation path now selects the current
section chord/mute at an explicit tick, derives the original pattern request,
and chooses its pattern. The inspector exposes these decisions with their real
bound resources. This connects prepared section/chord data to pattern selection;
it does not yet schedule or play the soundtrack. MUSIC stays unsupported.

## Original behavior and implementation

The installed `IMUSIC25.DLL` performs chord and mute lookup in its section tick
routine (`0x16afc..0x16b1a`, `0x16970..0x16996`), derives pattern requests at
`0x17b33`, and chooses patterns at `0x17a4f`. Original section entry paths set
mode 2. Pattern header mapping at `0x8748..0x879d` identifies file offset 18
as the signed measure count used by the chooser. The earlier verified section
reader maps four command bytes directly to its runtime flag word.

The portable helpers preserve first-future chord selection, file order, first
command equality, signed low-word measure arithmetic, off-beat chord flags,
negative requested lengths and the original fallback ordering. Pattern choice
uses a bounded linear equivalent of the original repeated descending-length
scan. It does not introduce new musical rules. The empty-chord global fallback
remains an explicit failure rather than substituting the section base scale.
[Contracts](../../contracts/music_selection_contracts.md) detail each
branch, bounds, ownership and the deliberate exclusion of seek/restart paths.

The catalogue now retains its validated immutable style binding for each prepared
section. Failed bindings remain unavailable, and old shared references expire on
world replacement. The probe uses those resources without file access, clock
advancement, mutation, musical RNG or audio output. World sounds shows the selected
chord and mute, requested flags/measures, chosen pattern and exact fallback reason.
Prepared harmony itself remains inactive.

## Executable comparison

The [probe](evidence/m2-music-selection-probe.py) executes bounded original x86
instructions using the hash-pinned external DLL in Unicorn 2.1.4. It runs no
native library or OS imports and exports no commercial payload. Timeline inputs
use the previously verified reconciliation rules. The actual lookup, request and
pattern decisions execute original instructions, including the private-data
comparisons. The global empty-chord fallback and full tick scheduler are excluded.

| Dataset | Decisions | FNV64 | SHA256 |
| --- | --- | --- | --- |
| Generated sections/patterns | 4,096 | 12281621980575432131 | `7b8a629fa84cca632c63c8bf37babf7bf8c995edcf5ba7326295ee92b1ddda01` |
| Bound installed sections | 1,188 | 2704444894253380274 | `a1c4ad7fa377894e2a1bd6890095f6dd074439575a27508297238611b0082e83` |

The [observations](evidence/m2-music-selection-probe.json) cover 132 bound sections
at nine boundary ticks each. The 24 Track1 missing-style sections remain failures.
Generated inputs include duplicates, unsorted commands/chords, signed tick extremes,
off-beat changes, absent patterns and incompatible flags. Focused assertions also
cover exact category matching, equal-length ties, last-eligible/head fallbacks,
32767-measure requests, invalid mappings and value lifetime after input destruction.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 102/102 passed | 15.69 s |
| release | 102/102 passed | 1.43 s |
| sanitize | 102/102 passed | 65.70 s |
| debug NVIDIA Vulkan | 2/2 passed | 10.51 s |
| release NVIDIA Vulkan | 2/2 passed | 5.38 s |

The [ledger](evidence/m2-music-selection.json) records exact commands, raw build/test
results and source hashes. The 102 cases cover the new decisions, malformed input,
private data, binding/lifetime and existing world/object/music/audio/configuration
paths. Clang ASan/UBSan passes with leak detection and halt-on-error. NVIDIA sound
and player keep-alive checks pass in Debug and inspector-free Release, including
hidden/reload decisions using actual section/style/chord resources and explicit
variation draws. No build warnings were reported.

Eight changed C++ files pass formatting. The 10,923 reference files and six
reference repositories are unchanged; documentation links pass. The user's README
is preserved. The reviewed NVIDIA capture shows the sound inspector summary;
the nested explicit-tick probe was not visually exercised. Dummy audio verifies
submission, not soundtrack listening parity. Native Metal, Windows and mobile
were not exercised for this increment.


Remaining music work: live section/pattern activation and clocks, effective bands,
controllers/curves, note ownership and playlist transitions. Empty-chord global
state, personality graphs, missing style references and invalid DLS loop profiles
are not resolved by this increment. The actual M2 acceptance criteria stay open.

Implementation commit: `90ed46370d12dee326e9fbc12dcef2f0fc9e74e5`.
