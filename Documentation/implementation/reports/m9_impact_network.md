# M9 — Independent impact sprites, sparks and blood

Network clients now execute the existing impact flash/smoke, sparks, blood-spurt
and blood-splat ports, alongside the prior shared ballistic composite. This
includes typed world sparks and corpse-pool publications, immediate weapon jets,
deferred server-gib requests, and client blood-emitter child splats/impact audio.
The network server publishes values without creating cosmetic instances through
these routed services. Original selection, damage, logical audio and per-weapon
recipe RNG remain on their existing paths. Solo has no forwarding hook and is unchanged.

This remains a partial M9 delivery. Explosion/flesh-fragment producers, persistent
owner-bound effects and other presentation consumers still need network delivery.

## Original rules and adaptation

The existing `CWeaponFX::CreateSprites/CreateSparks/CreateBloodSpurt` port keeps
its surface/ammo gates, selection order and damage-derived recipes. In particular,
flesh always draws the blood-choice random value before testing damage; jets occur
only when that draw is zero or raw damage exceeds 25. Beretta hits do not guarantee
jets. Original `WeaponFX.cpp` immediately updates both created jets, whereas the
server-gib publication starts through the normal effect update. `ClientSplatFX`
uses an instant message and removes its temporary server publisher independently.
The [source ledger](../sources/m9_impact_network_sources.json) records the reviewed
original files and selective reconstruction baselines; no game algorithm is
newly imported or changed here.

The old ballistic-only message becomes protocol **20** `B2EF`, with five tagged
recipe alternatives in one reliable Control sequence. This preserves the existing
port's publication order across shared child pools. Values move to a common
descriptor header; the original runtime classes still validate and execute them.
The wire encodes every relevant field, including null-versus-empty resources,
smoke flags/delay/offset, flash waveform/fade/alignment, blood texture selection,
immediate-update flag and splat quaternion/growth. Source IDs remain generation
qualified. Only the two blood alternatives permit the canonical absent ID used
by original ownerless client publication; it is not a server-object lookup.

Messages remain bounded to 8 KiB, source labels to 100 bytes, resources to 1000
bytes and clipped polygon rings to 512 points. Decode rejects truncation, trailing
data, unknown tags/reserved flags, invalid identities, nonfinite/out-of-bounds
values and malformed optionals before changing the output. Original finite
reversed blood velocity bounds, zero-particle sparks, unknown surface bytes and
default resource semantics remain accepted. Missing actual assets are separate
client creation diagnostics rather than invented substitutions.

Each recipient now has a shared limit of **2048 recipes and 2 MiB of encoded
payload**. The count accommodates multiple components per impact while the byte
bound retains the former ballistic queue's maximum payload envelope. Both server
outboxes and client pending queues enforce these limits, reset byte credit on
drain/world change and reject overflow independently. This does not qualify
16-player load or eliminate the existing ENet aggregate 2 MiB peer queue limit.
No global shared cosmetic RNG or particle state is serialized.

Baseline readiness still gates delivery. There is no replay of historical instant
effects, including old blood pools. Detached effects survive source removal;
world change/disconnect clears runtime pools and pending descriptors before
resource stores. Persistent owner-bound FX require their own later baseline.
Broadcast still reaches every ready peer without PVS/relevance filtering.

## Client execution and audio

The client advances its existing bounded 60 Hz clock before creating this tick's
pending effects, then runs the existing relative pool order: blood splats,
weapon sprites, ballistic fragments/casings, sparks and blood emitters. Immediate
blood consumes the frame duration once at creation; its original .001-second
deadline prevents the following pool advance from moving it again. Deferred blood
starts with zero elapsed time. Fresh flashes retain full initial alpha and fresh
casings become visible on their next ordinary update. This corrects the prior
network composite's creation-at-previous-tick phase; local solo clocks are untouched.
CMoveMgr is never called by this runtime.

Blood emitter collision and initial contents come from the independent client
replica. Original 30 Hz bounded emission, first-impact stop, local splat creation,
growth/reuse and primary-world collision quirks remain. Child calls use
`EffectServices`, so they cannot re-enter server callbacks or enqueue another
network event. Original component capacities and partial failures are retained.

Blood-drop and casing impact sounds share the existing client playback budget.
The cumulative audio failure counter now retains its last exact resource/error
through cleanup for the session log. The original `sounds/gibs/flesh/gib_impact0.wav`
request remains absent in these retail data: it fails visibly and is never mapped
to another numbered sound. Logical server voices, completion and game RNG remain
independent of these requests.

## Verification

Linux validation on 2026-10-31 passes **77 distinct native cases, 37 headless
ASan/UBSan cases and 77 inspector-free Release cases**. Final regression selections
contain 73/35/73 cases respectively; additional real-process and GPU checks are
counted once. The [manifest](evidence/m9_impact_checks_2026-10-31.json) records exact
case names, commands and SHA-256 hashes. Raw [native](evidence/m9_impact_native_2026-10-31.txt),
[sanitized](evidence/m9_impact_asan_2026-10-31.txt),
[Release](evidence/m9_impact_release_2026-10-31.txt) and
[build logs](evidence/m9_impact_builds_2026-10-31.txt) retain earlier runs too.

New codec coverage exercises all five variants, every truncated prefix, 25,000
deterministic mutations, optional resource distinctions, field limits and mixed
sequence ordering. The existing ballistic codec retains its additional 10,000
mutations and maximum-polygon checks. Independent byte and count exhaustion are
tested on both sides; one stalled server recipient does not change another's
sequence or logical simulation.

The server test requests all routed families and both source/ownerless forms,
checks their shared order and verifies empty server cosmetic pools, unchanged
logical voice count and identical subsequent game RNG samples with zero, one and
two consumers. These are typed publication stimuli, distinct from process combat.

The independent retail runtime test covers all five families, immediate/deferred
blood positions/update counts, real child splats/PCM, expiration, partial missing
flash with successful smoke, detached source lifetime and map cleanup. Its two
blood emitters each request the known absent zero sound; both omissions are
asserted explicitly while successful casing PCM remains audible in the buffer.
The GPU fixture uses decoded synthetic recipes in real Crypt geometry; it does
not substitute for multi-process combat or human LAN.

Dedicated plus two independent clients passes both Beretta and sniper-rifle
encounters natively and under ASan/UBSan. Each completes actual server damage,
one frag, USE respawn and departure. Beretta clients receive 34 recipes
(13 ballistic, 13 sparks, 8 jets); the rifle receives four (1/1/2) from one shot.
The rifle's original 100–125 raw damage guarantees the blood branch without
changing RNG or source rules. Native Beretta playback has three missing-zero
sound requests per client, sanitized Beretta two; both rifle runs have zero.
Every observed failure is matched to the exact absent resource in live reports.

Inspector-free graphical hosting plus two independent remote combatants passes
both encounters: Beretta clients receive 54 recipes (33/13/8), rifle clients four
(1/1/2). Each Beretta client reports two known missing-zero sounds, each rifle
client one. Host summaries retain those exact errors as well. Each host run
accounts nine discarded server ticks; client summaries account none. The
five-transition host rotation case also passes. These scripted loopback tests
are distinct from a human LAN session or impaired-network/load qualification.

An earlier Release Beretta run failed an incorrect test assertion requiring jets
on every encounter, despite successful lethal combat. Its log is preserved.
The corrected test requires guaranteed ballistic/spark receipts; the separate
rifle scenario checks the guaranteed blood branch. No gameplay probability was
changed to make the test pass.

Native and Release GPU fixtures run on NVIDIA GeForce RTX 3060 Laptop GPU/Vulkan.
The mixed fixture changes 19,421 pixels at 640×360 and restores byte-identical
Original output after Enhanced. New [Original](evidence/m9_impact_original_2026-10-31.png)
and [Enhanced](evidence/m9_impact_enhanced_2026-10-31.png) captures show the decoded
fixture in Crypt. The [host capture](evidence/m9_impact_host_2026-10-31.png) shows
the idle host after the rifle encounter, not an impact close-up. All were visually
reviewed. Independent runtime/process binaries in all three builds contain no
ServerAdapter/GameSession/CPlayerObj/CWeapon/CInventoryMgr symbols.

Five fresh [solo traces](evidence/m9_impact_oracles_2026-10-31.txt) for effects,
audio, movement, environment and camera/HUD are byte-identical to the preceding
ballistic lot. All 6,920 protected source/content/PBR/evidence hashes remain
unchanged. Formatting, source-ledger hashes, maintained links and diff whitespace
were checked. No new Windows/macOS, mobile/Quest/XR, all-map gameplay, 16-player,
physical audio/input or human LAN qualification is claimed.

## Remaining M9 scope

Explosions, flesh/destruction fragments, laser/lightning effects, persistent
particles/PolyGrids/lights/WarpGates, projectile lights and network fog remain to
route. In particular, child blood from still-unmigrated server-side cosmetic
fragment pools is not made remote by this lot. Music, communication portraits,
Host/Join screens, LAN discovery, chat/measured pings and the broader gameplay/load/
network/platform qualification remain pending. Solo, PBR files and immutable GPU
references are protected; M2-GPU-001 and M3 acceptance remain independent.
