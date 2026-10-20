# M2 continuation: original weapon fire sounds and automatic hearing

Beretta and Chosen Knife attacks now play their original WAV resources through
SDL audio and alert NPCs through original `SendSoundTrigger`. The original four
retained voices, earliest-end replacement, completion polling and destruction
cleanup are connected. The player Weapons inspector and selected AI weapon panel
show actual playback state and separate hearing/audio failures. **M2 remains open.**

See [contracts](../../contracts/weapon_audio_contracts.md),
[source ledger](../sources/m2_weapon_audio_sources.json) and
[evidence](evidence/m2-weapon-audio.json).

An accepted shot consumes ammo, attempts playback, broadcasts hearing and then
applies its vector impact. Missing WAVs still alert NPCs. The selected definitions'
empty dry-fire filenames produce no invented click. Source radii, attached position,
player/NPC priorities, first-empty slot and strict end-time tie order are retained.
The existing mixer outputs dry spatial audio; reverb and priority arbitration remain
explicitly unavailable. Client model-key sound references do not create a second voice.

The collision preflight continues to reject unavailable geometry before ammo use.
Hearing callbacks can alter lifetime or placement: the final ray is refreshed after
eligible delivery, including a callback that moves its target and then fails. Owner
removal records an interrupted accepted shot, releases its new voice and skips impact.
Deferred destruction and existing firing guards preserve the active weapon's lifetime.

Synthetic checks cover four voices, tie and earliest-end eviction, failure without
evicting existing voices, attached motion, decoded duration, completion after changing
weapons, player/NPC priority, actual hearing reaction, missing WAV, dry fire, unavailable
collision, callback removal/motion/failure and inventory destruction. Private checks
load real Beretta/knife primary/alternate audio across three SubwayA/player/unload
cycles. Direct audio probes during the intro are explicitly diagnostic; the existing
private weapon test separately fires through original client animation keys after
the intro and now checks successful audio for every shot. The destination rehearsal
check additionally preserves live source weapon voice IDs, slots and cursors.

The GPU weapon scenario verifies primary/alternate weapon activity, actual decoded
voices and SDL sample submission with the inspector hidden, then unloads/re-enters.
No screenshot or path-index lookup is used as proof that audio played.

## Validation

Debug passes **562/562 headless checks** (80.41 s)
and **41/41 GPU checks** (172.2 s). Inspector-free Release passes
**562/562 headless checks** (8.64 s) and **41/41 GPU checks**
(108.86 s): **603/603 per configuration**, including 120 private-data
checks (88 headless). Targeted Clang ASan/UBSan/leak checks pass **98/98**
(188.36 s). Neither GPU matrix emits Vulkan validation errors/VUIDs.

Commands: `cmake --build build/debug -j3`, `cmake --build build/release -j2`,
then `ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and `ctest --test-dir build/<configuration> -L gpu --output-on-failure`. Sanitizers
use existing Clang `build/sanitize`, targets weapon_audio_tests, weapon_tests,
weapon_drop_tests, object_contract_tests, ai_trigger_tests, ai_fire_tests,
ai_runtime_tests, world_tests, player_restore_tests and player_keepalive_tests,
followed by `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest`
with regex `^(weapon_audio|weapons|weapon_drops|objects|ai_trigger|ai_fire|ai_runtime|world|player_restore|player_keepalive)\.` and `-j2`.

`--inspect-player` exits 0; `--start-game` retains exit 6 for incomplete gameplay.
15 changed C++ files pass clang-format 23.1.3; 2,459 local Markdown links resolve.
All 10,923 reference files and six repositories remain unchanged. The evidence
contains the project commit, patch/hashes, test selection and detailed audio/GPU logs.
The Debug capture was inspected for readable live slot/counter/error presentation;
captures remain ignored. Audio claims rest on behavioral/mixer/SDL checks.

Initial failing records are retained. The hearing fixture first forgot the player's
owned name string; it now checks the pre-shot string baseline. The deliberate
message failure fixture first encountered original end-of-message rewind; it now
reads a deliberately invalid string length, preserving all failure/ray assertions.
The GPU scenario initially omitted audio-device startup; it now explicitly opens
SDL output and refuses failure. The reviewed panel was moved above the pose and
long text wrapped. No game behavior or acceptance assertion was weakened.

Reference sources, external assets and dependency pins remain unchanged. No retail
execution/audio comparison, other OS or XR validation is claimed. SubwayA accounting
remains 203 created, four transferred sounds, 372 editor-only, one unsupported Gideon,
zero failures. Safe NEW_LEVEL, required world services/illumination and the actual M2
gate remain incomplete; M3–M8 remain later milestones.
