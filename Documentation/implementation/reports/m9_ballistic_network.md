# M9 — Independent network ballistic effects

The first network cosmetic family now reaches independent clients: the existing
shared bullet/shotgun/BMG composite, including casings, clipped bullet marks,
material fragments, muzzle smoke, liquid splashes/ripples/bubbles and the source
normal-based bullet tracer. The dedicated publishes impact recipes; each client
owns the original effect pools, resource bindings, collision queries, cosmetic
random draws, playback and presentation frame. The graphical host uses this same
endpoint. This is a partial FX delivery, not completion of M9.

## Source and boundaries

Original `CWeapon::AddImpact` applies the 1.25-unit normal offset and calls
`SendWeaponSFXMessage` in `ObjectDLL/ClientWeaponSFX.h`. That active helper uses
`StartInstantSpecialEffectMessage`; the nearby object-creation block is commented
out. The existing selective `ShotgunEffects` port retains WeaponFX, ShellCasingFX,
SurfaceFragmentFX, MarkSFX and TracerFX arithmetic. No additional game algorithm
is imported. The [source ledger](../sources/m9_ballistic_network_sources.json)
records exact original and reconstruction baselines.

Protocol **19** adds reliable Control-channel `B2BF` recipes. Each message has a
world epoch, private recipient, monotonic sequence, generation-qualified source
identity/name, six vectors, raw surface byte, checked ejection/hand/ammo fields
and the copied authoritative clipping polygon. Bounds are 8 KiB per packet,
100 source-name bytes, 512 polygon points and 256 queued recipes per recipient.
Little-endian scalar encoding, finite coordinates/colors, nonzero directions,
strict booleans and complete consumption precede client mutation. Unknown surface
bytes preserve the source fallback; pointers, assets and GPU structures are absent.

Only baseline-ready recipients receive new instant recipes. Historical impacts,
casings and client-persistent marks are not replayed to late arrivals. Source
removal does not cancel a detached impact; its identity is provenance, not an
instruction to resolve or run a server object. Duplicate sequences do nothing;
gaps, wrong recipients and current-world mismatches fail. The endpoint discards
older world epochs. A full authoritative recipient queue fails that peer using
the existing capacity-disconnect path without changing the shot or other peers.
Delivery currently broadcasts to all ready peers without PVS/relevance filtering;
this bounded transport policy is a reconstruction adaptation.

`ServerAdapter::PublishShotgunEffects` retains its original resource/source gates
and clipping prepass, then forwards the recipe when a network session is attached.
It no longer creates or advances this composite's server-side cosmetic pools.
Solo has no forwarding hook and retains its existing synchronous path. Other FX
families still use their prior path and remain to extract.

`NetworkEffects` advances only cosmetic pools on the existing client 60 Hz ticks.
It uses independent client collision for containers and primary-world casing/ray
queries, with the original primary-only texture-flag quirk. Executed ticks age
effects; discarded catch-up time is not replayed. It never calls CMoveMgr or adds
a movement integration. Missing cosmetic components remain counted diagnostics;
invalid protocol/frame state fails the endpoint. World change and disconnect clear
pending recipes, runtime pools and presented state before resource stores.

The shared `append_runtime_effects` function now copies runtime presentation for
both solo and network clients, preserving its prior order. Solo-only projectile
light/fog extraction stays in `LocalEffects`. The existing Original/Enhanced
scene consumers still own rendering; no shader, GPU registry, post-processing or
XR rendering contract changes.

## Local cosmetic audio

`EffectAudio` exposes the existing single effect-play operation to solo ClientAudio
and NetworkAudio. Casing impacts use local buffered spatial voices and client PCM
time. They never create logical server voices, retain speech handles or consume
game RNG. Completed voices are reclaimed automatically. All playback shares the
existing 128-voice budget: incoming authoritative sounds retire old server tails
first, then disposable cosmetic voices, then preempt retained speech while
preserving its silent completion clock. Invalid/full cosmetic requests remain
explicit failures. World cleanup removes all such playback.

## Verification

Linux checks on 2026-10-31 finish with **49 distinct native, 34 headless
ASan/UBSan and 50 inspector-free Release cases passing**. The
[manifest](evidence/m9_ballistic_checks_2026-10-31.json) lists exact cases,
commands and hashes. Raw [native](evidence/m9_ballistic_native_2026-10-31.txt),
[sanitized](evidence/m9_ballistic_asan_2026-10-31.txt) and
[Release](evidence/m9_ballistic_release_2026-10-31.txt) logs retain earlier failures
and final results. The sanitizer build excludes the graphical client; its
dedicated process starts no window or audio device.

The codec test covers every truncated prefix including maximum clipping payload,
oversize packets/arrays, invalid fields, 10,000 deterministic byte mutations,
duplicate/gap/private/world handling and the 256-event client queue. Independent
retail runtime coverage checks bullet/shell/BMG casing counts, detached sources,
marks/smoke/tracers, real PCM, 25-second expiry and world cleanup. The same binary
has no ServerAdapter, GameSession, CPlayerObj, CWeapon or CInventoryMgr symbols.

The server test supplies typed effect requests to three fresh sessions with zero,
one and two consumers. It verifies no instant baseline replay, per-recipient
sequence/identity, overflow isolation, unchanged logical audio and identical next
game RNG samples. These requests are test stimuli, not end-to-end weapon fire;
the separate multi-process combat test exercises actual checked weapon fire.

| Production process case | Effects per remote client / failures | Casing sounds per remote client | Server ticks / discarded |
| --- | ---: | ---: | ---: |
| Native dedicated, BloodArena | 13 / 0 | 25 | 2,400 / 0 |
| ASan/UBSan dedicated, BloodArena | 14 / 0 | 27 | 2,400 / 0 |
| Release graphical host, Fields | 33 / 0 | 65 | 3,607 / 9 |

All three complete lethal damage, one frag, USE respawn and clean departure.
The Release host's local endpoint also receives 33 effects without failure and
plays 66 cosmetic sounds. Effect counts differ with actual shot paths/timing;
they are not a packet-loss metric. Its final [capture](evidence/m9_ballistic_host_2026-10-31.png)
shows the idle Fields view/HUD after combat, not an impact-visibility assertion.
Separate existing rotation processes pass four ASan dedicated rotations and five
Release host rotations. Those rotation cases fire no weapons; live pool cleanup
is covered by the independent runtime test instead.

The new decoded-recipe GPU fixture renders the actual retail composite in Crypt.
Its [Original](evidence/m9_ballistic_original_2026-10-31.png) and
[Enhanced](evidence/m9_ballistic_enhanced_2026-10-31.png) captures were visually
checked. Adding the composite changes 1,060 pixels at 640×360; switching
Original → Enhanced → Original restores identical Original pixels. The fixture
uses a synthetic wire impact in real geometry, not network combat. Both native
and Release use the logged NVIDIA GeForce RTX 3060 Laptop GPU through Vulkan,
dummy audio and isolated user directories. Physical audio/input and human LAN
are not tested. No validation diagnostics occur in these final runs.

The audio regression fills all 128 slots with retained speech plus cosmetic
voices, verifies authoritative sound admission preempts a cosmetic first, checks
actual spatial PCM, reclaims completed playback and clears a world. Existing
speech completion/preemption, cache bounds, effect receivers/services/owners,
admission/player codecs, checked fire/death/ACKs and endpoint rotation pass.
Native and Release additionally pass seven existing ballistic cases (including
liquids/clipping) and eight save-session cases.

Five fresh [solo traces](evidence/m9_ballistic_oracles_2026-10-31.txt) match the
previous CTF-capture lot byte for byte: effects 240 frames, audio 360, movement
360, environment 720 and camera/HUD 300. Effects read external retail data;
the other four use separate synthetic fixture directories. All **6,911 protected
hashes** match, including PBR files and immutable evidence. The unchanged mounted
retail SHA-256 is recorded from real server admission in the manifest.

[Build logs](evidence/m9_ballistic_builds_2026-10-31.txt) record targeted
`cmake --build build/{m9-fx-runtime,m9-fx-runtime-asan,release} --parallel 3
--target ...` followed by `ctest --test-dir BUILD -R SELECTOR -V` (two jobs for
short regressions, serial process/GPU checks). Initial integration failed when
the codec included the heavy runtime header; extracting pure settings and
qualifying the existing WeaponDefs legacy include fixed it. Fixture corrections
added the missing collision epoch/active collider, used its nested motion member,
and checked casings in the existing opaque model pass. The first ASan scene run
still used that old pass assertion; rebuilding the fixture passes. None of these
failed attempts are counted as successes. The client-only Release PvP CTest gate
now correctly depends on the client/GPU options, without requiring the dedicated
target. Final formatting and code/document diff checks pass; verbatim logs retain
their original whitespace. The external oracle retains its pre-existing
misleading-indentation warning, not a new production warning.

## Remaining work

Direct impact flash/smoke requests, world sparks, blood, explosions, projectile
lights, persistent owner-bound effects and fog are not yet sent. Only child smoke
and bubbles produced by this ballistic composite reach the new runtime. Server
FX extraction, persistent baselines, music, communication portraits, Host/Join UI,
LAN discovery, chat/measured pings and broader multiplayer qualification remain.
This lot does not qualify all 22 maps, 16-player combat, degraded networks, human
LAN, Windows/macOS interoperation or mobile/Quest/XR hardware. M2-GPU-001 and M3
acceptance remain separate.
