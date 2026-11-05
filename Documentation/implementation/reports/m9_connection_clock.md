# M9 — Connection deadline after content preparation

2026-10-31, following `1026355e`. NetworkClient now mounts and fingerprints its
external content before starting the network connection clock. This corrects the
startup deadline exposed by the [roster lot](m9_roster.md): its first sanitized
combat run timed out while other builds/tests ran, whereas the isolated rerun
passed. That initial failed run remains immutable evidence.

Previously connect accepted a time sampled before synchronous resource work and
set its deadline to that old time plus five seconds. Slow mounting/hashing could
therefore exhaust the connection budget before the first update. The new prepare
step owns resource work and enters Prepared without opening transport. Every
frontend/process caller then samples monotonic time and calls connect. The test
using a controlled clock also starts that clock after preparation.

Prepared endpoints refuse update and disconnect without changing state. Repeated
prepare, connect before prepare and invalid profiles are rejected; an invalid
profile does not consume prepared content. A fresh endpoint is still required
per admission. The five-second connection budget, later phase deadlines,
fingerprint algorithm, protocol 15, server clocks and CMoveMgr are unchanged.
See the [contract](../../contracts/multiplayer_contracts.md#connection-deadline-after-content-preparation).

## Verification

**Seven native tests, six ASan/UBSan tests and one inspector-free Release repeat
pass.** Project builds have no compiler warnings. Raw commands and results are retained under
`evidence/m9_connection_clock_*_2026-10-31.*`.

- A deterministic endpoint test advances its external clock by one hour while
  prepared. A subsequent connection without a responding server remains alive
  at 4.999 seconds and fails at exactly five seconds. No sleep, longer timeout
  or expensive synthetic busy work masks the boundary. Real ENet hello, backwards
  clock, offer timeout, malformed packet/channel/order and rejection reason
  checks still run with independently mounted retail data.
- Independent client impulse and separate-process admission, movement, firing
  and lethal PvP checks exercise every adapted caller. Native and sanitizer PvP
  are deliberately run concurrently with other verification work, without
  relaxing any deadline. The graphical host repeats the real remote combat path.
  Dedicated native/sanitized runs complete 2,400 ticks without discards, 13 shots,
  one frag, 13 impulses and health-100 respawn. The Fields host completes 1,200
  frames, 3,608 server ticks with nine discarded server ticks, zero local discards,
  three admissions/departures, 33 shots, one frag and health-100 respawn. These are
  lifecycle checks, not a load-capacity or physical-output qualification.
- Inspector-free Release builds and repeats the endpoint checks. Formatting,
  diff/link checks and independent client executable symbol inspection pass.
  All 6,832 protected hashes, PBR files and historical evidence remain unchanged.
  There is no solo simulation/audio/render code change or commercial asset import.

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json ctest --test-dir build/m9-fx-runtime -V -R '^(network\.(client_endpoint|impulse_client|admission_processes|movement_processes|fire_processes|pvp_processes)|gpu\.network_pvp_host)$'
ctest --test-dir build/m9-fx-runtime-asan -V -R '^network\.(client_endpoint|impulse_client|admission_processes|movement_processes|fire_processes|pvp_processes)$'
ctest --test-dir build/release -V -R '^network.client_endpoint$'
```

This fixes preparation consuming the connection timer; it does not certify
arbitrarily stalled networks or phase loading on every platform. No Windows/macOS,
mobile/Quest, physical-speaker or human LAN qualification is added. M9 remains in
progress with announcement/UI, remote effects, match rules and Team/CTF work;
M2-GPU-001 and M3 acceptance stay separate.
