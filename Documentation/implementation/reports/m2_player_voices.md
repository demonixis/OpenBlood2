# M2 — Original player voice manager

M2 remains incomplete. This increment connects original weapon-pickup replies,
player SOUND commands and conversation interruption to actual local WAV playback.
It adds no replacement dialogue, dependency, changed difficulty or forced chance.
The user-edited README files are preserved.

## Source and behavior

Original `ClientShellDLL/VoiceMgr`, selected `ObjectDLL/PlayerObj` helpers and the
shell/CCommLink callers are adapted to typed owned messages and the existing audio
services. The [contract](../../contracts/player_voice_contracts.md) records
selection, gates, timing, lifetime and omissions. The [source ledger](../sources/m2_player_voice_sources.json)
records donor ranges, hashes and adaptations.

The client discovers four characters' twelve groups with original custom-directory
precedence and 64-entry/128-byte limits. A single entry consumes no random draw;
multiple entries avoid the previous selection with the source's bounded retry.
Unique Caleb replies are marked played only after successful streamed playback.
Busy voice, external camera and strict cooldown gates are preserved. Jump is the
original buffered exception. Client randomness is separate from server gameplay,
and sorted archive enumeration is an explicit portability adaptation.

Successful weapon collection now executes the original optional voice branch,
including its previously absent second chance draw. SOUND stops VoiceMgr before
requesting a separate high-priority local one-shot. Conversation messages stop it
even when the communication panel is busy, and an actually started cue postpones
subsequent speech. These operations do not stop ambient sounds or footsteps.
Playback failure remains cosmetic and inspectable rather than reversing a pickup.

Safe NEW_LEVEL preserves unique flags, enabled state and matching-group/client
random history by value. It never transfers a sound handle or source-world timer.
Rejected transitions retain the source state and playing voice. Fresh entry resets
unique flags. The Session inspector accounts for empty/custom groups, rejected
paths, active/completed/stale voices, unique flags, cooldowns, queued messages,
starts/refusals/failures and SOUND requests. Cinematics reports actual stop/delay
arbitration. Reading the UI does not run the logic.

## Installed resources

The configured external version-2.1 installation exposes 44 nonempty groups with
431 WAV entries across the four base characters. This is enumeration evidence,
not a claim that every entry has been decoded. Seven of the nine unique Caleb
references stream successfully with advancing PCM cursors. Two source references
are absent from the mounted index:

- `sounds/chosen/caleb/unique/msc2_cal.wav` (Tesla unique).
- `sounds/chosen/caleb/unique/msc9_cal.wav` (Shotgun unique).

Tests require these to fail without setting played flags; no alias or placeholder
is supplied. The ordinary Weapon group remains independently subject to the
original optional chance and busy gate. No commercial bytes were imported.

## Verification

Final results and exact logs are recorded in [the evidence ledger](evidence/m2-player-voices.json).
Validation covers generated-fixture selection/bounds, strict cooldown, streaming
versus buffered Jump, failed replacement, missing/stale owner lifetime, client
validation/disconnect, SOUND parsing/single-player/dead-player behavior, actual
weapon-pickup callbacks, conversation preemption, and private rollback/CityHub
promotion. Forty deterministic pickup trials reach four original 15% branches
and one actual group reply; the mapped M16 helper is tested separately.

The existing continuous train runs at 30/60 Hz remain unchanged: normal input,
combat and contacts reach the authored exit and CityHub without teleporting or
editing health/ammo. Separate diagnostic voice tests do not claim that every
speech caller occurs during that playthrough.

GCC Debug and inspector-free Release builds pass 58 relevant headless/private
checks each. Clang ASan/UBSan passes the same 58 checks with leak detection enabled
and no sanitizer report. Existing timeouts and behavioral thresholds are unchanged.
NVIDIA RTX 3060 Laptop Vulkan passes `gpu.player` and `gpu.conversation_private`
in both Debug and inspector-free Release, with no reported VUID/validation errors.
The existing SDL/libdbus GPU-LSan residual is not re-tested or claimed resolved.
The NVIDIA conversation smoke additionally requires one real streamed unique
reply per preparation, PCM submission, interruption by the actual queued
conversation, hidden-inspector updates, unload/reentry and completed cleanup.
Counters span reloads; the test checks the start delta per preparation rather than
mistaking the cumulative count for a fresh-world count. The inspector-free Release
path uses the same logic. No native macOS/Windows run was available for this change.

The initial private test incorrectly assumed all nine referenced files existed;
the catalog probe identified the two absences above. The initial synthetic pickup
message lacked the required live sender; the final test uses SpawnWeaponPickup
and its original touch callback. GPU diagnostics initially selected the absent
Shotgun file, then exposed the cumulative-count assumption across reload. Final
checks retain the original playback, interruption and lifetime requirements.

## Remaining acceptance

This implements selected callers, not every VoiceMgr idle/pain/death/jump request,
addon character, voice-priority arbitration, original communication HUD or
serialized voice history. PCM streaming does not add compressed streaming support.
The reached MUSIC command remains unsupported: source inspection identifies old
interactive IMA `.sec`/`.sty`/`.dls` resources, requiring a distinct reader and
playback implementation. No music success stub was added. Required-service and
consolidated M2 acceptance remain open; later roadmap gates are unchanged.

Reproduction from `Blood2/` (configured external content and GPU tests enabled):

```sh
cmake --build --preset debug -j 8
cmake --build --preset release -j 8
ctest --test-dir build/debug --output-on-failure -j 2 -R '^(objects\.|player\.|conversation\.|pickup_audio\.|weapon_drops\.|screen_fade\.|player_voice\.|world\.(level_change|level_change_private|session_lifecycle|session_messages)$|train_session\.)'
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json SDL_AUDIODRIVER=dummy ctest --test-dir build/debug -V -R '^gpu\.(conversation_private|player)$'
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/sanitize --output-on-failure -j 2 -R '^(objects\.|player\.|conversation\.|pickup_audio\.|weapon_drops\.|screen_fade\.|player_voice\.|world\.(level_change|level_change_private|session_lifecycle|session_messages)$|train_session\.)'
```

Release verification built the application and relevant tests; sanitizer
verification built the relevant headless tests. The log ledger preserves the
separate 48-test and ten weapon-drop batches.
Debug has a consolidated 58-test final run. The full Debug project also compiled.
GPU automation uses SDL dummy audio: decoded samples, cursors and submitted PCM
are measured, not human listening. A captured inspector frame was visually checked.
The reference integrity check finds all 10,923 inventoried files unchanged and all
six reference repositories clean. No dependency pin or commercial content changed.

Implementation commit: `cf5a39a13767d3ff7b1b1f0c74a422be61ab945d`.

Raw tool logs are retained byte-for-byte, including CMake/CTest trailing spaces.
Whitespace checks apply to code and maintained documentation, excluding raw logs.
