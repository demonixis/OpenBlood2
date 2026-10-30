# Dedicated server — M9 in progress

M9 is in progress. This executable loads and advances a retail multiplayer world
without a window, GPU or audio device. Bloodbath accepts the new bounded ENet
admission protocol; Team/CTF still refuse player admission. Host/join screens,
network gameplay, match limits and rotation are not available yet. See the
[admission report](implementation/reports/m9_admission.md),
[foundation evidence](implementation/reports/m9_multiplayer.md) and
[accepted contract](contracts/multiplayer_contracts.md).

```sh
cmake --preset server
cmake --build --preset server --parallel 4
ctest --preset server --output-on-failure
build/server/blood2-server --data-dir /path/to/Blood2 --world multi/bb_bloodarena --mode bloodbath
```

The `server` preset sets `BLOOD2_BUILD_CLIENT=OFF`, `BLOOD2_BUILD_SERVER=ON`.
Both targets can also be enabled together in another build directory. Server-only
configuration does not fetch ImGui, miniz or the HTTPS download dependencies.
It retains CPU DAT/collision/model readers and SDL's sound decoding/conversion
services. SDL has no graphical or real audio-device backend in the Linux server
configuration; no audio subsystem/device is initialized.

Data stay external. `--data-dir` or `BLOOD2_DATA_DIR` selects the retail installation;
the full mounted archive contents are hashed at startup. No data download or
distribution is provided. Demo, Nightmares and mod overlays are refused. The
complete-content hash is separate from the existing save fingerprint.

Modes are `bloodbath`, `team` and `ctf`. Bloodbath/Team require a `multi/bb_*`
world; CTF requires `multi/ctf_*`. These select source object filtering, not a
completed playable match. Defaults are bind `0.0.0.0`, UDP port `27888`, maximum
16 transport peers. `--bind 127.0.0.1 --port 0` confines tests to loopback with an
ephemeral port. `--ticks 180` stops after three simulated seconds. Ctrl+C or
SIGTERM exits cleanly. The shutdown line reports ticks, discarded
overload ticks, admissions, departures, refusals and timeouts. Simulation runs
at 60 Hz with at most six catch-up ticks.

For private-data checks, configure with
`-DBLOOD2_TEST_DATA_DIR=/path/to/Blood2`. The tests exercise zero-client simulation and
Bloodbath admission, not interactive multiplayer gameplay. CI uses `ci-server` and synthetic data; desktop archives
are staged by `cmake/PackageServerCi.cmake`. Windows/macOS compilation, actual
interoperability and human LAN play require their own recorded results.

Admission version 1 exchanges little-endian messages on the reliable Control
channel, limited to 256 bytes. A client sends its edition and complete content
hash, receives the approved map and world generation, loads its own mounted
map and returns a validated profile with that generation. Only then does the
server create its original player. No content is sent by the server.

Deadlines are 5 seconds for Hello, 30 seconds for loading and 15 seconds for
joined-client liveness. Joined clients exchange a heartbeat every second; the
server uses a monotonic wall clock independently of its simulation clock.
Reconnect is a new admission with fresh player/object identities. Shutdown
detaches players before destroying the transport. This protocol is not compatible
with retail executables, and the regular graphical client is not connected yet.
