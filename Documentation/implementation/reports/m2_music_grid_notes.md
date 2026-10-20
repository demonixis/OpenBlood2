# M2 — Connected IMA grid note preparation

M2 remains **incomplete**. The section cursor now connects admitted notes to
original normal-mode harmonic conversion and selected-band event preparation.
This advances the music integration without marking diagnostic events as audible
soundtrack playback or a successful MUSIC service.

The new operation applies current chord/scale, channel variation and mute rules,
then selected-band transpose and original timing/velocity variation in source
order. One caller-supplied random stream covers both variation and note draws.
The output clock remains separate from the section cursor. Negative timestamps,
suppressed pitches and events extending past a grid remain observable.

A candidate step commits only after every admitted note succeeds. Failure preserves
its prior clock and inspection, including when earlier rows have already prepared
successfully. Results own their data; the band and optional percussion map are not
retained. The inspector adds a prepared-event list and reports opaque grid curves.

See the [contracts](../../contracts/music_grid_note_contracts.md) and
[source ledger](../sources/m2_music_grid_notes_sources.json).

## Original comparison

The bounded [probe](evidence/m2-music-grid-notes-probe.py) extends the previous
cursor oracle through the original admitted-note body. It executes the hash-pinned
external IMUSIC25 tick in isolation, observing pitch and note-on/off preparation.
Normal modes 2/3 explicitly enable harmonic conversion; prepared metadata remains
immutable. MIDI output, allocation/registration and unrelated transport callbacks
are intercepted. Active notes are discarded after observing the pair, so later
chord-dependent shortening and note-off dispatch are not covered.

| Input | Sections | Grid steps | Decision FNV64 |
| --- | ---: | ---: | --- |
| Generated commands, masks, modes, transpose and optional percussion maps | 128 | 2,799 | `937667046537544111` |
| Installed sections with their selected bands and prepared harmonies | 132 | 8,174 | `5880823308775508798` |

The private comparison observes 6,290 emitted pairs and 12,580 note draws. Generated
cases observe 6,779 admitted rows, 93 suppressed pitches and 12,179 note draws.

The 24 installed Track1 sections still lack their referenced style and are reported
separately; no substitute style is selected. Private fixture bytes stay outside the
repositories. This verifies prepared note events, not original song playback.
Behavioral tests additionally cover filtering-before-validation, exact draw order,
unused invalid draws, zero-pitch suppression, signed output time, selected-band
percussion transpose, insufficient draws, tick overflow, partial-step rollback,
owned result lifetime, stop/reload and admission-only inspection clearing.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 113/113 passed | 18.87 s |
| release-tests | 113/113 passed | 1.58 s |
| sanitize-tests | 113/113 passed | 75.24 s |
| debug-gpu | 2/2 passed | 12.22 s |
| release-gpu | 2/2 passed | 5.37 s |

The [validation ledger](evidence/m2-music-grid-notes.json) records the commands,
source hashes and raw logs; the [driver](evidence/m2-music-grid-notes-validation.py)
selects the music, object/world, audio and configuration suites. All eight changed
C++/CMake hashes match the tested versions; seven C++ files pass formatting. Final
builds emit no warnings. The frozen-source fixture export equals the probed input.

Both GPU configurations use NVIDIA GeForce RTX 3060 Laptop Vulkan. Hidden-inspector
and inspector-free Release paths prepare the selected sections' notes, then test
world teardown/reload. The reviewed Debug capture shows the music summary; the new
nested event controls were not visually exercised. Dummy audio verifies submission,
not a listening comparison. Native Metal, Windows and mobile were not tested.

All 10,923 inventoried reference files and six reference repositories are unchanged.
Local documentation links pass. The user's root README remains untouched.


Automatic tempo transport, active-note lifetime, section events/curves, controller
updates and playlists remain unfinished. Required MUSIC keeps the M2 exit gate open.

Implementation commit: `b5c3ee55d2dcf5084470f308747f31163a632bdf`.
