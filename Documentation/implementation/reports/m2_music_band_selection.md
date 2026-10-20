# M2 — Original effective music bands

M2 remains **incomplete**. Sections now select their original effective instrument
configuration and retain its source identity and ordered setup messages in the
world-owned music catalogue. This closes the initial band-choice gap; it does not
send MIDI, route DLS instruments or play the soundtrack. MUSIC remains unsupported.

## Behavior and integration

The original section loader replaces its band for each eligible direct AABN, so
the last one after `secn` wins. Without one, the referenced style's default getter
walks its prepended band list: the last file-order band after `styl` with nonzero
byte104 wins. Negative flags also count. A style with no marked band returns none,
without choosing an unmarked head. Early bands remain inspectable raw data but
are not selected. Names and collection labels do not determine priority.

The catalogue reuses its validated style binding. Each section retains an alias
into the immutable source catalogue, its source file/scope/index, and a separately
validated 80-message setup plan. Invalid volume/pan/program does not select a
different band. Selection failure and setup failure are displayed separately in
World sounds. There is no per-frame I/O or dependency on UI visibility.

The [contracts](../../contracts/music_band_selection_contracts.md)
define ownership, limits and remaining unsupported branches. In particular, the
old no-style GM construction remains unsupported. Selecting a section band does
not repair a missing style, invalid DLS loop or another sequencing failure.

## Original comparison

The [probe](evidence/m2-music-band-selection-probe.py) runs hash-pinned external
IMUSIC25 instructions in bounded isolated x86 emulation. It executes the original
section assignment branch, style-list prepend, section fallback and style default
getter. Allocation, constructor, refcounts, critical sections and band decoding
are intercepted; the earlier band probe covers decoding. No DLL is loaded natively,
no OS imports run and no commercial bytes are copied into the repositories.

| Observation | Result |
| --- | --- |
| Synthetic combinations | 800 |
| Original/portable decision FNV64 | `9953174249627237157` |
| Original decision SHA256 | `1b74b20d16fb13fadd3904d07b4e8bb8a4750edc8378ccc6ee51252a6b9ee7a9` |
| Installed section overrides | 156/156 |

Generated cases vary absent/multiple section bands, header placement, multiple
style defaults and zero/negative flags. Focused tests add missing defaults,
malformed metadata/budgets, alias lifetime, reinspection and selected-but-invalid
setup. Private world tests check selection/setup with hidden UI and resource
release on unload; the original 24 Track1 missing styles remain failures.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 108/108 passed | 15.74 s |
| release-tests | 108/108 passed | 1.45 s |
| sanitize-tests | 108/108 passed | 66.75 s |
| debug-gpu | 2/2 passed | 10.80 s |
| release-gpu | 2/2 passed | 3.71 s |

The [ledger](evidence/m2-music-band-selection.json) retains commands, source hashes,
raw build/test logs and isolated-probe observations. The
[driver](evidence/m2-music-band-selection-validation.py) builds relevant targets
with one job per configuration (three total) and runs the music, world/object,
audio and configuration suites. No compiler warnings occurred. Eight changed
C++ files pass formatting; 10,923 reference files and six reference repositories
are unchanged. Local documentation links pass and the user's README is preserved.

NVIDIA Vulkan sound/keep-alive tests pass with hidden UI and after reload in Debug
and inspector-free Release. The reviewed capture shows the sound-inspector
summary, not the nested selected-band tree. Dummy audio verifies submission rather
than soundtrack listening. Native Metal, Windows and mobile were not exercised.

Remaining music work includes tempo/transport and activation, controller
application/curves, DLS routing, section/active-note events and original
playlists/repeats/transitions. The M2 exit gate is unchanged.

Implementation commit: `c8d122b5fd8d9bd451ff0d0671c0d44d26c4cc4c`.
