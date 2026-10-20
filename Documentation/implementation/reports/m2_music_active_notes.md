# M2 — Original IMA active notes and due stops

M2 remains **incomplete**. Connected grid-note preparation now preserves active
note lifetime across steps, including the original chord-dependent deadline rule,
creation-serial exemption and due/retained list ordering. The in-game music
inspector exposes pending notes, due stops and chord assignments. No soundtrack
playback or successful MUSIC service is claimed.

New notes register before chord processing. A changed chord can assign an older
nonmember note's deadline to the output tick plus one. Notes from the same serial
are exempt. Both pending and due lists follow source head-insertion order instead
of chronological sorting. Inclusive grid horizons emit due values; section-end
tails remain visible for the future playlist transport.

The operation commits atomically, rejects an overflowing output horizon or more
than 65,536 pending-plus-new records, and retains checked immutable bindings.
Admission-only stepping explicitly clears lifetime tracking. Stop and catalogue
reload release it; copied inspection remains valid. See the
[contracts](../../contracts/music_active_note_contracts.md) and
[source ledger](../sources/m2_music_active_notes_sources.json).

## Original comparison

The [probe](evidence/m2-music-active-notes-probe.py) executes the installed,
hash-pinned IMUSIC25 normal tick in isolation. It now executes original serial
registration, keeps active lists across calls, observes chord deadline assignments
and intercepts the due-off sink. Prepared inputs use previously verified readers
and helpers. No native DLL or synth executes, and no commercial bytes are committed.

| Input | Sections | Grid steps | Due stops | Chord assignments | Decision FNV64 |
| --- | ---: | ---: | ---: | ---: | --- |
| Generated signed roots, masks and sustained notes | 128 | 2,799 | 4,061 | 812 | `4553459640914638122` |
| Installed sections with selected bands and prepared harmonies | 132 | 8,174 | 6,112 | 0 | `182961564129265722` |

Generated cases cover all 256 signed root encodings, masks with high bits and
5,532 emitted pairs; peak pending count is 37 and 1,471 tails remain at section end.
The private comparison has 6,290 emitted pairs, a peak of 17 and 178 terminal tails.
It does **not** exercise chord shortening; the generated cases do. The 24 Track1
sections missing their referenced style remain explicit failures outside the bound
section set. Original MIDI delivery, section events, notifications, metronome,
repeats and playlist transitions remain excluded from this comparison.

Additional behavioral checks cover inclusive deadlines, same-grid exemption,
unsorted stop order, deadline assignment under a clock jump, 16-bit serial wrap,
empty-grid draining, terminal tail retention, failed start/step atomicity, capacity
failure, admission-only clearing, catalogue unload and copied result lifetime.
The wrap test deliberately holds the explicit output clock still; it is a synthetic
serial boundary test, not an audio-clock scenario.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 116/116 passed | 17.02 s |
| release-tests | 116/116 passed | 1.77 s |
| sanitize-tests | 116/116 passed | 83.60 s |
| debug-gpu | 2/2 passed | 10.91 s |
| release-gpu | 2/2 passed | 5.37 s |

The [validation ledger](evidence/m2-music-active-notes.json) records commands,
source hashes and raw logs; the [driver](evidence/m2-music-active-notes-validation.py)
selects music, object/world, audio and configuration suites. All six changed
C++/CMake hashes match the tested versions; five C++ files pass formatting. Final
builds emit no warnings. The frozen-source fixture export equals the probed input.

Both GPU configurations use NVIDIA GeForce RTX 3060 Laptop Vulkan. Hidden-inspector
and inspector-free Release paths check conservation of prepared, pending and due
notes, then world teardown/reload. The reviewed Debug capture shows the music
summary; the nested lifetime controls were not visually exercised. Dummy audio
checks existing output integration, not a listening comparison. Native Metal,
Windows and mobile were not tested.

All 10,923 inventoried reference files and six reference repositories are unchanged.
Local documentation links pass. Source/documentation whitespace checks pass; raw
logs/XML retain tool-generated trailing whitespace byte for byte. The user's root
README remains untouched.


Live tempo/activation, section events/curves, controller delivery, playlist/repeat
handling and synthesis integration remain unfinished. Required MUSIC keeps the
actual M2 exit gate open.

Implementation commit: `013563793f4c9f6aed8ca1a9bbc3809500134c14`.
