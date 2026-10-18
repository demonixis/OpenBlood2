# Interactive world entry and NPC preservation

The finite CLI and direct 3D entry are now covered by [checked session startup](session_start_contracts.md).
The M2 acceptance gate remains separate.

The application now holds a loaded world's simulation until the original local
player enters. This is an application playback policy, not an alteration of the
original AI rules. See [evidence](../../../Documentation/Implementation/m2_npc_entry.md)
and [source ledger](../implementation/sources/m2_npc_entry_sources.json). **M2 remains incomplete.**

## Reproduced boundary and policy

The ordinary inspector previously advanced GameSession before the user pressed
Enter player. In SubwayA, the original empty-enemy-list fallback then searches the
Cabal list. Three actors select themselves; the existing distinct-object link
contract refuses this and permanently suspends them. Other NPCs can acquire
unintended same-faction targets. Entering a player does not undo these failures.
An immediate-entry session has different initial conditions. This explains a
reproducible part of the reported non-attacking NPC behavior, not every encounter.

`app/session_playback.h` is the single interactive-frame policy used by main.
A loaded world with no entered client waits; its simulation, animation, physics,
audio and effect clocks do not advance. Each frame still polls events, applies
explicit editor commands, drains deferred object removals, draws inspection and
presents the GPU frame. Removal callbacks retain their normal ownership rules.
Previously queued SDL audio samples may finish; this is not an audio-device pause.
No waiting wall time is accumulated for subsequent simulation. Empty diagnostics
and explicit GPU smoke modes retain their existing simulation behavior.

The new Enter player button queues the same existing original shell/player entry
as the Session panel. Successful entry resumes on the next ordinary frame; failed
entry stays in inspection and exposes its error. `--view-world` still enters the
player immediately, before the first tick. Leaving the player or loading another
world returns to waiting; original restoration/keep-alive entry can also resume it.
Once entered, folding/hiding the inspector does not pause gameplay. Dead players
remain entered and therefore do not pause their world.

Direct GameSession::step remains an explicit simulation API for diagnostics/tests;
it is intentionally not globally gated. Existing no-player tests still reproduce
and assert the source fallback failure. No self-links are permitted, no faction
rule is changed, and no failed NPC is silently revived. This policy preserves a
fresh interactive session; it does not repair an already failed diagnostic world.

## Inspection and verification

The header shows waiting status and entry errors beside the entry button. Session
shows ready/failed/active NPC counts; ready does not mean currently active or
attacking. Existing Skeleton details retain runtime error, camera gate, target,
activity/pings, weapon attempts and shots. The shutdown log records waiting frames,
simulation time, NPC readiness/failures and update totals. Reading these counters
does not run AI or alter activation.

A real application GPU check presents 12 pre-entry frames with time zero, all 15
SubwayA NPCs ready, no failed actors and no AI_Update calls. A private regression
waits 180 interactive frames, enters normally, runs the complete original intro,
then places only the player at a labelled encounter inside the actual train.
Authored NPC transforms, states, inventory, activation and targeting are untouched.
Autonomous acquisition, original attack/fire commands and actual damage must occur.
The scenario repeats after unload/reload, checks waiting after leave and verifies
old identity/resource cleanup. A synthetic check also exercises pending removal
while waiting and opt-in diagnostic time advancement.

This is not a human walkthrough, a recreation of the retail server pause mechanism
or proof of all combat. Center-ray activation (rather than retail PVS), missing
states/services and safe NEW_LEVEL remain documented limitations. The train-window
material report remains unresolved and is the next playtest priority before more FX.

The [sky integration check](sky_contracts.md) exposed a slow-frame feedback failure
in actual variable-frame CityHub execution. The interactive frontend now caps one
frame at 100 ms and reports discarded wall time in the inspector/log. It excludes
resource/capture work from catch-up. Explicit diagnostic stepping and the core
physics limit remain unchanged; slow execution can slow simulation time. Tests
cover bounded lag, recovery on the next ordinary frame, diagnostic time, and invalid
deltas, plus repeated NVIDIA application starts with actual capture and simulation.
