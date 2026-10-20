# M2 — Original combat voice feedback

M2 remains incomplete. This increment connects original player pain/burning,
NPC kill/gib and postmortem corpse replies to the existing client VoiceMgr.
The [contracts](../../contracts/combat_voice_contracts.md) and
[source ledger](../sources/m2_combat_voice_sources.json)
record original selections and portability adaptations. User-edited README files
are preserved. No dependency, difficulty, damage or health balance changes are introduced.

## Behavior and inspection

Player feedback follows the original net change in truncated integer health:
one `69 + loss` chance draw per decrease, Fire selecting Burning and other effective
damage selecting Pain. Healing, armor/ammo-only changes and unchanged integer
health do not generate a voice draw. Initial/Load baselines start at zero, avoiding
false pain after restoration. The existing compact HUD still reports exact stats.

NPCs now deliver the previously recorded original seven-percent BigGib and
six-percent Kill requests. They require the local player to be the checked killer.
The accepted lethal packet supplies original WhoKilledMeLast attribution, distinct
from retaliation memory that instant-Death damage deliberately does not update.
Corpse gib replies retain the source's separate seven-percent draw and lack of a
killer filter. Typed values survive actor destruction; audio is retained by the
player's voice manager. Client busy/camera/cooldown arbitration remains intact,
and failed cosmetic feedback cannot reverse damage or prevent actor removal.

A checked lookup through existing faction membership replaces the original raw
single-player global. No new registry or engine service is introduced. The Session
inspector displays integer health, last loss/draw/event and actual audio status.
Skeleton displays NPC/corpse rolls, eligible requests and queued replies, explicitly
separate from playback success. Updates do not depend on inspector visibility.

## Verification

Generated damage fixtures cover multiple hits before one update, fractional health,
healing cancellation, Pain/Burning selection, busy refusal, forced stat resend,
single-player filtering, restored cache invalidation and pending-player rejection.

The private diagnostic floor runs 320 isolated original Cultist encounters:
64 ordinary deaths, 64 gib deaths, 64 ordinary deaths followed by corpse damage,
then 128 instant-Death cases with an earlier, different attacker.
Alternating player/non-player killers verify the NPC filter; corpse damage always
comes from a non-player. The fixed portable sequence produced four Kill (including
two instant-Death), three BigGib and six corpse replies. All thirteen actually
stream external WAVs and advance
PCM cursors after the NPC has been destroyed. No sampled death qualified for both
NPC replies; simultaneous source-order arbitration is not claimed as private
encounter evidence. Each trial explicitly stops the preceding voice; cooldown/busy
timing is tested separately. Cleanup prevents fixture effects from accumulating between
trials and checks final world/audio reclamation. Three ordinary encounters retain
the original no-hit-node early return without a voice draw; that guard is asserted.

GCC Debug and inspector-free Release each pass 98 relevant headless/private tests,
including 30/60 Hz continuous train traversal to CityHub, player restoration,
world transition, corpse/death effects and the existing audio callers. NVIDIA
RTX 3060 Laptop Vulkan passes `gpu.player`, `gpu.conversation_private` and
`gpu.death_effects` in both configurations. These existing GPU checks verify
presentation/voice-manager regression and lifetime, separately from the new
headless combat-caller assertions.

Clang ASan/UBSan passes the same 98 tests with leak detection and halt-on-error
enabled, with no sanitizer report. Final test logs and hashes are retained in the
[execution ledger](evidence/m2-combat-voices.json).

The 10,923 inventoried reference files remain unchanged and all six reference
repositories are clean. Local Markdown links pass; source ledgers preserve the
historical hashes. No runtime thresholds, chance values or test timeouts were
relaxed. The prior SDL/libdbus GPU-LSan residual was not retested in this increment.

Commands (run from `Blood2/`):

```sh
cmake --build build/debug -j 4 --target blood2 object_contract_tests player_tests player_state_tests player_restore_tests player_keepalive_tests player_voice_tests death_effect_tests ai_hit_tests corpse_tests conversation_tests pickup_audio_tests weapon_drop_tests screen_fade_tests world_tests train_session_tests death_effect_gpu_tests
ctest --test-dir build/debug --output-on-failure -j 2 -R '^(objects\.|player\.|player_state\.|player_restore\.|player_keepalive\.|conversation\.|pickup_audio\.|weapon_drops\.|screen_fade\.|player_voice\.|ai_hit\.|corpses\.|death_effects\.|world\.(level_change|level_change_private|session_lifecycle|session_messages)$|train_session\.)'
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json SDL_AUDIODRIVER=dummy ctest --test-dir build/debug -V -R '^gpu\.(player|death_effects|conversation_private)$'
```

The same test selection runs in `build/release` and `build/sanitize`; sanitizer
execution enables `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. Build logs record the relevant application/test
targets (the sanitizer build has no GPU frontend). Raw logs retain tool-produced
whitespace and are excluded from prose/source whitespace checks.

## Remaining limits

These selected callers do not implement the full original player DoDeath/suicide,
idle, spawn, taunt, powerup or jump paths. Interactive IMA music and the consolidated
M2 acceptance gate remain open. Existing missing unique-voice resources remain
explicit failures. The diagnostic encounter fixtures are not a retail playthrough
or a comparison of random sequences. NVIDIA tests cannot establish native Metal
or Windows behavior, and dummy audio output is not a human listening check.
