# Dedicated server foundation

M9 is in progress. This executable loads and advances a retail multiplayer world
with no player, window, GPU or audio device. **It refuses player admission.**
Host/join screens, client replicas, network gameplay, match limits and rotation
are not available yet. See [M9 evidence](implementation/reports/m9_multiplayer.md)
and the [accepted contract](contracts/multiplayer_contracts.md).

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
SIGTERM exits cleanly. The shutdown line reports completed ticks and discarded
overload ticks; simulation runs at 60 Hz with at most six catch-up ticks.

For private-data checks, configure with
`-DBLOOD2_TEST_DATA_DIR=/path/to/Blood2`. The tests exercise zero-client simulation,
not multiplayer gameplay. CI uses `ci-server` and synthetic data; desktop archives
are staged by `cmake/PackageServerCi.cmake`. Windows/macOS compilation, actual
interoperability and human LAN play require their own recorded results.
