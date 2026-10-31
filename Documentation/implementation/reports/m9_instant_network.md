# M9 — Network explosions, fragments and instant beams

Independent clients now execute the existing six explosion components, flesh and
destruction fragments, laser beams and lightning bolts. The network server routes
their publication values through protocol **21**, without creating cosmetic pool
instances on these paths. Original damage, logical voices and gameplay RNG stay
on the server. Solo keeps its existing synchronous services and clocks.

This follows [impact sprites/sparks/blood](m9_impact_network.md). Persistent
owner-bound FX, including projectile trails, still require baselines and reliable
lifetime/parameter updates. The whole multiplayer plan remains open.

## Source behavior and transport

Original ClientExplosionSFX sends position, normal and type as an instant message;
ClientGibFX sends position, velocity, dimensions, flags, scale/count and optional
custom paths. Lightning sends its endpoints and three selection bytes. This lot
reuses the reviewed ports rather than importing another implementation. The
[source ledger](../sources/m9_instant_network_sources.json) records original and
reconstruction baselines.

Five alternatives extend B2EF: an explosion component recipe, flesh, destruction
debris, laser and lightning. The explosion discriminator retains the port's actual
model/sprite/ring/fragment/flame/light publication order; callers requesting only a
subset stay a subset. Successful empty branches stay empty: primary napalm has no
metal-fragment recipe. No shared global cosmetic random state or evaluated
particles/models cross the wire.

Explosion types 0–31, six component tags, finite nonzero normals, bounded positions,
scales/counts, original material bytes and bounded custom paths are checked before
delivery. Malformed wide counts/flags cannot truncate into valid bytes/words.
Laser's unknown-byte red fallback and lightning's unknown shape/form fallback
remain; lightning type stays 0–2. Zero/oversized beam axes fail. Blood and destruction
debris permit canonical absent source provenance; flesh/explosions retain
generation-qualified IDs. Creation never resolves those IDs into a server object.

Limits remain 8 KiB per packet, 2048 recipes and 2 MiB encoded payload per recipient/
pending queue, with separate ENet aggregate bounds. Baseline-ready clients receive
only new instant effects. Past explosions and resting cosmetic gibs are not replayed
to late arrivals, preserving the original instant-message classification.

## Client execution and preservation

The independent runtime advances these pools in solo's relative order. Explosion
fragments retain per-update gravity, displacement and angular increments; none is
converted to dt-scaled motion. Trails keep first-update behavior, distance-based
sampling, parent disappearance and finite budgets. The grenade's second light lasts
**25 seconds**; flash/fragment expiry does not remove it. Existing point-light
descriptors feed the same renderer.

Flesh uses copied collision, initial liquid classification, launch blood and impact
splats/audio. Immediate child blood consumes the frame once; its .001-second guard
prevents another update at the same clock. Flesh and destruction retain separate
pools/RNG. Destruction keeps material choices, custom model/skin/sound behavior,
LOW count halving, HIGH flesh doubling/byte wrap, glass alpha and oldest-slot fade.
The network frontend applies its existing local detail preference to future debris.
Resting fragments keep their existing eviction/world cleanup, with no new timeout.

Laser lifetime starts on its first update. Lightning keeps integer segment scaling,
cumulative rotations and the random-form typo. Pool/resource failures remain
visible without cancelling siblings. Children/audio stay local and cannot enqueue
another server event or damage objects. World reset clears pools before resources.
CMoveMgr is never called by these runtimes.

## Verification

Linux checks on 2026-10-31 pass **123 distinct native cases, 40 ASan/UBSan cases
and 123 inspector-free Release cases**. Final regression selections are 120/38/120,
with process/GPU checks counted once. The [manifest](evidence/m9_instant_checks_2026-10-31.json)
records case names, commands and hashes; raw [native](evidence/m9_instant_native_2026-10-31.txt),
[sanitized](evidence/m9_instant_asan_2026-10-31.txt),
[Release](evidence/m9_instant_release_2026-10-31.txt) and
[build](evidence/m9_instant_builds_2026-10-31.txt) logs retain earlier failures.

Codec checks cover five new alternatives, six component tags and 32 type values,
every truncated prefix, 25,000 deterministic mutations, identities, custom path/
flag/count bounds and ten-kind ordering. Existing impact/ballistic fuzz and queue
checks remain. Server publication tests cover every routed service, empty cosmetic
pools, unchanged logical voices and equal game RNG with zero/one/two consumers.

The independent retail runtime executes 26 seconds against owned collision:
six explosion components/trails, flesh launch/impact children, LOW wood debris,
beams/bolts, light tails, partial resource failure and epoch cleanup. It observes
four flesh impacts, four debris impacts, 1,045 peak trail particles, eight cosmetic
sounds, PCM peak .928111 and no missing-zero draw. Existing blood tests still assert
the exact absent gib_impact0 failure. The decoded GPU fixture uses real Crypt and
external resources, separately from real-process combat. No PBR/shader changes.

Earlier failures remain in raw evidence: a synthetic floor lacked its solid flag,
allowing indefinite free fall until the existing trail budget correctly failed;
a 12-second expiry assertion overlooked the 25-second light; the scene test counted
non-casing effects in the ordinary model pass. Fixtures were corrected without
runtime/budget changes. Howitzer had no starting ammunition under NORMAL rules;
the explosive scenario now uses Napalm's ordinary fuel. Its empty fragment branch
is accepted explicitly. A concurrent build retained the old test option once,
and the first rotation probe was still linked to protocol 20; both require current
test binaries rather than changing admission or game rules.

Dedicated plus two independent clients passes Beretta and Napalm combat natively
and under ASan/UBSan. Napalm takes two shots, one frag and USE respawn; each client
receives 13 instant recipes, including 12 explosion components, with peaks of
three explosion models and one ring. Thirteen cosmetic sounds play and two
requests fail on the exact absent zero sound per client. Beretta remains functional
with 34 recipes; its final native run has one known absent-zero request per client,
the sanitized run two. Dedicated summaries account zero discarded ticks.

Inspector-free hosting plus two independent remote combatants passes the same
Napalm death/frag/respawn route: six shots, 37 recipes (36 components plus flesh),
peaks of six explosion models and one ring, 15 cosmetic sounds and no audio failure
in that run. The host accounts eight discarded server ticks and zero discarded
client ticks. Five host map rotations also pass after rebuilding the probe. This
is scripted loopback coverage, not human LAN or impaired-network qualification.

Native and Release GPU checks run on NVIDIA GeForce RTX 3060 Laptop GPU/Vulkan.
The decoded mixed fixture changes 197,692 pixels at 640×360; returning from Enhanced
to Original reproduces identical Original bytes. The new
[Original](evidence/m9_instant_original_2026-10-31.png) and
[Enhanced](evidence/m9_instant_enhanced_2026-10-31.png) captures were visually
reviewed. The [host capture](evidence/m9_instant_host_2026-10-31.png) shows its idle
view after combat, not an explosion close-up. Independent runtime and process
binaries in all three builds have no ServerAdapter/GameSession/CPlayerObj/CWeapon/
CInventoryMgr symbols.

All five fresh [solo traces](evidence/m9_instant_oracles_2026-10-31.txt) for effects,
audio, movement, environment and camera/HUD are byte-identical to the prior impact
lot. All 6,929 protected content/source/PBR/evidence hashes remain unchanged.
Formatting, maintained links, 22 source-ledger hashes and diff whitespace pass.
No new all-map gameplay, 16-player, degraded-network, Windows/macOS, mobile/Quest,
XR or physical audio/input qualification is claimed.

## Remaining scope

Owner-bound particles, streams, PolyGrids, WarpGates, persistent light/flashlight
state and network fog remain. Visible napalm impacts do not yet include its flight
trail. Music, communication portraits, Host/Join screens, LAN discovery, chat/pings
and broader map/weapon/load/impaired-network/platform qualification remain pending.
Existing port omissions and absent commercial resources are unchanged. Solo/PBR
references stay protected; M2-GPU-001 and M3 acceptance are independent.
