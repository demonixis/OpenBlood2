# Dedicated server — M9 in progress

`blood2-server` hosts retail Bloodbath and Team without a window, GPU or audio
device. It shares the same simulation as the playable host. Movement, validated
weapon fire, death/respawn, scores, original logical audio and map rotation are
connected; see [Team network verification](implementation/reports/m9_team_network.md),
[Bloodbath combat](implementation/reports/m9_pvp.md),
[rotation](implementation/reports/m9_rotation.md) and the
[accepted contract](contracts/multiplayer_contracts.md). CTF admission, remote
music/FX, chat/ping and Host/Join/LAN screens remain in progress.

```sh
cmake --preset server
cmake --build --preset server --parallel 4
ctest --preset server --output-on-failure
build/server/blood2-server --data-dir /path/to/Blood2 --world multi/bb_crypt --mode team --end either --frag-limit 25 --time-limit 10 --map multi/bb_fields --friendly-fire off
```

Clients connect with `blood2 --data-dir /path/to/Blood2 --connect HOST:27888
--name Player --team auto`. Team preferences are `auto`, `blue` or `red`; the
server assigns auto to the least populated team and preserves assignment across
map rotations. Reconnecting is a fresh admission. Bloodbath ignores team choices.

The `server` preset sets `BLOOD2_BUILD_CLIENT=OFF`, `BLOOD2_BUILD_SERVER=ON`.
Both targets can be enabled together. Server-only configuration excludes ImGui,
rendering/XR, setup and download dependencies. It retains CPU DAT/collision/model
readers and SDL sound decoding/conversion, without initializing an audio device.
Source logical durations, completion, stops and server RNG remain active.

Data stay external. `--data-dir` or `BLOOD2_DATA_DIR` selects the retail installation;
the complete mounted archive payloads are SHA-256 checked before admission. All
clients need identical content and protocol **17**. No download, commercial data
distribution, retail wire compatibility, demo, Nightmares or mod admission is
provided. This complete-content hash is separate from the save fingerprint.

Modes are `bloodbath`, `team` and zero-client `ctf` inspection. Bloodbath/Team
require `multi/bb_*`; CTF requires `multi/ctf_*` and refuses players until its
independent capture consumers are connected. Defaults: bind `0.0.0.0`, UDP `27888`,
16 peers including any playable host. `--bind 127.0.0.1 --port 0` selects an
ephemeral loopback listener for tests. Internet by direct address needs a reachable
UDP port; there is no NAT traversal or relay.

`--end frags|time|either|never` defaults to 25 frags. `--frag-limit` uses player
scores in Bloodbath and accumulated team scores in Team, including contributions
from departed members. `--time-limit` uses minutes (dedicated default 10, playable
host default 15). Zero is immediate; an empty Team server has two zero-score teams.
`--map` appends to a cyclic rotation of up to 50 maps including `--world`.
Original intermission lasts strictly more than 7.5 simulation seconds. Peers and
profiles/teams remain connected, but each map resets scores, inventory and world/
player identities. `--friendly-fire on|off` and `--negative-team-frags on|off` both
default on, preserving the original host/dedicated Team defaults.

CTF server prerequisites now include `--flag-value N` (default 5, bounded to
0..1000000), `--only-flag-scores on|off` and `--use-team-size on|off` (both off by
default). OnlyFlagScores suppresses enemy-kill points while retaining suicide and
enabled team-kill penalties. UseTeamSize multiplies capture points by at least
one opposing member, including inactive/dead members. These rules reset with the
configured rotation; see [CTF server verification](implementation/reports/m9_ctf_rules.md).
CTF wire admission and independent capture presentation remain pending.

Simulation runs at 60 Hz with at most six catch-up ticks; state publishes at
20 Hz. `--ticks 180` stops after three simulated seconds. Ctrl+C/SIGTERM performs
checked cleanup. The shutdown line reports total ticks, discarded overload,
admissions/departures/refusals/timeouts and rotations. Empty servers keep ticking.

Control/private events and initial membership use separate reliable ENet channels;
poses use sequenced unreliable updates. Clients rebuild world/collision/presentation
from their own mounted resources; no process pointer is serialized. Admission
messages are bounded to 256 bytes, private messages to 2,048 bytes and initial
object snapshots to 1 MiB in bounded chunks. Deadlines: Hello 5 seconds, loading
30 seconds, joined liveness 15 seconds, heartbeat every second. All use monotonic
wall time independently of simulation clocks.

For private-data tests configure `-DBLOOD2_TEST_DATA_DIR=/path/to/Blood2`.
Synthetic CI, private-data tests and actual graphical/process verification are
reported separately. Desktop server archives use `cmake/PackageServerCi.cmake`.
Current Linux scripted results do not qualify human LAN, Windows/macOS
interoperability, 22-map combat, 16-player load or degraded-network combat.
