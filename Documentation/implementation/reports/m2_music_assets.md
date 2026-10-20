# M2 — Original music asset prerequisite

M2 is **incomplete**. The continuous SubwayA exit reaches `MUSIC LEVEL 3`, which
still cannot play. This increment identifies and validates the resources required
by the original client initialization without returning a silent success for that
service.

## Implemented

The selected original CMusic initialization builds an owned request plan from the
world's actual music properties. It preserves the two instrument arguments,
reverse transition loading, repeated `cts.sec`, SilenceList and authored playlist
order. Original source provenance and bounded adaptations are recorded in the
[source ledger](../sources/m2_music_asset_sources.json).

The external content root supplies loose music files. The new reader inspects
AASE sections, AASY styles and DLS instrument-bank RIFF structure with explicit
range, recursion, chunk, file and total-read limits. Section style references
resolve in the same directory with case-insensitive exact-name matching;
ambiguous names, unsafe paths and symlinked components fail visibly. No executable
is loaded and commercial files remain external.

The World sounds inspector shows original request order, actual unique reads,
file sizes, chunk/pattern counts, style references and per-file failures. Surveys
run at world loading and clear on unload; player entry, simulation frames and UI
visibility do not repeat them. Inspector-free builds take the same snapshot.

This is structural evidence, not decoded musical events or playback. Unknown
records remain opaque, `.PER` personality semantics are unimplemented, and the
existing MUSIC failure remains observable. See the [contracts](../../contracts/music_asset_contracts.md).

## Installed-content observations

- All **156 sections, 39 styles and 5 DLS banks** pass the bounded structural
  reader: **21,513 chunks / 11,986,804 bytes**. This does not certify their musical
  semantics or the five separate personality files.
- Both SubwayA and CityHub request `music/cabal`: **52 ordered requests** resolve
  to **45 unique files**, including section style dependencies, totaling
  **2,004,402 bytes** per survey. Repeated playlist/transition entries are retained
  in the original plan but do not duplicate file reads.
- No missing or malformed container is observed in those two worlds' surveys.
  DLS synthesis, section/style scheduling and personality behavior remain the
  implementation prerequisites for actual music.

## Verification

| Configuration | Result | Wall time |
| --- | --- | --- |
| GCC Debug, inspector enabled | 59/59 passed | 47.56 s |
| GCC Release, inspector disabled | 59/59 passed | 7.37 s |
| Clang ASan/UBSan, leak detection enabled | 59/59 passed | 242.93 s |
| NVIDIA Vulkan, Debug | 3/3 passed | 45.25 s |
| NVIDIA Vulkan, Release | 3/3 passed | 11.68 s |
| Final Debug inspector readability recheck | 1/1 passed | 4.47 s |

The headless selections contain 59 distinct cases per configuration; this is not
a claim that every registered project test ran. Both existing train routes
retain the unsupported MUSIC assertion. Sanitizers were headless; GPU runs used
the NVIDIA GeForce RTX 3060 Laptop GPU with dummy audio output. No native
macOS/Windows/mobile execution or human listening is claimed. Existing enum/float
build warnings remain. All 10,923 inventoried reference files and six reference
repositories are unchanged; the user-edited README is preserved.

Final command results and limitations are recorded in
[the validation ledger](evidence/m2-music-assets.json). Synthetic cases cover
truncation, corrupt lengths, duplicate/invalid style references, odd padding,
unknown opaque chunks, nesting/count limits, original request order and parsing,
missing/ambiguous/symlinked/oversized files, owned metadata and reset behavior.
Private cases read all installed RIFF music files, load both levels, enter/update
and unload the player/session, and verify transactional content-root mounting.

The existing object, resource, world/session, service-coverage, sound and complete
train-route checks guard integration. NVIDIA checks include the actual inspector,
hidden updates/reload, preserved player entry and continuous arrival in CityHub.
Screenshots remain local because they contain commercial game imagery.

## Remaining gate

Implement original musical records and personality references, DLS synthesis,
clocks and playlist transitions before claiming music support. The supplied
original motif/break methods are no-ops and must be assessed separately. Do not
replace the source music with an unrelated soundtrack or reinterpret a valid RIFF
header as successful playback. The selected-slice service audit and the full M2
exit gate remain open.

Implementation commit: `2a57e2c110b97fa67cf553e6747c65acad57ff3f`.
