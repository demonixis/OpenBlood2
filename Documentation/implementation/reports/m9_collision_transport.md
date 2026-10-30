# M9 — Collision and environment over the network

2026-10-30, following `3a17ff8e`. Protocol **version 5** now delivers collider and
liquid/ladder/conveyor values with the existing world replication. Remote clients
reconstruct primary collision and bound shapes from their own admitted DAT before
acknowledging the baseline. No geometry, server handle, pointer or gameplay object
is transferred. See the [source ledger](../sources/m9_collision_transport_sources.json).

Reliable metadata includes shape binding/basis, VolumeBrush classification,
optional code/current/gravity/viscosity/Hidden, dynamic/pushable flags, priority and
the Seeing Eye owner exclusion. Full unsigned 16-bit container codes remain valid,
including built-in zero and unknown inert values. State packets carry dimensions,
velocity, acceleration and the actual posed shape origin alongside object poses:
**seven records, at most 1,132 bytes** per independent datagram.

A shared read-only extractor retains the previous local collision publication's
values. Its deep-copy caches and ordering remain unchanged; solo object presentation
does not opt into extra wire capture. Client collision still performs no original
callback, authoritative push/contact, damage, air update or second integration.
CMoveMgr source and server logical audio are unchanged.

The independent receiver owns decoded DAT model data, primary collision and cached
base/posed shapes. Containers use closed-shell validation; world models use the
existing triangulated policy. Rotations reproduce the original port's authored
basis-relative transform at the actual shape origin. Translation retains the
shape and uses position minus origin, avoiding double translation. Object and
collision frames commit together after complete validation; failed geometry or
bindings cannot advance accepted object histories. Collision uses raw received
poses, separately from interpolated presentation. Retained frames own their shapes
after source-world teardown.

## Verification

[Linux evidence](evidence/m9_collision_transport_validation_2026-10-30.json):
**36/36 GCC cases** (35 headless and native Vulkan **gpu.player_restore**) and
**21/21 ASan/UBSan cases** pass. Following the final collision-classification
validation, **6/6 targeted cases pass again in each build**. Client and dedicated
builds pass. Standalone collision/probe binaries have no server adapter, original
shell, GameSession or registry symbols.

All **22 multiplayer maps** round-trip the complete object/physics baseline.
Client model/sprite/shape pointers differ from the server's, all reconstructed
shape triangle coordinates match exactly, body/head container probes match and
**704 segment queries** agree with the authoritative scene. The largest baseline
is **65,860 bytes**, CTF Jam. These are opening/geometry tests; CTF network admission
and two-player traversal of every map remain pending.

The synthetic fixture covers Water/Ladder/Conveyor, hidden first-head masking,
four-entry overlap order, independent stance values, primary support, translation,
rotation, cache reuse and independent lifetime. Seeing Eye exclusions apply to
movement sweeps while rays retain their existing behavior. Invalid binding, shape
origin or classification cannot partially replace object/collision state; retry
with the same State sequence still succeeds. Codec coverage includes every
truncation and **10,000 seeded physics-record mutations**.

Sixteen UDP peers in one process run **186 ticks** with exactly **992** state
publications and zero queue drops. The uncompressed payload is **34,031,552 bytes**,
about **686 kB/s per peer** in this fixture. This is measured protocol traffic, not
qualified sixteen-player gameplay capacity. The actual dedicated executable and
two independent client processes own their collision before ACK, receive live
states, observe arrival/departure and shut down with no rejected peers, timeouts or
discarded server ticks under both builds.

All five solo traces remain identical: 240 FX, 360 audio, 360 movement,
720 environment and 300 camera/HUD frames. All **6,373 protected hashes**, including
PBR files and immutable GPU/evidence references, remain unchanged.

The initial standalone compile exposed an unqualified legacy-header include; it
now uses its existing explicit source path. The initial synthetic support fixture
had not marked its primary surfaces solid; it now sets SURF_SOLID, asserts twelve
primary triangles and uses the solver's existing .001 contact separation. Failed
logs remain; no collision solver was relaxed. The redundant uint16 range warning
is fixed; the pre-existing unused-variable warning in replica_receiver_tests
remains outside production code.

## Remaining work

Private player/HUD/force-position events and the network CMoveMgr command bridge
are next. Remote firing/damage, death/respawn, Team/CTF rules, playable host,
graphical join and LAN UI remain in the active M9 goal. This lot does not qualify
collision or combat under degraded ENet conditions, human LAN play, Windows/macOS
interoperability or mobile/Quest builds. M2-GPU-001 and M3 remain independent.
