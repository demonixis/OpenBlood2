# M9 — Owner-bound particle streams over the network

Protocol 22 sends original ParticleStream and ParticleSystem descriptors with
reliable object membership. Dedicated and playable-host sessions share this path;
independent clients create and update the existing particle runtimes. Solo keeps
its original synchronous service. This is one persistent-effect family, not the
completion of M9 or qualification of all multiplayer presentation.

## Implementation

`ParticleStreamSettings` and its validator move unchanged into a value-only
header. Snapshot object metadata gains an optional descriptor. Explicit
little-endian fields include canonical booleans, the original numeric limits,
32-bit count and four strings bounded to 1000 bytes each. The unused sound-radius/
three-sound tail is preserved without inventing playback. Baseline chunks and
membership patches retain their 1 MiB total bound; both decoded fragments and
merged worlds enforce 128 emitters. Runtime particle budgets remain 2048 per
system and 8192 across streams.

Creation is atomic with owner membership, so independent reliable channels cannot
expose a recipe before its owner. Metadata comparison sends later attachments,
visibility changes and removals through the existing patch path; unreliable poses
continue to interpolate normally. An attached recipe is immutable during that
owner generation. A patch cannot replace/remove it while preserving the owner;
ON/OFF changes the existing visible user flag. Retired generations cannot return.

`ServerSession` installs its capture hook before world initialization, records
checked descriptors, prunes retired owners and clears the registry before map
replacement. Texture decoding remains a required external-resource check. This
path creates no server cosmetic particle instances and consumes no game RNG.
Snapshot generation includes live descriptors for every recipient, including
late arrivals. Publication on a dead owner, duplicate publication, invalid values,
missing resources and registry exhaustion remain failures.

`NetworkEffects` starts an emitter once per object generation, follows only
ClientWorld poses/flags and advances the existing original algorithms in solo's
relative order. Failed or expired starts remain remembered while their owner
lives, avoiding retries on every snapshot. Owner removal retires particles and
reports before replacement-generation creation. Epoch changes clear local pools.
The client keeps its own resources, particles, clocks and RNG; no server object
callbacks or GPU structures enter this protocol.

A late arrival creates a fresh emitter in the **current** owner-visible state.
The authored initial ON remains in the wire recipe. Selecting current visibility
instead of replaying obsolete initial ON is an explicit network adaptation; it
avoids a spurious arrival flash. Past particles and ramp history are not replayed.
Preserve original stream ramps, alpha wrapping, color sampling and sound-tail
omission. ParticleSystem still floors rate × elapsed without fractional carry and
continues internal emission while hidden; hiding suppresses its presentation.

## Validation and limits

The [evidence manifest](evidence/m9_stream_checks_2026-10-31.json) lists exact cases,
commands, artifacts and hashes. Evidence distinguishes native headless synthetic
checks, tests using externally installed commercial data and real NVIDIA/Vulkan
readbacks. The [source ledger](../sources/m9_stream_network_sources.json) records
original references and changed integration files.

- Codec tests exercise every truncated prefix, 10,000 mutated snapshots, canonical
  fields, unused sound-tail round trips, count/string/numeric bounds, the 128-owner
  limit and reliable patches.
- Independent runtime tests exercise both original algorithms with real textures,
  pose changes, OFF/ramp/ON, late arrival while OFF, no duplicate starts, immutable
  metadata rejection, missing resources reported once, owner removal, generation
  reuse and world cleanup.
- Dedicated-session tests inspect all 22 retail multiplayer maps before and after
  a late admission, round-trip their snapshots and check dynamic publication,
  duplicate refusal and owner departure. Seven authored streams occur on Bridge
  (2), Condemned (3) and Rooftop (2). No authored ParticleSystem occurs in these 22
  maps; its service and renderer are exercised with synthetic descriptors.
- Bridge → Crypt → Bridge rotation checks fresh epochs and 2 → 0 → 2 baseline
  emitters while server particle pools remain empty.
- Two genuine independent client processes join Bridge sequentially, observe
  replicated player arrival/departure and run both authored streams without
  duplicate starts or cosmetic failures. This is loopback, not human LAN coverage.
- The independent scene test renders decoded stream/system descriptors through
  existing Original and Enhanced paths, then verifies identical Original pixels
  on return. Its particle colors/positions are synthetic; the texture is external
  retail artwork. No renderer/shader/PBR code is changed.

Final passing coverage: **139 distinct native cases, 48 ASan/UBSan cases and
138 inspector-free Release cases**. The selected regressions include replica
membership/state, private events, audio, combat services, effects and save sessions.
Eight original particle-stream cases pass natively and in Release; that older test
target is not registered in the server-only sanitizer build. New codec/runtime,
22-map server audit, rotation and two-process stream checks are sanitized.

The native/Release GPU uses NVIDIA GeForce RTX 3060 Laptop GPU/Vulkan at 640×360;
stream/system particles change 3,455 pixels and Original → Enhanced → Original
returns identical Original bytes. Both captures were visually reviewed. The native
process probe records particle peaks 148/90 for the first/late clients. Five fresh
solo traces (effects, audio, movement, environment and view) match the preceding
lot byte for byte. All **6,938 protected hashes** remain unchanged, including the
39 untracked PBR files and historical capture/reference artifacts. Independent
client test executables have no ServerAdapter, GameSession or original server
player/weapon/inventory symbols.

Earlier failures are retained in the evidence: the initial System fixture used a
rate below one particle per 60 Hz update, expected hidden internal particles to
vanish, and assumed a System existed on the multiplayer maps. Corrected tests
preserve those original behaviors and actual map contents. An initial scene
assertion incorrectly changed the visible-player count in the stream case; the
ordinary model count remains unchanged. Sanitized endpoint rotation also exposed
a test ordering issue: preparing the second client's full content hash after
connecting the first could consume its admission deadline under build load. Both
preparations now finish before either connection; production deadlines are unchanged.
The first sanitizer test list preceded CMake regeneration and omitted new test
executables; fresh targets and explicit cases are required for final results. A
stale sanitized scene executable also retained the corrected visible-player assertion
until its target was explicitly rebuilt.

Object-particle recipes/projectile trails, PolyGrids, WarpGates, evaluated lights,
fog and other presentation/UI work remain separate M9 lots. No Windows/macOS,
mobile/Quest, impaired-combat matrix, 16-player load or human LAN qualification is
claimed here. One CMoveMgr integration, logical server audio/RNG, solo behavior,
immutable PBR references and independent M2-GPU-001/M3 gates remain required.
