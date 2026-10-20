# M2 — Original trigger activation and lock audio

Trigger activation, locked-player feedback and explicit UNLOCK now play the
original attached one-shot requests through the existing WAV mixer. The Triggers
inspector shows configured cues, attempts/starts/failures, the checked speaker,
radius/priority and decoded voice progress. Source branches and ownership rules
are recorded in [contracts](../../contracts/trigger_audio_contracts.md).

Activation attaches to the trigger at its authored radius and priority 1. Locked
feedback follows the denied player at radius 200 and priority 2; explicit UNLOCK
follows its command sender. Inner radius remains one tenth of the outer radius.
A successful key response stays silent, as the corresponding original sound branch
is commented out. Repeated requests overlap; missing audio does not cancel key,
lock or activation progression. Checked voice history does not own the sound.

No new backend, generic interface, dependency or game-rule replacement was added.
Action-mode localized console feedback remains explicitly unavailable. The mixer
is still dry and does not implement reverb, priority arbitration or occlusion.
Music and consolidated M2 acceptance remain incomplete.

## Behavioral and private checks

Synthetic checks exercise all three trigger classes, count thresholds,
touch/message activation, current attached transforms, mixed PCM, overlapping
one-shots, player and non-player speakers, silent key responses, inactive command
gates, missing audio, invalid zero radius, absent sender, Action-mode refusal and
the unchanged 128-voice budget. Owner removal, a surviving sender after trigger
removal, stale IDs, hidden updates and unload/cache cleanup are checked separately.

The private check enters CDM twice and plays the unmodified authored denial cues
on `endtrigger`, `crutrigger` and one door-owned SelfTrigger. They decode
`sounds/events/cwvoice6.wav` (39,717 frames) and
`sounds/chosen/caleb/msccaleb1.wav` (13,664 frames). These are labelled direct-command
diagnostics, not a complete CDM playthrough.

SubwayA's SelfTriggers do not have authored lock sounds. Normal loading leaves
those properties empty. The existing NVIDIA train trigger fixture explicitly
borrows a CDM cue for its controlled lock scenario, just as it already borrows CDM
key assets. This does not add a production fallback. It retains the prior denial,
actual key pickup, consumption, hidden unlock, door motion and reload assertions,
and now also requires decoded audio progress and real SDL sample submission.

## Validation

Commands, outcomes and retained logs are listed in the
[evidence ledger](evidence/m2-trigger-audio.json). Debug's 88 selected behavioral,
resource, lifetime and private regressions pass, including continuous train routes
at 30 and 60 Hz. The same 88 checks pass in inspector-free Release, along with all
five selected NVIDIA checks. Clang ASan/UBSan with leak detection also passes the
same 88 checks. This is scoped validation, not a rerun of the whole suite.

NVIDIA Debug lifecycle, doors, interaction and level-change checks pass. The
extended trigger smoke initially exposed its missing SDL-output initialization;
adding that explicit smoke initialization produced 3,307.5 decoded source frames
advanced and 7,200 submitted SDL frames with the inspector hidden. The corrected
trigger check passes. Dummy output verifies sample submission, not human listening.
The selected Triggers panel capture was reviewed; captures remain in ignored build
folders. Native macOS execution of this change has not been tested.

Initial fixture failures are retained: a synthetic key lacked required icon paths,
and the first audio probe selected silent SubwayA triggers. CDM has real authored
feedback; moving that private probe there and explicitly labelling the GPU borrowed
cue corrected the fixtures without changing world loading or suppressing failures.
The first build also caught a smoke signature changed while its caller was being
updated; the final caller and declaration agree.

The preceding [door-audio audit](m2_door_audio.md) retains the unrelated sanitizer
clearance-test timeout and exploratory driver/libdbus GPU leak report. This scoped
increment does not erase those failures or certify full M2 acceptance. No timeout,
assertion, leak setting or dependency pin was relaxed.
