# M9 — Bloodbath death, slow death and respawn

2026-10-31, following `81fe4cfa`. Original Bloodbath life transitions now execute
on the server, with bounded protocol-v12 death/frag events and independent weapon
cancellation. This lot verifies rules and replica boundaries. Interactive held
fire remains neutral pending remote damage impulses and real-process lethal PvP;
audio/FX, match rules and Team/CTF still need their subsequent lots.

## Source behavior

HandleDeath drops a carried flag before checking the lethal attacker. Null lethal
identity still postpones death. For a non-self multiplayer kill, the first update
sets the ordinary-death sentinel or starts slow death; confirmation occurs on a
later update. The actual source chance is **10%**, only when accumulated death HP
is below −5, despite its 25% comment. Self kills bypass this chance.

Slow death retains 15 seconds, a 0.5-second finishing-hit grace and a 5-second
minimum stay. Safe/stay timers decrement before the ordinary player update; the
death timer decrements in HandleDeath. Expiry clears the slow flag and waits one
more update before confirming death. Float clocks and their strict comparisons
are preserved. USE after the minimum stay clears slow death, heals one HP and
ends WonkyVision; earlier USE does nothing. Ordinary Heal refuses a slow-dying
player, and Life Leech cannot heal a slow-dying attacker. Source MegaHeal remains
separate. Movement/fire permissions are disabled while damage-dead, but the
client's confirmed-dead state starts only at PlayerDead.

A damage message after the safe interval confirms humiliation before the damage
aggregate reads the incoming hit. The **original lethal attacker** therefore gets
the frag, including when another player supplies the finishing hit. The first
three humiliation animations are selected once through server RNG. The fourth
loops on humiliation death without an ordinary corpse. Changed animation extents
use checked resizing; contact transactions defer a resize, never weaken collision.
The existing retail ABC mappings supply these clips. The visible humiliation body
produces original gibs on respawn. Ordinary corpse/gib rules remain unchanged.

Confirmed death awards +1 to the still-connected killer, or −1 for self, and
records original name attribution. Removed killers get no award, broadcast or
random draw. One random announcement index is drawn per frag, then broadcast to
all connected recipients; recipient count does not multiply server RNG draws.
Finish-him announcements remain addressed to the checked original killer.

USE respawns the same player/inventory and invokes the original start selection.
Health 100, armor 0, air 100, configured multiplayer ammo, reset motion/life timers
and cleared remote bombs are retained. The source shared creation-time quirk is
world-owned: each newly constructed player resets the timestamp used by the
65% spawn-voice chance after five seconds. The existing checked occupied-start
placement and source marker draw remain unchanged. If respawn/placement fails,
the player stays dead at the previous position, hidden, with an explicit error
and a negative command ACK; a later USE may retry. Already executed respawn side
effects are not rolled back. No frag is awarded again. Multiplayer fields are
explicitly refused by the solo save boundary.

## Replication and cancellation

Protocol v12 appends WonkyVision (bounded duration/no-move), FinishHimVoice
(index/two valid Chosen IDs), and FragEvent (generation-qualified victim/killer,
bounded signed score, announcement index). Scores use an explicit biased LE u32;
no pointer or native signed representation crosses the wire. Existing queue,
size, owner, world and sequence bounds apply. Private event decoding remains
atomic on malformed/truncated input.

The network camera distinguishes slow death from confirmation and retains the
original no-hide flag when PlayerDead arrives during no-move WonkyVision. It shows
the humiliation body during the death orbit and resets that flag on the new force.
This is not the Wonky FOV/roll/model-warble effect: those visual consumers remain
unconnected, as do finish-him/frag announcement playback and the scoreboard.

Death invokes original CViewWeapon::CancelFiringState: reset, rest animation,
reset, REST; retain reload/barrel/RNG rather than recreating the weapon. Both
client and server do so on each dead frame and on each received dead loadout.
PlayerDead immediately captures the actual dead loadout before a subsequent USE
can coalesce death and respawn into one live publication. The server consumes
every intervening issued loadout, mirroring the reliable client stream even when
no weapon frame ran between states. Old-life commands/fire still fail the full
force identity after respawn. No CMoveMgr code or movement integration changes.

## Reliable weapon acknowledgements

The GPU host regression exposed a previous transport problem: both clients
filled their 64-frame input window although the server continued processing.
The instrumented host had processed 166 frames while its client had received ACK
106; neither server nor client had discarded ticks. Rechecks reproduced the
failure without concurrent sanitizer work. Those failures remain recorded.

Weapon ACKs now use reliable Control at the existing 20 Hz publication cadence,
only when their processed sequence changes. They release a bounded reliable input
window, so best-effort State loss/throttling must not suppress that progress.
Visual/movement snapshots remain on State; no queue bound or input budget is
increased. The endpoint validates the new ACK channel, and failure diagnostics
include sent/acknowledged frames and clock counters. Protocol v12 includes this
channel change. A real ENet fixture discards every State delivery and verifies
180 weapon frames are still acknowledged on Control within the bounded window.

## Verification

The dedicated retail `multi/bb_crypt` fixture uses three connected original
players and real typed damage. It covers ordinary confirmation delay/corpse,
slow safe/stay/expiry boundaries, healing refusal, USE rescue, humiliation and
gibs, first-killer credit, self −1, exact single broadcast RNG draw, private
announcement recipients, removed/null killer guards, ammo/health reset, stale
USE refusal and failed-start retention. It never creates a client audio device.
It performs zero server movement extrapolation steps; this is a rules fixture,
not a remote impulse or interactive combat qualification.

The real-animation firing fixture engages an attack, confirms death and respawns
before any intervening client tick/publication. It observes both loadouts, retains
the hand revision and proves the pending shot never occurs on either timeline.
Independent camera fixtures verify visible humiliation, duplicate-death stability
and force reset. Wire tests now cover all 25 private tags, every truncation and
37,500 mutations, including negative scores and invalid identities/characters/clocks.

**39 distinct native tests and 9 ASan/UBSan tests pass after the ACK correction.**
The native set includes the eight save-session cases, original solo death cases,
seven connection/profile cases, admission with 16 peers, movement/weapon UDP,
separate client processes, the solo GPU weapon smoke and graphical host/remote.
The final host runs 900 rendered frames, the visitor 180, with two clean admissions/
departures and no discarded client or host ticks. The host capture was visually
checked for world, Beretta, HUD and crosshair. This is graphical regression coverage,
not lethal PvP or a human LAN match. The four pre-correction GPU failures are
retained in the inventory and their raw logs.

Five rebuilt solo oracles remain byte-identical: effects (240 frames), audio (360),
movement (360), environment (720) and view (300). All 6,704 protected hashes remain
unchanged, including existing PBR files and immutable references. The independent
process client still contains no ServerAdapter, CBloodServerShell, GameSession,
Objects, CWeapon or CInventoryMgr symbols. Final builds have no new warnings;
the external effects-oracle helper retains its known indentation warning. The
sanitizer build has client/renderer/inspector targets disabled.

[Check inventory and capture hash](evidence/m9_network_death_checks_2026-10-31.json),
[final UDP/process/GPU checks](evidence/m9_network_death_ack_final_tests_2026-10-31.txt),
[rules and solo death](evidence/m9_network_death_native_before_ack_2026-10-31.txt),
[save/session regressions](evidence/m9_network_death_regression_tests_2026-10-31.txt),
[sanitized rules](evidence/m9_network_death_asan_tests_2026-10-31.txt),
[sanitized ACK/process checks](evidence/m9_network_death_asan_ack_tests_2026-10-31.txt),
[solo oracles](evidence/m9_network_death_solo_oracles_2026-10-31.txt),
[preservation/linkage](evidence/m9_network_death_preservation_2026-10-31.txt).

Commands used in the isolated native and headless sanitizer trees (the inventory
counts distinct tests across bounded runs):

```sh
cmake --build build/m9-fx-runtime -j6 --target blood2_app blood2_server network_server_death_tests network_server_fire_tests network_camera_wire_tests network_player_event_tests network_admission_codec_tests network_player_command_tests network_weapon_frame_tests network_server_player_event_tests network_server_player_command_tests network_server_weapon_loadout_tests network_admission_tests network_admission_process_tests network_client_endpoint_tests network_client_movement_tests network_player_profile_tests player_connections_tests player_death_tests save_session_tests
ctest --test-dir build/m9-fx-runtime -V --output-on-failure -R '^network\.(server_death|server_fire|camera_wire|weapon_frames|player_commands|admission_codec|player_events)$|^player_death\.|^gpu\.(network_host|weapons)$'
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(server_player_events|server_player_commands|server_weapon_loadout|server_camera|client_endpoint|client_movement|fire_processes|admission_processes|movement_processes)$|^save_session\.'
ctest --test-dir build/m9-fx-runtime -V --output-on-failure -R '^network\.(weapon_ack_udp|admission_private|movement_udp|fire_processes|client_endpoint)$|^gpu\.network_host$'
cmake --build build/m9-fx-runtime-asan -j4 --target blood2_server network_server_death_tests network_server_fire_tests network_camera_wire_tests network_player_event_tests network_weapon_frame_tests network_player_command_tests network_admission_codec_tests network_admission_tests network_admission_process_tests
ctest --test-dir build/m9-fx-runtime-asan -V --output-on-failure -R '^network\.(server_death|server_fire|camera_wire|weapon_frames|player_commands|admission_codec|player_events|weapon_ack_udp|fire_processes)$'
```

## Remaining work

Remote movement currently ignores damage-dead reports and does not yet deliver
server damage impulses to CMoveMgr. Port that boundary, preserve passive momentum
without double integration, and exercise two actual client processes killing and
respawning before enabling frontend held fire. Then complete remote audio/FX,
weapon-specific multiplayer branches, match limits/rotation, Team/CTF, scoreboard,
chat and LAN/menu flows. No Windows/macOS interop, human LAN, degraded combat,
22-map interactive sweep or 16-player combat qualification is claimed.

[Source/adaptation ledger](../sources/m9_network_death_sources.json).
