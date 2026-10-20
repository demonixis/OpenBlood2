# M2 — Session cost and acceptance audit

The acceptance audit reproduced the existing AI-clearance sanitizer timeout with
current live game music. The unchanged CTest failed at 90.12 seconds. A separately
labelled diagnostic with a 180-second process ceiling completed in 90.36 seconds:
both complete world lifetimes retained eight blocked resizes, 93 original AI shots
and successful death/corpse teardown. That diagnostic does not replace the failed
CTest, and no test duration or assertion is reduced.

A 500-stack GDB sample of the current Debug scenario included service tracing in
33 stacks and voice advancement in 46. These are indicative inclusive samples,
not precise percentages of execution time. The first allocation-only change still
exceeded the 90-second deadline; its failed attempt is retained in the evidence.

Two bounded changes remove redundant work: service-counter lookups no longer
construct temporary owned names, and DLS voices omit zero-depth modulation math.
No gameplay, collision, AI cadence, audio-event ordering, sample-rate, resource or
dependency change is involved. Existing inspection remains truthful and unchanged.
See [contracts](../../contracts/session_cost_contracts.md).

The broader audit also found an obsolete hardcoded line in the CityHub particle
test. Its existing assertions already require 453 created, zero unsupported,
zero failed and one missing-world-model entry, but its console text still printed
the old 443/10/1 counts. The report now prints the measured counters. No entry is
reclassified and no assertion changes; the original stale log and the corrected
recheck are both retained. Historical documentation is not current runtime evidence.

## Validation

Implementation commit: `8a0db703cde22a96d8f1d7ca84e94938432d49ce`.

All **991 distinct non-GPU cases** are validated in Debug, inspector-free Release
and Clang ASan/UBSan with leak detection. This is a complete registered non-GPU
set, unlike the preceding selected regression runs. The sanitizer clearance case
runs separately without competing builds/tests; the other 990 cases run together.

| Configuration | Initial suite / corrected case | Wall time |
| --- | --- | --- |
| debug | 990/991; audio-device recheck 1/1 | 334.36 s |
| release | 990/991; audio-device recheck 1/1 | 53.65 s |
| sanitize | 989/990; audio-device recheck 1/1; isolated clearance 1/1 | 1674.55 s |
| debug, NVIDIA | 5/5 | 50.69 s |
| release, NVIDIA | 5/5 | 17.02 s |

Wall times refer to the initial suite/GPU invocations. Isolated clearance and
single-case recheck times are recorded separately in the ledger.

The initial full invocations each retain one harness-environment failure:
`cinematics.device` deliberately selects the dummy device using `SDL_SetHint`,
but the wrapper had globally forced `SDL_AUDIODRIVER=dummy`. In the pinned SDL,
an environment value takes priority even when equal to the requested hint. Each
exact case passes with both `SDL_AUDIODRIVER` and `SDL_AUDIO_DRIVER` unset, so the
test can perform its own selection. No product code or assertion is changed for
this failure. The initial Debug build also records an omitted `OpenBlood2`
qualification in the inspector-only counter snapshot, fixed before the final
build and graphics checks. All three configurations build successfully.

The [evidence ledger](evidence/m2-session-cost.json) retains validation commands,
complete logs, JUnit results, source hashes, failed attempts and the diagnostic profile.
The [source ledger](../sources/m2_session_cost_sources.json)
identifies the exact project scalar baseline; no new original-code import is involved.

The unchanged 90-second sanitizer clearance test now passes in **87.93 seconds**,
with the same eight blocked resizes and 93 original AI shots in each lifetime.
This leaves only about two seconds of local headroom; a single before/after
measurement does not establish a stable speedup or guarantee the deadline under
different load. The earlier 90.12-second failure remains part of the record.

Differential tests preserve exact PCM, sample cursors, frame counts, loops and
completion across all eight modulation combinations and every one of the 38
external Cabal regions. They cover live controller changes, delayed LFO starts,
one-shots, loops, partial/absent output, choke and release. Service tests retain
owned names after caller storage changes, unknown counters, disabled tracing,
nested scopes, failure counts, inspection snapshots and world teardown/reset.
The complete suite also checks malformed readers, original object/player lifetime,
restoration, contact/animation delivery and the continuous 30/60 Hz train route.

NVIDIA RTX 3060 Laptop Vulkan checks cover lifecycle, cinematics, player keep-alive,
level replacement and the complete train route in both builds. All ten logs confirm
the device. The CityHub and player-inspector captures were visually inspected;
they show actual textured geometry/characters and restored player state. Commercial
imagery stays untracked under the build directories. Dummy output is not a listening
comparison, and this run adds no native Metal evidence.

All nine changed C++ files pass formatting and owned whitespace/link checks.
All 10923 inventoried reference files and six reference repositories remain unchanged;
the user-edited root README is byte-identical. The corrected CityHub report also
passes a final direct sanitizer recheck after its logging-only edit, under the same
360-second process ceiling; it preserves all existing assertions and three lifetimes.

## M2 acceptance follow-up

The [original M2 goal/work/exit gate](../../history/Roadmap/03_milestones.md) is unchanged.
The selected continuous SubwayA combat route and CityHub handoff already have
original shell, movement/contact/animation, resource ownership and service-trace
evidence. The current music increment also removed the authored MUSIC rejection.
These facts do not certify every branch of the game or every current test.

Consolidation must distinguish required selected-session calls from unsupported
APIs outside that path: general SaveObjects, RestoreObjects and EndHMessageRead
still fail explicitly and are unexercised by the selected route. Whole-game saves
and full soundtrack parity are not silently added to the M2 exit criteria.

The public `run_world` entry is still an unconditional acceptance refusal after
attempting player entry. Replacing that historical guard requires concrete
runtime readiness checks and rejection tests, together with a completed review
of the selected session. The separate `--view-world` session remains usable.
No runtime guard is removed and M2 is not marked complete by this cost correction.
The existing external NVIDIA/DBus LeakSanitizer report is retained separately
from headless session validation; this change does not suppress it.
