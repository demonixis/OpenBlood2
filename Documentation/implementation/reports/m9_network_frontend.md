# M9 — graphical network connection and playable-host ownership

2026-10-30, following `fc4b840b`. The desktop application can now display a
remote Bloodbath world or host that same world with its own connected client.
This is the graphical admission/movement/HUD milestone; remote weapons, audio/FX,
combat, death/respawn and match rules are still required for playable multiplayer.
See the [contract](../../contracts/multiplayer_contracts.md) and
[provenance](../sources/m9_network_frontend_sources.json).

## Behavior

```sh
blood2-server --data-dir /external/retail --world multi/bb_crypt --mode bloodbath
blood2 --data-dir /external/retail --connect 192.168.1.20:27888 --name Caleb
blood2 --data-dir /external/retail --host multi/bb_crypt --port 27888 --name Host
```

The remote branch constructs only NetworkClient. The host branch owns exactly one
ServerSession/ServerNetwork and connects the same NetworkClient through loopback
ENet. Both follow the existing protocol/content/baseline gates. IPv4 DNS names are
accepted, with optional `:port`; the default is 27888. Port zero selects an ephemeral
host port for tests. Edition/overlay, solo-tool and XR combinations are rejected.
The host currently selects Bloodbath; Team/CTF configuration remains subsequent work.

The new desktop loop supplies ClientScene from locally resolved DAT/resources,
replicated objects and the predicted camera. The original HUD reads addressed
ClientHud values. Existing keyboard/mouse/gamepad controls and saved bindings,
mouse settings, display settings, Original/Enhanced shading and post-processing
are reused. Preferences are read without writing them. Esc or gamepad Start opens
a small session menu; Resume and Leave/End are navigable through ImGui. Original
Host/Join menu integration and LAN discovery remain pending. There is no third-person
toggle yet: the local displayed model still has its authoritative pose while the
camera predicts, so chase presentation needs a separate owner-pose adjustment.

Focus/menu state neutralizes local commands. The server clock, ENet pumps and
client prediction keep running. Server catchup remains capped at six 60 Hz ticks
with overload accounting. Leaving requests a graceful client disconnect; closing
the host shuts down its listener and removes all players. Captures use the existing
GPU readback; `--frames N --capture image.png` captures the final active frame.
Without a frame limit, a requested capture is taken once on the third active frame.

Render frames can contain zero or several fixed movement ticks. NetworkInput now
accumulates mouse-forward/strafe offsets until a tick, divides them across the due
ticks and consumes them once. Overload drops the same proportion of offsets as
simulation time. Menu/focus neutralization clears pending offsets, and leaving
mouse-move mode clears pending forward motion. Digital held controls retain the
existing CMoveMgr behavior; no second command simulation or integration is added.

The graphical path currently supplies an empty, world-qualified ClientEffects
snapshot. Remote sounds, particles, lights/fog, view weapons, chat and scoreboard
are not silently approximated. Their independent consumers are following M9 work.
The solo frontend, original movement rules and PBR files are unchanged.

## Verification

**18 native tests pass**, including five GPU tests (three network process scenarios,
solo menu traversal and player restore); **5/5 headless network tests pass under
ASan/UBSan**. Native client/server and sanitized server-only builds succeed.
The independent endpoint binary still contains no GameSession, ServerAdapter,
CBloodServerShell or Objects methods. All **6,600** protected PBR files, captures
and earlier evidence match their pre-edit hashes. No golden was regenerated.

The new movement regression feeds real independent CMoveMgr instances equivalent
mouse motion at 60 Hz, 240 Hz and three-tick render batches: final body/velocity and
120 substeps are identical. It also checks neutralization, mouse-mode changes,
invalid samples and proportional six-tick overload discard. CLI tests cover
host/connect isolation and bounded IPv4 hostname/port parsing; all eight existing
configuration cases pass. Existing endpoint/admission/movement process cases pass.

Three actual Linux GPU process topologies use retail `multi/bb_crypt`:

- Dedicated plus two graphical clients: 2,400 server ticks, 180/240 rendered client
  frames, two admissions/departures, zero rejection/timeout/state drops.
- Graphical host plus graphical remote: 900/180 rendered frames, 2,707 server ticks,
  two admissions/departures and zero discarded host ticks.
- Host departure while the remote remains connected: 180 host frames, 545 server
  ticks; the remote receives `Server stopped` and exits cleanly after 178 frames.

The first dedicated GPU test failed because its 900-tick lifetime ended before the
second window reached 240 frames (it reached 214). That failure is retained. The
fixture now grants 2,400 ticks; it still requires exact frame counts, successful
exits, capture output and admission/departure counters. The passing native run
records one discarded prediction tick for one dedicated client and six for the
remote in the shutdown case; these are explicit bounded catchup accounting, not
server tick loss. The initial host scenario already passed before that fixture fix.

The dedicated and host remote captures were visually inspected: world geometry,
textures, scene objects, crosshair and original health/armor/ammo HUD are present.
The captures remain build artifacts with recorded hashes/dimensions. Weapon models
and remote audio/effects remain absent as described above. No human LAN session,
Windows/macOS interop, mobile/XR execution, gamepad hardware run or combat under
network impairment is claimed. Esc/menu command gating is implemented; automated
GPU tests cover process lifetime and focus coexistence, not manual menu navigation.
Solo menu/restore tests pass; the preceding five detailed solo oracles were not
rerun because no solo runtime algorithm changed. M2-GPU-001 and M3 stay independent.

Commands (complete logs and the initial failure: `evidence/m9_network_frontend_*`):

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_frontend_process_tests network_address_tests network_client_movement_tests network_client_endpoint_tests network_admission_process_tests config_tests -j6
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.(address|client_movement|client_endpoint|admission_processes|movement_processes)$|^gpu\.network_(dedicated|host|shutdown)$|^config\.' -j3
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^gpu\.(menu_traversal|player_restore)$'
cmake --build build/m9-fx-runtime-asan --target network_address_tests network_client_movement_tests network_client_endpoint_tests network_admission_process_tests blood2_server -j4
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.(address|client_movement|client_endpoint|admission_processes|movement_processes)$' -j2
```
