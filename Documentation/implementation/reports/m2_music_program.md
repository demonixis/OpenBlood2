# M2 — finite original IMA section playback

M2 remains **incomplete**. A selected authored section can now play from start
to finish through the existing DLS sampler and SDL audio clock, independently of
the inspector. The developer UI exposes start, cancellation and actual program/
queue state. Game `MUSIC` playlist selection and transitions remain unavailable.

Implementation commit: `f89101114c282223d8a50f7683539ce8b65fd24f`.

See [contracts](../../contracts/music_program_contracts.md) and
[source ledger](../sources/m2_music_program_sources.json).

## Behavior and source evidence

Preparation composes the verified cursor, harmony, variations, ordered note/curve
stream, selected band and runtime tempo. It checks the whole finite program before
atomic queue admission, preserving early note offsets through an explicit output
epoch. The audio mixer remains the only advancing clock. There is no per-frame
pattern rebuild or new scheduler. At finite section end, remaining note-offs keep
their stored dates and list order; their release envelopes continue naturally.

The [isolated original-code probe](evidence/m2-ima-program-probe.py) executes
IMUSIC25's finite completion branch and note-off message constructor, with synthetic
active lists and intercepted sink/free calls. All 500 cases / 7930 terminal notes
preserve dates and order. Decision SHA-256:
`d1d49036ab8d54f39d59fdfac8a787d5cac2723c3d1e9499b8a6746d07c8b8d0`.
No native DLL, commercial bytes or disassembly are added to the project.

Explicit seed 1, random variation channels, 20% gain and the finite preparation
epoch are documented audition policies, not verified original global initialization.
Unsupported section-level events, malformed bindings, oversized programs and clock
failures are refused. Future instrument failures remain visible in the existing
receiver; scheduling alone is never reported as successful synthesis. Message,
grid and combined traversal budgets also reject oversized silent programs.

The stop action cancels all instrument voices and future messages without resetting
the shared audio clock or erasing error history. Existing world WAV sounds continue.
World unload releases queued banks and catalogue reports through existing ownership.

## Release initialization defect found by complete playback

The first full-section runs exposed a real Debug/Release audio difference despite
identical musical events and sample dates. The installed GCC 16.2.1 `-O3` build
zeroed controller fields for channels 9..15 when constructing the nested aggregate
through its lambda and then setting percussion slot 9. Some instruments were
accepted but muted by expression zero. A [standalone project-owned reproduction](evidence/m2-ima-program-init-probe.py)
isolates the same behavior without SDL or game data. Explicitly filling all sixteen
channels before the percussion flag fixes construction and reset; no optimization
flags or game music rules are changed. Tests now check all channel defaults and
nonzero finite audio for every nonsilent Cabal section, rather than only aggregate
output. The final validation uses the corrected implementation.

## Validation

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 145/145 passed | 27.21 s |
| release-tests | 145/145 passed | 4.42 s |
| sanitize-tests | 145/145 passed | 104.60 s |
| debug-gpu | 2/2 passed | 12.38 s |
| release-gpu | 2/2 passed | 5.74 s |

The [validation ledger](evidence/m2-ima-program.json) records frozen sources,
commands, complete logs and exact per-configuration Cabal accounting. The relevant
music/audio/object/world suites pass in Debug, inspector-free Release and Clang
ASan/UBSan. Synthetic tests cover early notes, original terminal-list order,
nonzero sample origins, reproducible consumed-only draws, malformed bindings,
message/traversal bounds, sample-for-sample block-size independence and bank lifetime after
cancellation. All 35 installed Cabal sections run through full PCM synthesis;
all scheduled notes start, no dispatch fails, and no held voice remains after the
last event. Each nonsilent section must produce finite nonzero PCM. Silent
sections correctly emit zero notes. Complete controller defaults are checked on
construction and reset; the final Debug/Release/Clang totals also agree.

The initial private check found a legitimate 4.80654-second release envelope in
hta.sec. Tests now distinguish note-off delivery from envelope completion: after
the last event, every remaining voice must already be released, then expire within
the sampler's existing 20-second bound. No envelope is shortened for the test.

Both GPU runs report NVIDIA GeForce RTX 3060 Laptop Vulkan. The sound smoke plays
the actual a2.sec section on the ordinary audio output, checks completion, restarts
and cancels without clock rewind, and leaves a separate future note for unload.
The same checks repeat hidden and after reload; the new catalogue report must
start empty. Existing player keep-alive/level lifecycle smoke also passes. Debug
capture inspection confirms the actual section/cancellation and queue report.
The new UI widgets were not manually clicked; the smoke invokes their backend.
Dummy output and nonzero PCM do not establish listening parity.

The final incremental rebuilds emit no compiler warnings. Formatting, owned whitespace
and local Markdown links pass. All 10923 reference files and six reference
repositories remain unchanged; the user's root README is preserved.

## Remaining scope

This finite audition does not implement playlist continuation, repeat selection,
quantized transitions, cross-section tempo/variation continuity, game music volume
or the original `MUSIC` service. Other DLS banks still retain their documented
unsupported data. Full soundtrack/listening parity and native macOS/Windows/mobile
execution are not established. Required unsupported calls still keep the unchanged
M2 exit gate open. The user's root README remains outside this increment.
