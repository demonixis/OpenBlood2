# M2 — Original single-section grid progression

M2 remains **incomplete**. The prepared original music data now drives a stateful,
inspectable section cursor: pattern boundaries, command restarts, chord/mute
selection, variation cadence, note admission and section-end reset. This connects
the previously isolated decisions across consecutive grids. It does not play the
soundtrack; MUSIC still reports unsupported.

## Original behavior

The installed `IMUSIC25.DLL` tick routine at RVA `0x16908` captures the grid ordinal
before testing exact-tick commands. A command can reset the current grid, select
a new pattern and still look up the old ordinal in that pattern. The portable
cursor preserves this quirk, including a silent step when that grid does not exist.
The interval is pattern header `timing[0]`; grid count is the signed low word of
length divided by that interval, independent of the decoded grid-list length.

Variations are reevaluated at grid zero or when chord mask/root changes. A change
only to scale or flags keeps the previous variations. Notes pass mute filtering
before variation filtering. At or past section end, original tick/grid/serial
resets occur before the cursor stops at the unimplemented repeat/playlist boundary.
The [contracts](../../contracts/music_cursor_contracts.md) document
bounds, explicit seeds/draws, atomic failures and immutable resource lifetime.

World sounds now offers a manual grid cursor with start, step and stop. Its report
shows processed/current ticks, selected motif, old ordinal, absent grids, variation
reasons/draws and admitted versus excluded note rows. Catalogue unload/replacement
releases its retained section/style/timeline/harmony. UI visibility never clocks it.
These are truthful preparation diagnostics, not synthetic successful MUSIC calls.

## Original executable comparison

The [probe](evidence/m2-music-cursor-probe.py) executes original x86 instructions
in Unicorn 2.1.4 against the hash-pinned external image, without native DLL loading
or OS imports. `ima_cursor_tests export` produces a temporary prepared input/trace
file; it contains external data and must not be committed. The probe compares every
step's 38 state/report words and exports only aggregate hashes. Existing reader,
timeline and harmony comparisons establish the preparation used by these fixtures.

This is an isolated control-flow comparison: section event and active-note lists
are empty, and hooks intercept category/intensity notifications, metronome/beat
notifications, MIDI emission and playlist lookahead/repeat/transition side effects.
The original note admission branches still execute. No full scheduler, audible
output or active-note-lifetime equivalence is asserted.

| Dataset | Sections | Grid steps | FNV64 | SHA256 |
| --- | --- | --- | --- | --- |
| Generated | 128 | 2799 | 10075978454587385952 | `166dcfdb0cff0dae19dcb2afe496e68b1749aa89591d0accf350226734b8cab0` |
| Installed bound sections | 132 | 8174 | 14793402716944126194 | `43a59ca5b55ef2f482a2ad74bbf1d2e2f565a66a328ed34ddf7c946d40765ea9` |


Generated scenarios include duplicate and unsorted commands, mid-pattern resets,
changes in chord masks/roots, nonuniform intervals, end overshoot, missing grids,
muted channels, variation fallback/cycle/hold/random combinations and mismatched
pattern flags. Focused tests verify flag/scale-only chord changes, atomic failures,
invalid timing/channels/budgets and retained resources across failed replacement,
stop and world unload. The installed 24 Track1 sections retain missing-style failures.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug | 106/106 passed | 18.24 s |
| release | 106/106 passed | 1.48 s |
| sanitize | 106/106 passed | 70.29 s |
| debug NVIDIA Vulkan | 2/2 passed | 10.87 s |
| release NVIDIA Vulkan | 2/2 passed | 3.71 s |

The [ledger](evidence/m2-music-cursor.json) records exact commands, raw logs/results
and source hashes. The [validation driver](evidence/m2-music-cursor-validation.py)
builds the relevant targets with one job per configuration (three total), then
runs 106 cases covering cursor behavior, bounds, generated/private traversal,
world/catalogue lifetime, existing music readers/helpers, objects, audio and config.
Clang ASan/UBSan uses leak detection and halt-on-error. No build warnings occurred.

NVIDIA sound and keep-alive checks pass in Debug and inspector-free Release,
including explicitly stepped Cabal sections with hidden UI and after reload. The
reviewed capture shows the sound inspector summary; nested manual cursor buttons
were not visually exercised. Dummy audio checks submission rather than soundtrack
listening. Native Metal, Windows and mobile were not exercised in this increment.

Seven changed C++ files pass formatting. The 10,923 reference files and six
reference repositories remain unchanged, local documentation links pass and the
user's README is preserved. The temporary private emulator fixture is removed;
only the probe source and aggregate observations are committed.


Remaining music work: original playlist initialization and transport/tempo clocks,
section events, effective bands/controllers/curves, note emission/ownership,
repeats/transitions and MUSIC command dispatch. Existing missing style references,
invalid DLS loop profiles and broader required-service acceptance remain explicit.
The M2 exit criteria are unchanged.

Implementation commit: `2b9f1901115a7040baa102a082a2006dd0fd70b8`.
