# M9 — moving effects and child publications on the client

Implemented against `7c4b9fd1`, following [cosmetic impact audio](m9_effect_audio.md)
under the [M9 contract](../../contracts/multiplayer_contracts.md).

## Delivered boundary

Flesh and destruction fragments, ballistic casings and blood-drop emitters no
longer accept ServerDE or original handles, either at creation or during updates.
Their recipes receive copied owner values and narrow read-only EffectQueries.
EffectServices supplies typed client outputs for impact sounds, blood marks,
child spurts, sparks and weapon sprites, together with the existing phase clock
and frame delta. It retains no gameplay callback or server random generator.

After authoritative frame completion, the local session publishes one collision
scene for the moving cosmetic families. Updates query that owned scene and child
effects publish synchronously into the same client's pools. No CMoveMgr integration,
contact callback or additional server physics belongs in this phase. Source pool
order, primary-only collision, zero texture flags, bounce/fade arithmetic, sound
selection, per-pool RNG and immediate first blood-spurt update remain unchanged.
Destruction/casing failures keep their existing nonfatal diagnostic behavior.

The synchronous local creation bridge still queries authoritative geometry while
inside original callbacks; it supplies the same copied recipes and client output
services. This preserves creation-time resource errors, ordering and RNG without
copying a busy object registry. Such queries are not retained by effects. Full
remote creation events and their admission/asset lookup remain subsequent work.
Ballistic clipped surface polygons still arrive as copied values from that bridge.
Server effect aliases remain for publishers and inspection. No wire codec or
network event queue is claimed by this lot.

The [source ledger](../sources/m9_effect_services_sources.json) identifies the
existing adaptations and original gib, casing, emitter and client-physics sources.
Original references and PBR files are unchanged. No save format, gameplay sound,
movement integration, renderer or shader changes.

## Verification

Linux native validation uses the isolated runtime and sanitizer trees described
in the [runtime lot](m9_effect_queries.md). The standalone `network.effect_services`
test links only blood2_client_effects and engine_core. It creates actual synthetic
models, sprites and sound resources, launches flesh/destruction fragments, spurts
and shotgun casings, advances 180 steps, and checks impacts, child marks/audio,
independent client pools, unavailable services and reset. Its initial fixture
omitted the floor's solid flag; the corrected fixture explicitly verifies its
collision before launching effects. Initial output is retained.

`network.effect_queries_local` compares 120 steps of flesh, casing and emitter
motion through the legacy and client query backends, including impact counts,
angles, rest state, emissions, sound selection and RNG state. It also verifies
that client updates add no server service calls. Existing fragment comparisons
remain. Direct legacy unit fixtures now explicitly supply local effect services.

The [validation record](evidence/m9_effect_services_validation_2026-10-30.json)
records **44/44 distinct headless cases**, **4/4 selected GPU cases after fixture
correction**, and **4/4 ASan/UBSan cases**. Five prior solo traces remain byte-identical
(240 FX / 360 audio / 360 movement / 720 environment / 300 camera-HUD frames).
Sanitized dedicated Crypt completes 60 ticks with unavailable audio/video drivers
and zero discarded ticks. All 6,252 protected hashes remain unchanged. The standalone
binary has no GameSession, ServerAdapter, Objects or server-shell symbols.

The initial GPU shotgun fixture rewound the server world clock after an already
published replica, correctly rejected by ClientWorld. It now clears only the
cosmetic pools and logical audio it intended to isolate, leaving the monotonic
world clock intact. Production clock validation and captured reference files are
unchanged. The initial failure and successful single-test rerun are retained.
The first standalone compilation also exposed an incorrect fixture pool name;
its initially unbuilt CTest selection is recorded as Not Run, never as a pass.

Rebuild the [listed targets](evidence/m9_effect_services_targets_2026-10-30.txt), then
use the [targeted](evidence/m9_effect_services_targeted_names_2026-10-30.txt),
[headless](evidence/m9_effect_services_headless_names_2026-10-30.txt) and
[GPU](evidence/m9_effect_services_gpu_names_2026-10-30.txt) names with
`ctest --tests-from-file`; run GPU serially. Synthetic tests,
private retail execution and native GPU smoke remain distinguished. No simulator
or emulator is used.

## Remaining work

Flashlight presentation still needs its separate client owner/query boundary.
Immediate event creation, the remaining shell services, remote admission and
network gameplay remain ahead. This is not multiplayer acceptance or human LAN
qualification. Windows/macOS/mobile/Quest remain unqualified here. The prior
PolyGrid pixel issue, M2-GPU-001 and M3 acceptance remain separate.
