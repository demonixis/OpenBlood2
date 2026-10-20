# M2 — original single-player death and camera

CPlayerObj now runs the selected original HandleDeath/DoDeath path after a lethal
hit. The client receives SMSG_DEAD, hides its weapons and uses the original death
camera orbit. Ordinary death creates an independent animated Caleb corpse; the
source gib decision invokes the existing real fragment/limb services. M2 remains
incomplete. Full world saves, active-power/multiplayer death and interactive music
remain separate gaps.

The implementation preserves the original null-killer guard, once-only commitment,
Suicide/Death voice selection and stats-before-death ordering. It also preserves
less obvious source behavior: EXPLODE is a bit test (so Fire/Melee take it), the
player gib fraction threshold is literally 50.0, inventory removal includes only
the first key, and fresh Caleb's separate player node/side fields select death
slot 1. Corpse sound directory remains empty. No balance adjustment or respawn
shortcut was introduced.

The player and client persist after death. Existing movement/fire/item/weapon
selection guards remain; original single-player USE remains allowed. Corpse and
fragment failures are exposed without duplicating death on following frames.
Player removal does not destroy the corpse. Loading an already supported live
checkpoint resets the death camera; restoring the dead body/corpse together is
still explicitly unavailable.

The camera uses the original .8 rad/s orbit, 108-unit radius, 30-unit target offset,
35-unit elevation and one-second transition, including the source's double basis
rotation. The existing checked ray supplies obstruction placement. Zero initial
look vectors retain a valid orientation; the inspector reports actual state and
failures. See [contracts](../../contracts/player_death_contracts.md)
and [source ledger](../sources/m2_player_death_sources.json).

## Verification

Implementation commit: `61f0be6c046131db6ebdcaad4ac02348b59a7154` in the independent
Blood2 repository.

| Configuration | Targeted behavior/lifetime tests | NVIDIA GPU checks |
| --- | ---: | ---: |
| GCC Debug, inspector enabled | 113/113 (61.75 s) | 4/4 |
| GCC Release, inspector disabled | 113/113 (9.70 s) | 4/4 |
| Clang Debug, ASan/UBSan, inspector disabled | 113/113 (330.19 s) | Not run |

The three new player-death tests cover generated camera/guard fixtures and five
private death paths with five live-checkpoint re-associations. The latter verify
one ordinary animated corpse, three original gib paths, the freefall omission,
first-key-only removal, active flashlight cleanup, retained weapons/ammunition,
removed-killer attribution, command gates and independent corpse lifetime. Final
unload leaves no object records or audio voices. Existing session, camera,
inventory, damage, voice, restoration and 30/60 Hz train-to-CityHub checks pass.

The GPU checks ran on **NVIDIA GeForce RTX 3060 Laptop GPU / Vulkan** in both
builds. The death-effects test draws the actual Caleb corpse from the computed
death camera: **4,959 non-background pixels**, identical repeated readback, corpse
survival after player removal and clean client/GPU teardown. Its local ignored
image was visually inspected; no commercial screenshot or asset is committed.
The other three GPU cases cover player, scripted cameras and conversation lifetime.

[Machine-readable evidence and commands](evidence/m2-player-death.json) records
builds, configurations and log hashes. The focused [behavior log](evidence/m2-player-death-behavior.log),
[Debug](evidence/m2-player-death-debug-tests.log),
[Release](evidence/m2-player-death-release-tests.log) and
[sanitizer](evidence/m2-player-death-sanitize-tests.log) logs contain the assertions
and results. GPU output is recorded for [Debug death](evidence/m2-player-death-debug-gpu.log),
[Debug regressions](evidence/m2-player-death-debug-gpu-regressions.log) and
[Release](evidence/m2-player-death-release-gpu.log). Source ledger hashes match;
10,923 reference inventory files and all six reference repositories remain unchanged.
The user's workspace README edit is preserved and excluded from this run's commits.

Private death tests use external commercial assets on a labelled diagnostic floor;
they do not claim authored player-death encounter parity or a full retail
playthrough. Audio uses the dummy output driver for automation, not a human
listening test. Native macOS/Windows execution is not claimed. The previously
recorded SDL/libdbus GPU LeakSanitizer residual was not retested; sanitizer results
above are headless. The final inspector text was rebuilt after validation to
clarify that dead single-player USE remains allowed; it changes no behavior.

## Remaining gate

M2 remains incomplete. This increment closes the supported Caleb death presentation
and client notification path, while full world/dead-player restoration, active-power
and multiplayer death, interactive IMA music and the other outstanding service
contracts still require real implementation and observable validation. Neither the
private diagnostic death fixtures nor successful build counts close those gaps.
