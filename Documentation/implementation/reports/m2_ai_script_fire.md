# M2 — Original scripted opportunity fire

Original Script_Fire_Stand, Script_Fire_Walk and Script_Fire_Run are now connected
to the existing SCRIPT interpreter. FOLLOWPATH and MOVETOOBJECT no longer refuse
the OPPORTFIRE flag solely because those methods were missing. **M2 remains incomplete.**

## Original behavior preserved

The source in [AI_Mgr.cpp](../../../../sources/blood2/ObjectDLL/AI_Mgr.cpp) uses different
rules for the two movement commands. FOLLOWPATH tries standing fire while outside
its arrival threshold and resumes ordinary locomotion when no target is found.
It skips a velocity command while engaged but does not stop residual motion.
MOVETOOBJECT instead uses looping walking/running fire, writes velocity from the
previous movement sample and restores phase 2 after its moving phase.

Standing fire refreshes the selected target position on every call. Moving fire
retains the source's cached aim after reacquisition, and the shared target can
replace the named movement destination. Both use a strict reload deadline. A blocked
standing shot clears animation without consuming cadence; a blocked moving shot
consumes cadence and keeps the animation. Dry attempts retain the existing original
Fire acceptance. Completion does not advance the script phase. No new pathfinding,
damage formula, physics integrator or weapon implementation is introduced.

The typed boundary distinguishes valid no-target decisions from failed services.
Unavailable collision/model/clip/shot operations remain failures. A checked post-shot
boundary stops continuation after owner removal, recursive calls, nested script
failure or state replacement. The existing deferred lifetime remains in force.
Read the [contracts](../../contracts/ai_script_fire_contracts.md) and
[source ledger](../sources/m2_ai_script_fire_sources.json).

The in-game Skeleton inspector shows the last scripted-fire mode, engagement,
call count, reload clock and existing shot/blocked counters, including hidden and
inactive actors. These observations do not advance scripts or simulation.

## Verification

New behavioral cases verify strict timing, phase increments, no-target continuation,
real bullet damage, dry acceptance, source cached aiming, full injured movement
speed, loop completion, SCRIPT/FOLLOWPATH dispatch and MOVETOOBJECT phases. Explicit
fault probes cover blocked versus unavailable shots, removal, reentry and script
replacement during firing. They are not presented as real collision outcomes.

A private diagnostic loads SubwayA and a real Cultist ABC/animation table, then
runs each mode for four simulated seconds with the ordinary game step. Standing
fires 15 shots; walk/run fire 12 each. Target HP decreases in every case. Walking
advances about 80 units and running 160. Each mode unloads before the next; old
handles, strings, messages and clients are checked. These are isolated diagnostic
actors above the map, not authored placements or a completed encounter.

Targeted suites cover firing, following, scripting, perception, targets, production
AI, object lifetime and player controls. GPU checks cover the existing character,
hidden-inspector/reentry, combat/corpse and level-transition flows on NVIDIA. They
are graphics regressions, not visual proof of a newly authored scripted encounter.
Debug and inspector-free Release are built; Clang ASan/UBSan/leak tests cover the
changed boundaries. Commands, results, source hashes and the implementation patch
are retained in the [evidence ledger](evidence/m2-ai-script-fire.json).

The first build caught an incorrect namespace qualification of the existing
FireResult enum. Initial synthetic tests also used a one-unit target vulnerable to
normal bullet spread and a one-second frame outside the existing movement bound;
their fixtures now use an explicit target volume and valid half-second frames.
No production bounds, fire-spread rule or failure assertion was removed.

| Validation | Passed | Private-data tests included |
| --- | ---: | ---: |
| Debug targeted headless | 94 / 94 | 18 |
| Release targeted headless | 94 / 94 | 18 |
| Clang ASan/UBSan/leak checks | 70 / 70 | 10 |
| Debug NVIDIA | 6 / 6 | 5 |
| Release NVIDIA | 6 / 6 | 5 |

All 10,923 reference files match the inventory; all six reference repositories
remain clean. Formatting and local documentation links are checked before commit.

No dependency pins, shader artifacts, reference repositories or commercial assets
changed. Native macOS execution of this increment, authored opportunity-fire
encounters, remaining NPC death/gib/fire services and full M2 acceptance remain
unverified or incomplete. The continuation automation remains active.

Implementation commit: `79a2cef1891b0f10b9ab83b076cd19867352e305` in the independent Blood2 repository.
