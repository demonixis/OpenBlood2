# M2 — Original door audio and regression audit

Sliding and rotating doors now play their original busy/stop WAV requests through
the existing mixer. Busy loops stop on replacement and destruction; stop sounds
follow their owner and finish independently. The Doors inspector distinguishes
requests, successful decoded voices, completion/removal and failed playback.
Original source behavior and ownership are specified in
[contracts](../../contracts/door_audio_contracts.md).

The source's fixed 500/1500 inner/outer radii and priority 1 are preserved.
`SoundRadius` is still authored metadata but is not read by original StartSound.
Missing replacement audio still stops the old loop and does not stop door motion.
Reverb/priority arbitration, Trigger lock sounds, portals and music remain absent;
no synthetic success or substitute audio was added. M2 remains incomplete.

## Regression audit

The initial full non-GPU Debug run passed 848/849 tests. The failing `exits.catalog`
still expected 232 objects after its delayed exit. The later original smell-trail
implementation adds one real SmellHint after start-point placement and the first
trail deadline. The test now checks exactly 233 objects, eleven attachments, one new
player drop, two live hints and an empty hint list after unload, across three
world lifetimes. No game behavior or milestone gate was changed to satisfy it.

A fresh rebuild also exposed `player.private`'s obsolete expected word
`autosave`. It now requires the current acceptance-gate diagnostic and explicitly
checks `SessionState::Blocked`; it still requires RunWorld to refuse. The initial
new audio fixtures wrongly supplied a rotating-only property to sliding Door;
the fixture was corrected to match each real class schema, with no schema bypass.

The first full NVIDIA run also caught an incorrect new smoke-test expectation:
the attached audio position is sampled before object motion, but the assertion
compared it with the later render position. The corrected check captures the
pre-step door position and verifies that exact sampled attachment, decoded cursor,
successful starts and SDL output submission. Simulation order is unchanged.

The private check decodes the authored hydraulic loop on seven SubwayA doors over
two reloads. The complete 30 Hz and 60 Hz train routes each record **28 successful
door sound starts and zero failures**, while preserving their previous ten Cultist
deaths and health/ammunition at CityHub entry. This exercises actual USE/contact
and exit behavior independently of the direct-trigger audio diagnostic.

Final commands and results are recorded in the
[evidence ledger](evidence/m2-door-audio.json).

The whole non-GPU suites pass **853/853** in GCC Debug and inspector-free Release.
The complete NVIDIA Debug suite passed 91/92 before the new attachment assertion
was corrected; the corrected door check and GPU lifecycle then passed 2/2.
The other 91 scenarios require no changed behavior. Eight relevant NVIDIA Release
checks also pass, including weapons, doors, triggers, interaction, objectives,
NEW_LEVEL and the continuous train route. The corrected door smoke records one
decoded start, zero failures, cursor 2940 and 7200 submitted SDL frames while
inspection is hidden.

An earlier `gpu.objectives` run exceeded its wall-clock deadline while builds
were active. It passed the subsequent complete Debug run and the Release check
with its timeout unchanged. Captures stay in ignored build directories; the
Doors panel was visually inspected. Dummy audio tests submitted decoded samples,
but this is not a human listening check.

The full Clang ASan/UBSan run, with leak checking enabled, passed **852/853**.
`ai_runtime.clearance_private` exceeded its existing 90-second timeout under
parallel load and again in isolation (90.12 seconds). A separate diagnostic run
with a 180-second process ceiling finished successfully in **91.98 seconds**:
both world lifetimes reported eight blocked resizes, 93 original AI shots and no
suspended Cultist; all corpse/lifetime assertions passed with no sanitizer report.
This diagnostic does not erase the failed CTest result. The configured timeout and
all test assertions remain unchanged. Earlier evidence already measured this
fixture at 86.06 seconds; wall-clock headroom is a remaining validation limitation.

A timing control relinked the same sanitizer test object and current libraries
with only the previous request-only Door implementation. It passed in **92.69
seconds**, with identical encounter results. This single comparison does not
establish a performance benchmark, but it gives no evidence that the new door
playback caused the deadline overrun. The control uses the current class layout;
it is not a rebuild of the entire previous commit and is not acceptance evidence.

## GPU leak investigation

The preceding full-train exploratory GPU LeakSanitizer failure also reproduces
with `--gpu-smoke`, without loading a world or player: 44,088 bytes in 816
allocations on this machine. Dynamic-loader maps place every unresolved frame
inside NVIDIA driver mappings (libnvidia-glcore/glsi, with historical address
reuse overlapping one gpucomp mapping). The remaining symbolized stacks enter
libdbus from SDL_DBus_Init or from the NVIDIA driver. The baseline log, loader
maps and address attribution are retained in the evidence.

This identifies external allocation sites; it is not proof that every GPU
scenario is free of application leaks. The baseline remains a failed unsuppressed
LeakSanitizer run. The SDL debug-only shutdown hint did not remove the report and
was not enabled in production. No global suppression, driver preload, changed
dependency revision or disabled leak checking is used to claim a clean result.
Headless session sanitizers and ordinary NVIDIA rendering tests are reported
separately. Native macOS validation remains user-confirmed from earlier work.
