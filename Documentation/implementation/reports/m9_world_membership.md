# M9 — Continuous object membership over ENet

2026-10-30, following `f38eae26`. Existing clients now receive new objects,
metadata changes and removals, including another player joining and leaving.
Protocol **version 3** adds an exact world/sequence acknowledgement after the
client resolves its initial assets. Only then does the server publish continuous
membership; the loading-baseline deadline is 30 seconds.

Every three successful simulation ticks, each recipient compares the current
world with its last published membership. Reliable patches carry their exact
predecessor sequence, frame headers, removed identities and new/changed records.
Resource names/bindings, flags, hidden nodes and attachments trigger updates;
ordinary poses, scale/color and playback tracks await the State channel. New
objects still carry those initial values. Publication coalesces missed intervals.
See the [source ledger](../sources/m9_world_membership_sources.json).

The client remembers each slot's highest admitted generation until world reset.
An explicit live removal is required for replacement; retired generations cannot
return. Missing recipient players, wrong bases/worlds, unknown removals and failed
asset resolution leave the accepted frame and identity history intact. Both patch
and resulting world retain the 1 MiB bound and 32,768-byte ordered chunk limit.
Resource failure does not consume a generation. Replica callbacks remain absent.

## Verification

[Linux evidence](evidence/m9_world_membership_validation_2026-10-30.json) records
**66 distinct headless cases passed across the runs**, including the prior solo
replica, movement, environment, camera/HUD, audio and effect cases, plus all-map
zero-client simulation. The first broad invocation passed 65 cases and reported
one **Not Run**: `network.server_clock_audio` was not yet built in this isolated
tree. After compiling it, **7/7 final targeted cases pass**. Both logs remain.
Client and dedicated server compile; the initial missing-initializer warning is
fixed. The movement trace remains identical for all **360 frames**.

**5/5 Clang ASan/UBSan targeted cases pass.** Codec checks include every patch
truncation and 10,000 deterministic patch mutations alongside the existing
25,000 snapshot mutations. Receiver tests cover retired-generation resurrection,
slot replacement without removal, unknown removals, stale base/world, invalid
recipient references, resource failure followed by retry, and retained assets.

The 16-peer UDP fixture runs **186 ticks**, reconnects a peer and checks that
all replicas contain all live player identities and no retired player. Wrong
baseline world/sequence, duplicate ACK and four timeout phases remove the affected
player. This load fixture shares one process.

The actual dedicated executable and **two separate client processes** independently
hash and decode retail data. The first client observes the later player's creation
and departure through its own ClientWorld, under GCC and sanitizers. The probe and
asset receiver have no linked ServerAdapter, original server shell, GameSession or
object-registry symbols. The **6,337 protected file hashes**, including existing
PBR files and immutable references/evidence, are unchanged.

## Remaining work

This is continuous membership, not playable network movement/combat. Unreliable
20 Hz poses and interpolation are next; reliable teleports, collision/environment,
private player/HUD messages, logical audio and effect recipes still need delivery.
Remote weapon use, death/respawn, Team/CTF, playable host, graphical join and LAN UI
remain within the active M9 goal. No new GPU qualification, Windows/macOS
interoperability, mobile/Quest build, degraded-network gameplay or human LAN
qualification is claimed. No simulator/emulator was used. M2-GPU-001 and M3 remain
independent.
