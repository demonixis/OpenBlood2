# M2 — Original KeyFramer sound playback

Original sound-bearing path keys now play attached WAV voices with the authored
radius and loop flag. The controller retains one handle across all controlled
targets: every target replaces the preceding sound before attempting playback.
Silent keys, OFF and path completion leave the voice intact. Destruction releases
it; target removal is handled by the existing checked mixer ownership. Sound runs
before key messages, without changing timeline or movement rules.

The Keyframes inspector shows actual attempts, starts, failures, checked speaker,
retained/completed voice, decoded frames, cursor and last mixed position. See
[contracts](../../contracts/keyframe_audio_contracts.md) and the
[source ledger](../sources/m2_keyframe_audio_sources.json).
No dependency, engine service, resource fallback or game rule was added.

## Behavioral evidence

Four synthetic cases exercise retained one-shots/loops, single-handle replacement
across multiple objects, missing audio after successful playback, silent keys,
OFF/Resume/restart, empty target lists, decoded PCM, following a hidden speaker,
reentrant OFF/REMOVE, owner/controller removal, non-reused IDs, unload/cache cleanup,
invalid radius and the unchanged 128-voice limit. Existing keyframe/camera/world-path
and object-lifetime checks remain intact.

The installed catalog has 16 sound-bearing keys. Thirteen references decode;
two basename-only `pickuptruckloop1.wav` entries in Abandoned Buildings and
`sounds/enemies/civilian/scream2.wav` in Slaughter House do not resolve. They remain
failures; no broad basename search or invented replacement is used. The existing
malformed Bridge world rejection is unchanged.

Two Train Station reload cycles activate the actual `uhoh` controller through a
diagnostic ON command. Its authored `tr` keys wait six seconds, then play
`trainpassby1.wav`: 110592 decoded frames, outer/inner radii 6000/600, priority 1,
retained loop attached to the actual `train` Damage volume. Audio continues after
OFF and is released on controller removal. **Moving this container remains
unsupported**: audio success does not imply Train Station traversal or motion parity.

## Validation

Results and normalized command logs are recorded in
[evidence/m2-keyframe-audio.json](evidence/m2-keyframe-audio.json).
The focused Debug and inspector-free Release suites cover original keyframes,
world paths, object lifetime, cinematics, door/trigger audio, service coverage and
the continuous SubwayA-to-CityHub routes at 30/60 Hz. The same selection runs under
Clang ASan/UBSan with leak detection enabled: **61/61 pass in each configuration**.
The sanitizer run took 301.90 seconds; both continuous train routes passed with
their unchanged 240-second per-test limit.

NVIDIA checks pass **5/5 in Debug and 5/5 in Release** (plus a final Debug
inspector-layout-only rerun, 1/1). They exercise GPU lifetime, existing SubwayA camera/train paths, the new
Train Station sound path, and level changes. The new app smoke uses normal updates
through the authored six-second delay, verifies advancing decoded audio and SDL
submission while inspection is hidden, then repeats after unload/reload. The
inspector capture was inspected locally: selected controller, actual sound resource,
attempt/start/failure counts, retained voice and decoded cursor are visible. It
stays untracked with commercial imagery.
SDL dummy output verifies submitted samples, not human listening. Native Metal was
not run for this increment.

During implementation, checks caught a synthetic request with an invalid default
inner radius and a catalog assumption that missed the third absent file. Those
fixtures were corrected against measured input; runtime budgets and existing
acceptance assertions were not relaxed. Initial compile-only API-name errors were
fixed before final builds. A preliminary link check ran before the new report and
source ledger existed; the final check includes them.

## Remaining gate

M2 remains incomplete. Reverb, priority arbitration, print presentation, moving
containers, full save/restore and required remaining services stay explicit. The
previous `ai_runtime.clearance_private` sanitizer timeout and unsuppressed exploratory
NVIDIA/libdbus LeakSanitizer report remain recorded in
[door-audio evidence](m2_door_audio.md); this scoped run does not erase either.
Commercial assets and all reference repositories are unchanged.
