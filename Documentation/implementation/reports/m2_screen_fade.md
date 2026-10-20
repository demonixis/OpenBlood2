# M2 — Authored train exit, player fades and smoke capacity

The complete SubwayA exit sequence exposed two gaps that direct ExitTrigger and
stationary session checks had missed: `player FADEOUT` was silently ignored, and
a final explosion's smoke trail exceeded its 2,048-particle pool, failing a session
update before the exit. Both are addressed without changing authored trigger
schedules, fragment motion, emission or lifetime. **M2 remains incomplete.**

## Behavior and inspection

The selected original `CPlayerObj::ProcessTriggerMsg`/`SendFadeMsg` and client
SMSG_SCREENFADE/update branches now deliver an owned fade event. Literal-space
first-token parsing, case-insensitive fade names, ignored suffixes, dead-player
handling, retrigger reset and base callback continuation follow the original code.
The client changes its 0–255 value by 100 units/second, clears fade-in only above
255, and holds fade-out at zero. The numeric rate, not the inaccurate original
“4 second” comment, determines its approximately 2.55-second duration.

The clock advances with simulation while inspection is hidden. Client exit and
new/restored world entry clear the state, so CityHub does not inherit a black
screen. The inspector shows mode, value, scale, delivery counts, unknown commands
ignored by source, and retained last unsupported command. Music, sound/VoiceMgr
and CTF command families remain explicit omissions; the bounded server-interface
catalogue does not describe every game command.

The current unlit renderer multiplies the composed scene with a black SDL_GPU UI
layer before HUD/inspection. This is a visible, portable presentation adaptation,
not full retail global-light-scale/material behavior. Blindness, seeing-eye,
night-vision precedence and detailed lighting remain separate. No shader or pinned
dependency changes are needed; native Metal was not rerun on this Linux machine.

## Measured capacity correction

A real contact with the authored exit relay reproduces the old failure at 6.783
seconds. One trail has 2,024 live particles just before the failing burst, while
all trails together contain only 3,919. With unchanged source rules, the complete
sequence at 60 Hz peaks at **10,240 particles in one trail**. The previous simultaneous
explosion probes did not exercise this staggered sequence.

The smoke pool now permits 16,384 live particles per system and across all
explosion trails. The shared integrator accepts an explicit bounded capacity;
other callers retain their 2,048 default, and a requested capacity above 16,384 is
rejected. Per-burst limits, 128-system limits, atomic invalid-state rejection,
random draw order and visible failure behavior remain. The shared sprite snapshot
has a 32,768-quad ceiling for smoke plus the other supported effects. Arrays grow
on demand. No emission is dropped and no failed update is declared successful.
These are measured replacement-engine budgets, not claimed retail limits.

## Acceptance scope

The private regression completes the actual intro, then explicitly relocates the
player to the last carriage. It walks/uses the door through the existing player
path and activates `Trigger2` through contact. A diagnostic aiming loop fights
visible authored Cultists through the original view-weapon FIRE_KEY path. It grants
no health/ammunition, changes no NPC or trigger, and does not inject damage, exit
messages or invulnerability. A first stationary attempt correctly died in the
uncleared carriage; that failed diagnostic is retained rather than claiming the
area harmless.

The authored relay sends FADEOUT after seven seconds and triggers ExitTrigger
after twelve; the latter's two seconds produces the actual exit at fourteen.
The test verifies black hold, combat hits/model keys, no suspended AI or generic
unavailable services, retained health/ammo into CityHub, fresh fade state and full
teardown in two cycles. Diagnostic relocation/aiming is not complete level traversal.
Earlier mouse-driven combat/corpse tests remain separate evidence.

Synthetic checks cover strict endpoints, invalid time, retrigger/reversal, parsing,
message/string ownership, dead-player dispatch, queued-message disposal and client
reentry. Particle tests retain old default-budget rejection and validate explicit
larger bounded pools. GPU readbacks check brightness 1/0.5/0/1, two viewport sizes,
unchanged simulation during drawing and legible HUD/inspection above black.

## Validation results

| Configuration | Headless checks | Private checks included | NVIDIA checks |
| --- | ---: | ---: | ---: |
| Debug, inspector enabled | 69 / 69 | 14 | 10 / 10 |
| Release, inspector disabled | 69 / 69 | 14 | 10 / 10 |
| Clang ASan/UBSan, leak detection | 25 / 25 | 3 | Not run |

The initial sanitizer suite passed 68 of 69 tests: the new fade guard rejected an
existing `.11`-second ring diagnostic. Core session steps allow up to 60 seconds;
only the interactive frontend caps at 100 ms. The code now respects that contract.
The final sanitizer run rechecks the corrected fade/ring paths, object lifetime and
player restoration; it is not reported as a second full 69-test sanitizer run.

The final authored exit has 174 service rows per cycle (348 in each retained
Debug/Release trace), zero unknown catalogue names/generic unsupported calls, and
one separately recorded unsupported player command (`MUSIC LEVEL 3`). Successful
server-boundary counts therefore do not imply that every original game command is
implemented. All ten relay slots dispatch once. Large smoke snapshots above 8,192
particles prepare every trail entry without resource errors.

All 20 changed/new C++ files pass formatting; local Markdown links resolve.
The 10,923 inventoried reference files are unchanged and six reference repositories
remain clean. Generated private assets/captures stay outside tracked project data.
The black-frame GPU capture was visually checked with readable HUD/inspection.

Validation results and exact source/test evidence are recorded in the
[evidence ledger](evidence/m2-screen-fade.json). See the
[contracts](../../contracts/screen_fade_contracts.md) and
[source selections](../sources/m2_screen_fade_sources.json).

Implementation commit: `1a8915f962768de41739a646561b33c54bfbcc31` in the independent Blood2 repository.
