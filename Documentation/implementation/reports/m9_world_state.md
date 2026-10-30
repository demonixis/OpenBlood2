# M9 — Absolute poses and remote interpolation

2026-10-30, following `e7604167`. Protocol **version 4** publishes absolute object
poses on ENet's unreliable sequenced State channel every three successful server
ticks. Reliable membership still controls creation, removal, resources and flags.
Each State packet contains at most **10 records / 1,120 bytes**, independently
applicable without assembling a whole unreliable frame. Repeated absolute states
recover from loss. A publication that cannot fit the bounded peer queue is skipped
and counted; missed simulation intervals do not produce a catch-up burst.

The receiver gates packets by world, current reliable membership, object generation
and pose revision. Per-object sequences reject duplicates and older states while
allowing disjoint packets of one publication. Invalid model/sprite indices fail
atomically. ClientWorld's publication counter is separate from server sequences.
The [source ledger](../sources/m9_world_state_sources.json) records the boundaries.

A passive sampler keeps **eight samples per object** and interpolates positions,
normalized shortest-sign rotations, scale/color/alpha and compatible advancing
animation clocks. It clamps outside its available history without extrapolation.
Clip changes and loop boundaries use stored playback; no animation events or game
callbacks run. The local predicted player and its attached children are excluded.
The caller supplies delayed server time; the process probe uses **100 ms**.
Production client clock synchronization remains to connect.

The existing TeleportObject service now records a transient server-wide revision
when position changes. Read-only capture passes it to the object and its attached
child. The reliable metadata patch carries the destination and resets interpolation.
Same-position calls do not change revision. No new teleport/collision path, saved
gameplay field or RNG draw is introduced; private forced-movement commands still
need their remote delivery. CMoveMgr integration remains unchanged.

## Verification

[Linux evidence](evidence/m9_world_state_validation_2026-10-30.json): **9/9 targeted
GCC tests**, **17/17 regression cases**, and **9/9 ASan/UBSan cases** pass. The GCC
sets cover **24 distinct headless cases plus native Vulkan gpu.player_restore**.
Client and dedicated builds pass. The all-map baseline case checks all 22 mounted
multiplayer worlds with the new protocol. Standalone state/probe binaries contain
no ServerAdapter, original server-shell, GameSession or object-registry symbols.

Codec checks cover every truncation and **20,000 seeded mutations**. Behavioral
checks cover dropped/duplicated/reordered states, future membership, stale identity
and pose revision, bounded history, interpolation without extrapolation, local
prediction exclusion and reliable teleport snapping. A local parent/attachment
test checks revision inheritance and passive capture. **18 synthetic pose schedules**
combine RTT 0/50/150 ms, loss 0/1/5% and jitter 0/20 ms. These are encoded-message
receiver tests, not impaired ENet gameplay or collision/combat qualification.

Sixteen real UDP peers in one fixture run **186 ticks**: exactly **992** addressed
pose publications, **23,581,376 payload bytes**, zero queue drops. This uncompressed
all-object stream is approximately **475 kB/s per peer** in that fixture; it is a
measurement, not a qualified 16-player gameplay capacity. Existing clients still
observe reconnect replacement and retired-player removal.

The actual console server and **two independent client processes** receive and
sample live poses, observe arrival/departure and shut down cleanly under GCC and
sanitizers. Each run publishes 140 pose frames across the two clients during a
900-tick server run, with zero queue drops, rejected peers, timeouts or discarded
ticks. The clients remain headless probes, not interactive game clients.

All five solo traces remain byte-identical: 240 FX, 360 audio, 360 movement,
720 environment and 300 camera/HUD frames. All **6,348 protected hashes**, including
PBR files and immutable GPU/evidence references, remain unchanged.

## Remaining work

Collision/environment values, addressed movement/player/HUD messages, logical
voices and effect recipes need wire delivery before remote movement and combat.
Death/respawn, Team/CTF, playable host, graphical join, LAN UI and production clock
synchronization remain in the active M9 goal. No Windows/macOS interoperability,
mobile/Quest build, human LAN or degraded-network gameplay qualification is
claimed. No simulator/emulator was used. M2-GPU-001 and M3 remain independent.
