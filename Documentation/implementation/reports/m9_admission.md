# M9 — ENet Bloodbath admission

2026-10-30, following `f48e889f`. The console server now admits original
Bloodbath players through the new ENet protocol. The shared `blood2_server_runtime`
module owns admission and simulation for reuse by the eventual playable host.
There is still no interactive network game or graphical connect path.

The bounded handshake checks protocol, retail edition and the complete mounted
content SHA-256 before offering a map. The client loads its own DAT and sends
its profile with the offered world generation. Only then is an original player
created and initialized. Reconnection allocates fresh player/object identities.
Departure, timeout and shutdown remove the original player before freeing its
connection. Team/CTF admission remains explicitly refused.

Five little-endian message schemas are capped at 256 bytes on reliable Control.
Decoding is atomic; map/profile/identity fields are checked before mutation.
The endpoint handles at most 64 events per pump over the existing 16-peer and
transport queue limits. Monotonic wall deadlines are 5/30/15 seconds for
Hello/loading/joined liveness, with one-second heartbeats. Old-world packets,
duplicate Ready and incorrect phases/channels are refused. These clocks do not
advance simulation, movement or logical audio. No content is downloaded.

## Occupied start correction

The sixteen-peer test exposed the seventh player choosing an occupied Crypt
marker. The original performs one random marker draw, teleports there and ignores
the ground helper's return. The reconstruction's strict solver instead failed
while entering another player's body.

Initial multiplayer placement now reuses the bounded actor-separation service:
four horizontal face-adjacent candidates, nearest first, with a checked endpoint
and swept route. Only the initially overlapping player is omitted from the
escape sweep; the world and every other body still block. Existing players are
never moved. The marker draw remains exactly one, with no retry or additional
RNG consumption. If all four exits are blocked, entry fails cleanly rather than
crossing a collider. This is a documented replacement-physics policy, not verified
retail spawn behavior. Solo placement and runtime collision are unchanged.
Failure diagnostics now survive destruction of the failed player.

See the [source ledger](../sources/m9_admission_sources.json) and
[contract](../../contracts/multiplayer_contracts.md#enet-bloodbath-admission).

## Verification

[Linux validation](evidence/m9_admission_validation_2026-10-30.json):
**45/45 GCC headless cases** and **15/15 Clang ASan/UBSan cases** pass.
The codec covers every truncation of each schema, golden bytes, malformed bounds,
atomic rejection and 100,000 deterministic malformed/mutated inputs.

The real UDP private-data test admits **16 peers on Crypt for 180 simulation
ticks**, then exercises duplicate Ready, new admission and shutdown. All peers
in that load test share one test process. A separate test starts the actual
console executable and **two independent client processes**; each mounts and
hashes its own retail data, decodes the offered DAT, enters, exchanges heartbeats
and disconnects. Its client binary links no ServerAdapter, original server shell,
GameSession or object registry. This is process-isolated admission and static
world loading, not gameplay replication, combat or movement qualification.

All **22 multiplayer maps** still complete 180 zero-client ticks. Solo entry,
movement, saved-player restoration and existing NPC placement regressions pass.
The 360-frame solo movement trace retains SHA-256
`25c2932ddcc886dc77262d628c70993c33c186ce65850b26b01f945717f9f144`.
All **6,301 protected files**, including PBR and prior GPU/evidence references,
retain their hashes. The graphical client rebuild succeeds. The sanitized
dedicated also completes 60 ticks with unavailable audio/video drivers and no
discarded ticks. No new GPU run or emulator/simulator is included in this lot.

Initial failures are retained: the timeout fixture advanced its fake clock
before the server received ENet's final connection ACK; the corrected fixture
waits for both sides. The later occupied-marker failure led to the checked
placement adaptation above. One intermediate compile rejected calling the new
initial-placement service before declaring it in ServerDE; the declaration is
now explicit. The final GCC and sanitizer tests pass without suppressions.

## Remaining work

Gameplay snapshots/baselines, private event serialization, remote CMoveMgr and
combat, slow death/respawn, match rules, Team/CTF, host/connect UI and LAN discovery
remain in progress. The private event queues are still bounded local queues,
not drained by the transport. A fully enclosed occupied spawn is refused; no
general spawn search or retail collision equivalence is claimed.

No Windows/macOS interoperability, mobile/Quest build, human LAN game or degraded
network gameplay is certified by this lot. M2-GPU-001 and M3 remain independent.
