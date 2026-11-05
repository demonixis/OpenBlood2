# M9 — Basic Bloodbath combat across real processes

2026-10-31, following `ec399fc2`. The graphical network frontend now forwards
held primary/alternate fire through the existing independent NetworkClient.
Selection, USE and alternate-zoom commands stay ahead of the corresponding
weapon frame on reliable Control. Existing mouse/keyboard/gamepad bindings,
menu/focus suppression, original primary priority and dual-hand alternate rules
are retained. CMoveMgr, server weapon cadence, damage, death, impulses and solo
logic are unchanged by this frontend connection.

The dedicated command-line status now identifies the connected Bloodbath combat
and respawn loop. This is not full M9 acceptance: remote audio/FX, scoreboard/chat,
match limits/rotation, remaining multiplayer weapon/creature branches, Team/CTF,
LAN discovery and original host/join menus still remain.

## Independent-process combat

The new `network.pvp_processes` fixture launches the actual `blood2-server` plus
two separately linked client executables over IPv4 ENet. Each client owns its
own NetworkClient, resources, collision replica, CMoveMgr and view weapons. No
server gameplay pointer or direct damage injection is available in the client
process. Test movement and aim use only public client inputs and replica queries.
No authoritative teleport, health override, random-seed override or commercial
asset modification is used.

On retail Blood Arena, the attacker walks until the victim is visible and holds
Beretta primary fire. The victim observes decreasing private HUD health,
reliable damage impulses, exactly one PlayerDead and the addressed broadcast
FragEvent. The attacker observes exactly one +1 frag, actual shot acknowledgements
and spent ammo. Half a second after confirmed death, the victim sends USE through
the ordinary command channel, then requires alive weapons, a newer force identity,
health 100, one accepted command ACK and a reset impulse sequence. After departure,
the attacker observes the retired remote identity and disconnects. The server
continues to 2,400 ticks and shuts down with both clients departed.

The passing native run recorded 14 shots/ammo 36; ASan/UBSan recorded 13 shots/ammo
37. Both recorded 13 damage impulses, one death, one frag and respawn force 2.
The final held frame can produce another valid shot before the frag reaches its
sender; this is not another damage/death delivery. Slow death, finishing-hit
attribution and old-life fire refusal retain their separately tested source rules.
This single encounter does not qualify every weapon, all maps or impaired combat.

The first fixture attempt assumed the new alive loadout, force, HUD and command
ACK were one atomic publication. Original Respawn publishes its force before the
next UpdateStats. The fixture now waits for all required observations rather than
failing on the intervening frame; no gameplay timing was changed.

## Graphical host verification

The host fixture runs the actual graphical application with one local player and
two independent remote combatants. Distinct Ophelia/Ishmael profiles let the
clients select each other without targeting the idle Caleb host. Host rendering
and simulation remain active while remote clients run. This is an automated
session; it is not a human LAN match or a claim that a human fired from the host.

The Fields host run passes: **33 actual shots, ammo 17, 13 victim impulses, one
frag/death, health-100 respawn at force 2**, three admitted/departed connections
and 1,200 rendered frames. The local client discarded zero ticks; the server
reported eight discarded ticks while running 3,610 simulation ticks. That
reported overload is not hidden or treated as a capacity qualification. Its
capture was inspected for world, sky, held Beretta, crosshair and original HUD;
no reference capture was replaced.

Two Blood Arena attempts retained collision
correctly but the scripted clients could not navigate its separating walls; no
shots occurred and both test deadlines failed. A read-only original-spawn probe
on Chosen Arena, Dustbowl, Fields and Cabana identified direct visibility on
Fields for the three-player arrangement. The host combat fixture uses that retail
map instead of changing placement, collision or gameplay rules.

## Verification and limits

**19 distinct native tests and the real-process ASan/UBSan combat test pass.**
The [inventory](evidence/m9_pvp_checks_2026-10-31.json) lists the cases, capture
hash, host counters and explicit qualification limits. Raw evidence includes the
[final dedicated encounter](evidence/m9_pvp_dedicated_final_2026-10-31.txt),
[graphical host encounter](evidence/m9_pvp_host_final_2026-10-31.txt) and
[sanitized dedicated encounter](evidence/m9_pvp_asan_dedicated_2026-10-31.txt).
Failed HUD-timing/navigation attempts remain preserved with their actual failures.
Seventeen existing input/control tests pass: keyboard/mouse gates and entry/combat, action
mapping, remapping, menus, gamepad, wheel/touch, rebinding and persisted controls,
plus the network command ordering/edge test. No movement, audio or renderer
algorithm was changed; the previous solo oracle results remain applicable.

All **6,739 protected SHA-256 values** remain unchanged. Both native and sanitizer
PvP executable symbol checks exclude ServerAdapter, server shell, GameSession,
Objects, CWeapon and inventory-manager symbols. Final builds have no new warnings;
the initial fixture's unused parameter warning was removed. The native build keeps
the graphical client/inspector; sanitizer qualification uses the server-only build.

Commands for the selected checks:

```sh
cmake --build build/m9-fx-runtime -j6 --target blood2_app blood2_server network_pvp_process_tests player_controls_tests input_action_tests
ctest --test-dir build/m9-fx-runtime -V --output-on-failure -R '^network.pvp_processes$'
ctest --test-dir build/m9-fx-runtime -V --output-on-failure -R '^gpu.network_pvp_host$|^player_controls\.(keys|mouse|gates|entry|combat)$|^input_actions\.|^network.player_commands$'
cmake --build build/m9-fx-runtime-asan -j4 --target blood2_server network_pvp_process_tests
ctest --test-dir build/m9-fx-runtime-asan -V --output-on-failure -R '^network.pvp_processes$'
```

The evidence retains the separate failed attempts and corrected host run. The
existing solo, PBR files, original GPU references and immutable captures remain
protected. M2-GPU-001 and M3 acceptance are independent. Windows/macOS interop,
human LAN, 22-map combat, 16-player combat and latency/loss/jitter qualification
remain unverified; the previous foundation/loading checks retain their narrower
scope. Remote sounds/shot effects and remaining presentation consumers are the
next implementation work after this combat connection.
