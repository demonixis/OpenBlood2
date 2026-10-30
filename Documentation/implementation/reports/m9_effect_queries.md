# M9 — client effect ownership and fragment queries

Implemented against `7c2fe6ad`, following the
[multiplayer contract](../../contracts/multiplayer_contracts.md). This continues
[passive effect presentation](m9_client_effects.md): it moves ownership of the
original cosmetic pools to the client and makes fragment updates independent of
the authoritative registry. Full M9 remains in progress.

## Delivered

`ClientSession` owns an optional `EffectRuntime`; local games supply
`OriginalEffects`. That runtime owns the twenty existing effect pools, including
lights and the separate flesh/destruction pools. Reset retains their original
clear order, seeds, monotonic identity counters and bounded diagnostics. The
local adapter retains references for immediate publication and inspection, not
ownership. The runtime is destroyed after the adapter. Core-only clients need
no game runtime.

The `blood2_client_effects` static library contains the existing cosmetic
implementations and depends on `engine_core`, independently of authoritative
game classes. Five unnecessary registry includes are replaced by the math/property
header they actually use. A test creates, resets and destroys all pools without
linking a GameSession, ServerAdapter, Objects registry or server shell.

`EffectQueries` exposes only read-only segment/ray/container queries. Its client
implementation uses `ClientCollisionWorld`'s owned geometry and values; it cannot
schedule callbacks, apply damage, integrate a body or access a server handle.
Primary-only rays preserve the original finite domain and zero texture flags.
General segments preserve SolidOnly versus SolidOrRayHit. The first containing
volume still masks later volumes, including a hidden or zero-code first volume.
Unavailable geometry remains an explicit failure with no live-server fallback.

`CExplosionFragFX::Update` now consumes those queries and records a generational
ObjectId hit instead of HOBJECT. Its arithmetic, per-update movement/gravity,
reflection, fade, terminal update and trail ordering are unchanged. At the same
point in the solo update, live fragments receive a collision snapshot taken after
authoritative frame completion. This publication does not advance CMoveMgr or any
server object. The authoritative query implementation remains for comparison tests
and other legacy services; normal fragment updates use the client implementation.

[Provenance](../sources/m9_effect_queries_sources.json) records the original and
pre-extraction implementation hashes. No game rule, saved payload, renderer,
shader, server audio timeline or RNG draw changes.

## Verification

Linux x86_64, GCC 16.2.1 and Clang 22.1.8. The
[validation record](evidence/m9_effect_queries_validation_2026-10-30.json) reports:

- **305/305 headless cases pass**: the prior 303-case selection plus two new
  query/lifecycle boundaries. The first run left 27 cases unrun because four
  executables were missing from the copied build-target list. Building them and
  rerunning those 27 passes; no test failure is suppressed.
- **4/4 ASan/UBSan cases pass**, including two retail worlds simulated for 180
  ticks each, in a fresh server-only build.
- **Five traces remain byte-identical**: 240 FX frames, 360 audio, 360 movement,
  720 environment and 300 camera/HUD frames, against the previously committed
  references. The validation record includes their SHA-256 values.
- Native NVIDIA RTX 3060 Laptop Vulkan movement and scene-explosion smoke runs
  pass, including world unload/re-entry. The full GPU pixel suite is not repeated
  for this runtime-only change; its earlier PolyGrid failure remains open.
- Sanitized dedicated `multi/bb_crypt` runs 60 ticks with zero clients, unavailable
  video/audio drivers and zero discarded ticks. This is a smoke test, not load
  qualification.
- The independent runtime test contains no GameSession, ServerAdapter, Objects or
  server-shell symbols. All 6,224 protected files, including the 39 pre-existing
  PBR files and earlier evidence, retain their hashes.

The isolated builds use `build/m9-fx-runtime` (GCC, RelWithDebInfo with
`-O2 -g0 -DNDEBUG`) and `build/m9-fx-runtime-asan` (Clang, server-only, sanitizers,
`-O1 -gline-tables-only`). No other build writes to these trees.

Evidence: [headless](evidence/m9_effect_queries_headless_2026-10-30.txt),
[missing-executable rerun](evidence/m9_effect_queries_headless_retry_2026-10-30.txt),
[targeted fragments](evidence/m9_effect_queries_targeted_2026-10-30.txt),
[sanitizers](evidence/m9_effect_queries_asan_2026-10-30.txt),
[dedicated](evidence/m9_effect_queries_dedicated_2026-10-30.txt),
[movement smoke](evidence/m9_effect_queries_movement_smoke_2026-10-30.txt),
[effects smoke](evidence/m9_effect_queries_effects_smoke_2026-10-30.txt).

Rebuild the [listed targets](evidence/m9_effect_queries_targets_2026-10-30.txt),
then use `ctest --tests-from-file` with the previous
[303 test names](evidence/m9_client_effects_headless_names_2026-10-30.txt), and
`ctest -R '^network.effect_queries'` for the new boundaries. The sanitizer subset
uses `ctest -R '^network\.(effect_queries|effects_local|effects_private)'`.
The five unchanged oracle sources/references are linked from
[the prior presentation report](m9_client_effects.md). The new independent test covers pool lifecycle, primary/general rays,
visibility masking, unavailable geometry and fragment bounce/rest/fade/expiry.
The local comparison runs two fragment trajectories over 120 updates each against
the same geometry, checking exact position, velocity, impacts, alpha and object
identity. Client updates must leave the server service-call count unchanged; a
removed collider disappears at the next publication.

## Remaining work

Ownership is not full service separation. Owner-bound effects still retain local
handles and update through the adapter; blood, gibs and casings still use legacy
collision, child-effect and cosmetic sound services. Creation still uses the
immediate local publisher. Those dependencies, typed network events and the
remaining client shell will be extracted in subsequent M9 lots.

Only Linux local validation is covered here. Network gameplay, late admission,
LAN human play, degraded networking and Windows/macOS/mobile/Quest qualification
remain pending. The previous exact-pixel `gpu.polygrid` failure, M2-GPU-001 and M3
acceptance remain separate. Existing PBR files and earlier evidence are excluded
from the change and checked by hash.
