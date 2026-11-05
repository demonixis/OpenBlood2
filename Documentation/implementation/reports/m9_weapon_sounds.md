# M9 — Validated weapon animation sounds

2026-10-31, following `ffa68c66`. Network weapon animations now produce the
original attached SOUND_KEY, SOUNDLOOP_KEY and SOUNDSTOP_KEY playback through
the server's checked CViewWeapon timeline. Independent clients receive the
existing reliable audio events and mix actual PCM. See the
[contract](../../contracts/multiplayer_contracts.md#validated-weapon-animation-sounds)
and [source ledger](../sources/m9_weapon_sound_sources.json).

## Original behavior and boundary

The active original ViewWeapon sends all three key actions to PlayerObj;
the nearby direct local playback block is commented out. PlayerObj owns one
retained weapon loop shared by both hands. A replacement first kills that loop,
even if the replacement resource fails. A one-shot leaves it alone. Stop, view
replacement/removal, actual death and player destruction release it. Attached
playback retains the original outer/inner radii 1000/100, priority 2, buffered
loading and reverb request. The current mixer still does not implement reverb.

WeaponServices carries a typed action and resource name, without sound handles.
The common parser retains doubled-separator normalization and missing-argument
guards. The solo adapter retains its previous shotgun-only local one-shot
behavior. Network presentation predicts keys without playing PCM; server-owned
AnimationOnly resources determine the action and filename. There is no new
client sound command, wire schema, RNG draw or movement integration. Protocol
14 and existing B2AU bounds remain unchanged. Missing playback is diagnostic;
it does not undo damage or fail the simulation.

Sound validation checks the world/player/object, current character, selected
weapon, hand view revision and life. A position correction does not change a
sound's life: delayed pre-correction SOUNDSTOP must still release its loop.
The last issued dead-loadout sequence rejects keys from an earlier life,
including death and respawn between input frames. View replacement stops the
shared loop at authoritative publication, before its acknowledgement. Adding
a previously absent second hand does not terminate the existing hand's loop.
FIRE keeps all current force, pose, movement, camera, ammo and combat checks.

## Verification

**28 distinct native tests, nine ASan/UBSan tests and three inspector-free
Release tests pass.** The Release cases repeat cases counted in the native set.
Commands, raw outputs, failures and check metadata are retained under
`evidence/m9_weapon_sound_*_2026-10-31.*`.

- Independent retail CViewWeapon coverage checks 39 firing modes, 595 predicted
  requests, 107 sound keys (including two loop and two stop keys), and identical
  presentation/AnimationOnly timing. All 22 referenced WAV resources decode.
  Synthetic parsing checks mixed-case keys, normalized paths and missing args.
- The dedicated Crypt fixture drives real shotgun keys and the retail Minigun
  loop/stop timeline. Two independent clients produce PCM peak 1 without local
  prediction playback. Duplicate frames, obsolete views and preceding-life
  keys are inert. Teleport followed by a delayed release stops the Minigun loop.
  Replacement failure, shared ownership, selection, death and departure clean
  up correctly. Five stale sound actions are blocked; movement steps remain 0.
- Dedicated Blood Arena with two real client processes: three shotgun shots,
  five shells remaining, one frag/death, three impulses and health-100 respawn.
  Each client observes an authoritative animation sound and PCM peak 1. Native
  playback starts 58 server voices (one explicit stop, 55 natural retirements);
  sanitizer playback starts 57 (one stop, 54 retirements). Both runs finish
  2,400 server ticks with no discarded ticks and two admissions/departures.
- Graphical Fields host with two real remote combatants: three shotgun shots,
  five shells remaining, one frag/death, three impulses and health-100 respawn.
  Both remotes observe animation playback and PCM peak 1; each starts 53 server
  voices, with 53/52 natural retirements and no explicit stops. The host renders
  1,200 frames, advances 3,610 server ticks with **eight discarded server ticks**,
  zero discarded local ticks, and three admissions/departures. Host device
  output is dummy; this does not qualify physical audio or load capacity.
- Existing Beretta process combat, server fire/death, local weapon lifetime/
  rollback, shotgun behavior/FX and native save/load checks pass. Five solo
  traces are byte-identical to the preceding lot: effects 240 frames, audio 360,
  movement 360, environment 720 and camera/HUD 300. Server logical audio clocks
  and CMoveMgr's single integration remain unchanged.
- Linux/NVIDIA host and solo shotgun GPU checks pass. Inspected captures show
  Fields terrain/sky, held Beretta, crosshair/HUD, and the isolated original
  shotgun primary/secondary flashes and casing. Capture hashes are recorded;
  no golden images changed. The native and sanitized independent weapon/PvP
  executables contain no server adapter/session or original gameplay symbols.

Focused reproduction commands, using the existing private-data configurations:

```sh
ctest --test-dir build/m9-fx-runtime -V -R '^(network\.(weapon_services(_private)?|weapon_local_.*|server_fire|server_death|weapon_sounds_private|weapon_sound_processes|pvp_processes)|shotgun\..*|shotgun_effects\..*|save_native\..*)$'
ctest --test-dir build/m9-fx-runtime-asan -V -R '^network\.(weapon_services(_private)?|weapon_local_.*|server_fire|server_death|weapon_sounds_private|weapon_sound_processes)$'
ctest --test-dir build/release -V -R '^network\.(weapon_services(_private)?|weapon_sounds_private)$'
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json ctest --test-dir build/m9-fx-runtime -V -R '^gpu\.(shotgun_effects|network_weapon_sound_host)$'
```

Project builds produce no compiler warnings in this lot. The unchanged external
effects-oracle helper retains its misleading-indentation warning. All **6,791
protected hashes** remain unchanged, including PBR files, immutable captures and
previous evidence. No commercial resource is committed.

Three intermediate failures are retained. The first Minigun fixture supplied
BMG ammo although the source uses bullets; the fixture now supplies bullets.
The first Fields shotgun script exhausted its eight shells at excessive range;
the script now approaches within 250 horizontal units before firing. Neither
fix changes game rules. The teleport regression exposed a real implementation
defect: using FIRE's force gate for audio rejected the delayed stop and stranded
the loop. Separate life/view validation fixes that defect without weakening
authoritative fire checks. Every failing case subsequently passes.

## Remaining scope

Frag/FinishHim announcements need the remote player roster and original text/
voice selection. Music, inventory activation and jump/impact cosmetics, remote
visual effects/lights/fog, Wonky visuals, scoreboard/chat, match limits/rotation,
remaining multiplayer branches, Team/CTF, LAN discovery and Host/Join screens
remain in progress. These tests do not qualify human LAN, physical speakers,
two-player combat on all 22 maps, 16-player load, impaired combat/audio,
Windows/macOS interoperability or mobile/Quest execution. M9 remains active;
M2-GPU-001 and M3 acceptance remain separate.
