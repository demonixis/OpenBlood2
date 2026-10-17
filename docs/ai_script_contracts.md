# Original AI script preparation and state dispatch

Later [production NPC lifecycle contracts](ai_runtime_contracts.md) supersede
this snapshot's absence of Cultist/Civilian initialization, AI_Update and activation.
The existing action/service limitations remain unless explicitly superseded.

[Source selections](m2_ai_script_sources.json) connect AI_Mgr's script list,
UpdateScriptCommand and AI_STATE_Script to the existing [movement helpers](ai_follow_contracts.md),
[target/state services](ai_state_contracts.md), animation clocks and WAV mixer.
See [evidence](../../Documentation/Implementation/m2_ai_script.md).
This is the explicit script-state entry point, **not the complete AI_Update**.
Production SCRIPT messages still fail: their original forced AI_Update includes
activation, target retention and other prerequisites that are not yet integrated.
No production NPC registration or M2 completion is claimed.

## Parsing and ownership

ParseScript is a pure bounded conversion with unchanged output on failure.
PrepareScript parses first, then performs original SetNewState(Script), installs
owned commands, selects index zero and sets the next-command flag. It does not
run a pretend forced update. Callers explicitly supply the current PathMgr to
AI_STATE_Script; no borrowed manager or source string is stored by game code.
Command views are read-only and expire on preparation/selection.

The existing semicolon/quoted-token parser accepts at most 1,279 bytes and 64
commands including the SCRIPT header. Embedded NUL and incomplete quotes fail.
Each instruction has exactly two tokens; names/arguments are limited to 49 bytes,
leaving the original 50-byte argument buffer's terminator room. Overlong arguments
are rejected, never silently truncated. Unknown names map to DONE as in
AIScriptList.h and retain their spelling for inspection. Empty arguments are valid.
The header's extra tokens are ignored as in the source.

A header without flags clears only interruptible bit 1. INTERRUPTABLE sets that
bit and preserves other prior bits; a numeric header replaces all flags. Numeric
flags and WAIT retain defined atoi-prefix behavior: leading ASCII whitespace/sign,
integer prefix, fractional/trailing text discarded, nonnumbers zero, negative
values allowed. Signed 32-bit overflow fails before mutation. Negative flags use
defined unsigned conversion. This differs deliberately from the stricter standalone
trigger numeric grammar. SDK Parse edge cases are not claimed bit-compatible.

A malformed replacement preserves stored commands/state but latches a visible
failure; a valid PrepareScript clears the latch and starts a new program. Missing
assets, failed path plans, blocked resizing and missing state decisions also latch.
No failed side effect is retried automatically. Reentrant preparation/update fails;
an outer callback cannot overwrite that failure with success. Object removals stay
deferred; owner validity is checked before continuing and all path actions retain
the existing checked message/string ownership.

## Sequencing and source quirks

Each state update selects at most one new instruction. LOOP bit 0 retains the
list and advances/wraps its index; nonlooping selection copies/removes the first
instruction. Current instruction storage survives callbacks and list mutation.
SETMOVEMENT and PLAYSOUND complete immediately for selection on the next update.
FOLLOWPATH/MOVETOOBJECT run their initial helper and first update in the same state
callback, retaining source cached-basis timing and residual velocity.

WAIT sets an absolute original game-clock deadline and completes inclusively at
`time >= deadline`. It does not stop residual movement. PLAYANIMATION selects a
named clip, disables looping, and waits for MS_PLAYDONE. Validation/clearance happen
before the looping mutation, unlike the unchecked source. Model loss is a failure.
PLAYSOUND submits an attached, unretained spatial one-shot with radius 5,000, inner
radius 500, priority 2 and full volume. It does not wait for voice completion.
Missing WAVs fail; the existing mixer owns decoding/playback and unload cleanup.

SETSTATE and TARGET retain their original state transitions (TARGET chooses
AttackFar). The remaining commands stay stored, but the next caller observes the
changed state and does not execute them. State selection does not execute an
unported attack/search action. Target lookup failure remains explicit, retaining
the prior target rather than continuing as if acquisition succeeded.

Exhausting a list clears flags and calls the subclass ComputeState once. An explicit
or unknown DONE instruction calls it twice: once in UpdateScriptCommand, then again
in AI_STATE_Script. PLAYANIMATIONLOOPING selects a looping clip but has no case in
the original state switch, so it also clears flags and calls ComputeState (once).
These source quirks are tested, not repaired. Missing subclass decision logic is
a failure; synthetic probes use a labelled test-only inactive decision. A production
state dispatcher must still choose the correct original subclass behavior.

## Inspection, verification and remaining scope

The Skeleton inspector exposes outcome, flags, cursor, remaining instructions,
current command/argument, next-selection flag, deadline, selections/updates,
started voices and failures. Unknown DONE mappings are visible. Reading inspection
runs no commands and works for hidden/inactive/missing-model instances.
The existing GPU script probe now runs an actual four-instruction program with a
real Cultist ABC, checks it with inspection hidden and repeats after reload. It
runs beside the existing fire driver without changing the authored scene lifetime.

Headless tests cover parsing, one-command cadence, loops, wait boundaries, all
instruction families, animation completion/default fallthrough, state interruption,
path/target motion, self-removal, reentrancy, missing services and real PCM playback.
Private checks parse all six SubwayA script properties (12 instructions, zero
rejections) and complete one real animation on each of six character models.
Parsing authored text does not prove those scripts execute against production NPCs.

Full AI_Update/initialization/deactivation, autonomous attack/death actions, shade
queries remain unavailable at this historical snapshot. The
[opportunity-fire continuation](ai_script_fire_contracts.md) now supplies the original
standing/walking/running scripted firing branches. Required client
FX, ExitTrigger and complete session services also keep M2 open. Source object counts
remain 187 created, 4 transferred sounds, 372 editor-only, 17 unsupported, 0 failed.
