# M9 implementation record

Started 2026-10-30 against `446933cc`, following the authorized
[multiplayer contract](../../contracts/multiplayer_contracts.md).
M9 is in progress and unaccepted. M2-GPU-001 and M3 remain independent.

## Audit: current dependencies

| Subsystem | Current dependency | Required change |
| --- | --- | --- |
| Session | GameSession owns one ServerAdapter, server shell, client shell and local_client | Separate simulation and replica ownership; retain one bound original server per process |
| Admission | Internal M9 registry accepts 16 checked connections; Bloodbath start selection is ported; init still routes to OnSinglePlayerInit | Checked N-player admission and original multiplayer init |
| Renderer | Object/world/sky presentation and all scene FX/light/fog consumers use client replicas; legacy FX producers/updates remain in the local adapter | Extract legacy FX runtime services and the remaining client shell |
| Movement | Local CMoveMgr body and owned collision replica, immediate authoritative correction; environment now queries copied geometry/values, stance remains a local service | Remaining services; delayed reports and checked server advancement between uploads |
| Audio | Server AudioTimeline owns logical voices; ClientSession owns independent playback/music/device and copied descriptors | Recipient delivery/network synchronization; remaining legacy client request entry points |
| Effects | GameSession::step advances server and client pools together | Separate source events/persistent descriptors from local cosmetic pools |
| Cameras/HUD | Client-owned descriptors, ray queries, view state, HUD values/clock; checked local permissions/Eye commands | Network delivery; remaining inventory/FX runtime and client-shell extraction |
| Activation | Player updates call PingObjects with a visibility observer | All live players contribute independently of presentation |
| Frontend | waiting_for_player and menu/focus gates suppress session advancement | Keep zero-client dedicated and multiplayer host running |
| Gameplay | Multiplayer death/init/respawn/frags and pickup respawn are omitted | Port source branches; reuse existing local CTF objects without claiming network coverage |
| Saves | Save fingerprint hashes directory metadata; multiplayer saves refused | Separate full-content network fingerprint; retain save refusal |
| Build | engine and blood2_game mix CPU, rendering, audio, menu and FX | Dedicated-only target/dependency graph and separate presentation |

## Delivery ledger

| Lot | Status | Evidence |
| --- | --- | --- |
| Contract and dependency audit | Written | Static inspection; no network gameplay qualification |
| ENet and bounded transport | Foundation implemented | Real loopback UDP, three channels, 16 peers, reconnect identities and queue bounds; no gameplay wire schema yet |
| Dedicated separation and logical services | Build/zero-client foundation implemented | Separate server preset, logical audio test, 22 mounted maps × 180 ticks; shared CPU/client storage remains transitional |
| Replica and solo loopback | Object presentation, movement body, collision/environment, cameras, HUD, audio and visual-effect presentation separated locally; remaining runtime services/network pending | [Local replica](m9_client_world.md), [movement](m9_local_movement.md), [collision](m9_client_collision.md), [environment](m9_client_environment.md), [cameras/HUD](m9_client_view.md), [audio](m9_client_audio.md), [effects](m9_client_effects.md) |
| Network sessions and Bloodbath | Pending | — |
| Team and CTF | Pending | — |
| UI, LAN discovery and qualification | Pending | — |

Existing untracked PBR documents, sources, shaders and tests are pre-existing
workspace work and are not part of this change. Historical captures and evidence
remain immutable.

## Implemented foundation

`BLOOD2_BUILD_CLIENT` / `BLOOD2_BUILD_SERVER` separate the build graph:
`engine_core` holds CPU services, `engine` holds client GPU/runtime code,
`blood2_frontend` holds menus and `blood2_server` links simulation plus `blood2_net`.
The server preset excludes ImGui, XR, setup/miniz/HTTPS and real audio/video
backends. Linux `ldd blood2-server` lists only libc, libm, libstdc++, libgcc and the
loader; no graphics, display or audio-device shared library. The binary does not
initialize a window or audio device.

`ServerSession` currently wraps shared GameSession storage. `step_server` preserves
server audio completion, original updates, physics, projectiles, models, container
damage, deferred lifetime, attachments and evaluated lights, while omitting local
movement, view weapons and client FX advancement. It ticks with no local player.
The 60 Hz clock caps catch-up at six ticks, counts discarded work and refuses
invalid deltas without mutating its remainder. Normal `GameSession::step` keeps
its prior order and time delta. Further extraction of client shell/CPU effects,
logical audio records and renderer data is still required before network play.

ENet has one owner per host, generation-like monotonic peer IDs, a 2 MiB per-peer
outgoing budget and matching inbound waiting-data limit. Control and initial-state
channels are reliable; state uses ENet's unreliable sequencing (including its
source reliable promotion on sequence wrap). Limits are 16 KiB / 32 KiB / 1200
bytes respectively. Bounds and flags are checked before payload delivery. Address
resolution follows ENet initialization, including Winsock on Windows. The complete
content hash reads all mounted bytes in bounded blocks, includes ordered archive
names/sizes/digests and ignores absolute installation paths. It refuses edition/mod
mismatches and observed file changes; it does not replace the save fingerprint.
Mounted data must remain immutable for the session.
ENet's upstream Unix timeout clock still uses `gettimeofday`; the simulation uses
`steady_clock`. A monotonic session-timeout policy and clock-jump tests remain to
be added with admission, rather than claiming that ENet already provides them.

The bounded codec currently covers scalar/string primitives with explicit little
endian encoding. Truncation, oversized strings, NUL, nonfinite floats and seeded
random byte input are tested. It is **not** yet the game protocol or full protocol
fuzzing. The dedicated process binds UDP but explicitly refuses every admission;
help, startup log and archive notices say so. There is no `--host`/`--connect`
frontend in this foundation increment. The subsequent local replica lot is
recorded separately below.

Real multiplayer map loading exposed the old ItemPickup precreate restriction.
The original quantity/respawn branches were ported as a prerequisite; see the
[pickup scope](../../contracts/multiplayer_contracts.md#pickup-loading-prerequisite).
The initial failed Blood Arena/Cabana runs remain failures of that earlier build;
the successful reruns follow the source port, not a relaxed startup check.

## Verification and remaining gates

Linux Release server: **14/14 CTest tests pass**, including 11 synthetic/help tests
and three private-data tests. The full mounted-world test finds exactly 22 retail
multiplayer DATs and runs 180 server ticks on each without clients. Separate
Blood Arena/Cabana process tests run the real-time loop with empty DISPLAY and
WAYLAND_DISPLAY and an invalid audio driver. This is opening/short simulation
coverage, **not** the required two-player/16-player qualification.
The per-map log was kept on the original development branch (`develop-multiplayer`), not in this repository.

The first GCC sanitizer link attempt failed because this machine lacks
`libasan.so.8.0.0`; Clang ASan/UBSan passed the eight transport/core tests. The full
Clang build also exposed a pre-existing recursive JSON completeness issue with
libstdc++ 16. Defining Json's constructors/destructor after the complete type fixes
that compiler compatibility issue without changing material parsing or rendering.

The completed Clang build passes **11/11 synthetic/help tests under ASan/UBSan**,
with ENet itself instrumented too. Separate sanitizer processes load Blood Arena
and CTF Cabana and finish 180 ticks, exit 0, with no sanitizer finding. The Debug
sanitizer runs discard 530/806 overload ticks under concurrent compilation; these
are not real-time performance qualification. The Release process tests report
zero discarded ticks on both maps. The inspector-free Release client builds.

The development client and requested test binaries build, and **87/87 targeted
regressions pass**: objects, worlds, player, movement, cinematics/audio, menu state
and preferences, session saves, pickup saves/collection and surface-material
parsing. A stale empty save-file member in the development build
archive initially broke linking; rebuilding that archive and its dependents fixed
the build. Tests run against incomplete/stale outputs were not accepted as final
evidence. The final run includes the previously unavailable six save-pickup cases
and the embedded-pickup case, all passing.
Six additional menu checks pass (credits text plus five private-data artwork,
session, cheats, credits and profile checks), for **93 targeted client regressions**.

Commands used (data path denotes the external local installation):

```sh
cmake --preset server -DBLOOD2_TEST_DATA_DIR=/path/to/Blood2
cmake --build --preset server --parallel 3
ctest --preset server --output-on-failure

cmake --preset server -B build/server-clang-sanitizers \
  -DBLOOD2_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBLOOD2_TEST_DATA_DIR=
cmake --build build/server-clang-sanitizers --parallel 3
ctest --test-dir build/server-clang-sanitizers --output-on-failure
# Also run this binary on bb_bloodarena and ctf_cabana with --ticks 180,
# --bind 127.0.0.1 --port 0, isolated BLOOD2_USER_DIR and no display/audio driver.

cmake --preset release
cmake --build --preset release --target blood2_app --parallel 3
cmake --preset dev
cmake --build --preset dev --target blood2_app player_tests movement_tests \
  cinematic_tests menu_tests save_session_tests object_contract_tests world_tests \
  pickup_tests save_pickups_tests surface_tests --parallel 3
ctest --preset dev --output-on-failure \
  -R '^(objects|world|player|movement|cinematics|menus|save_session|pickups|save_pickups|surface)\.' \
  -E 'gpu\.|private|catalog|retail|menus\.(session|cheats|credits|profiles|artwork)|cinematics.subway'
ctest --preset dev --output-on-failure \
  -R '^menus\.(credits_text|artwork|session|cheats|credits|profiles)$'
```

JSON/YAML parse checks, local documentation links, formatting and `git diff --check`
pass. Linux server packaging was smoke-tested from the staged directory using
`cmake -DPLATFORM=linux-x64 -DSERVER_BUILD=/absolute/build/server -P cmake/PackageServerCi.cmake`.
The test output with the final pass counts and sanitizer logs was kept on the
original development branch (`develop-multiplayer`), not in this repository.

The desktop CI gains a separate server matrix and archives with ENet/SDL/spdlog
notices. This workflow has not been run on Windows/macOS in this session. No remote
push, hardware interoperability, human LAN play, mobile/Quest rebuild or GPU capture
is claimed. GPU baselines, original sources, commercial data and existing PBR work
are unchanged. Admission, ClientWorld, original multi-player life/combat/teams,
snapshot interpolation, UI/discovery, rotation and impaired-network tests remain.

## Server player connection prerequisite

The [connection lot](m9_player_connections.md) now accepts 16 checked internal
connections in M9 modes, enforces exclusive player ownership and restores the
original random Bloodbath marker selection. Solo retains its connection limit
and selection. No packet creates a player yet; multiplayer profiles, match rules
and wire admission remain pending.
