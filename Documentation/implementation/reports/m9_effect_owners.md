# M9 — owner-bound effects on the client replica

Implemented against `9cadcc3a`, continuing
[client effect ownership and fragment queries](m9_effect_queries.md) under the
[M9 contract](../../contracts/multiplayer_contracts.md).

## Delivered boundary

Object particles, particle streams, PolyGrids, WarpGates and evaluated CLightFX
lights now retain `ClientObjectId`, containing the world epoch and generational
object identity. They no longer keep HOBJECT or call ServerDE during updates.
Creation receives copied `EffectOwner` values: identity, pose, user flags and the
diagnostic source name. The immediate local publisher still validates the live
server object and refreshes its attachment before copying, as the former position
and rotation services did.

After authoritative frame completion and attachment refresh, a solo step publishes
an object frame if any owner-bound effects need it. Every subsequent owner lookup
uses `ClientWorld`. Publication does not advance clocks, movement, animation keys,
RNG, sound or callbacks. The full presentation publication remains available at
its previous boundary; the extracted object-only publication does not capture an
intermediate effect frame. A failed owner publication fails the step explicitly
and is never replaced by live-server queries.

All five families preserve the existing source calculations, emission budgets,
random seeds, diagnostic limits and update order. In particular:

- PolyGrids poll position and user-flag visibility, retaining creation rotation;
  hidden grids retain their prior wave-update gate.
- Particle streams and object particles retain their original missing-owner
  removal, not an invented detached tail or a new smoothing rule.
- Warp sprites and both particle systems keep their separate child failure,
  alignment, ramp and emission-clock behavior.
- CLightFX retains absolute waveform phase and original off/visibility behavior.
  Its evaluated state is advanced in the client runtime; server light rules and
  source object scheduling are unchanged.

A slot reused with a different object generation or in another world cannot supply
an old effect's pose. Client runtime reset receives the new epoch and clears the
same pools. Immediate removal, restore rollback, duplicate-publication checks,
resource validation and inspector lookups use checked identities at their local
boundaries. No save payload, CMoveMgr integration, logical audio service, renderer,
shader or PBR file changes.

Existing tests that deliberately bypass `GameSession::step` now explicitly publish
an owner frame before updating effects. The generic synthetic REZ writer is moved
out of the player fixture without changing its algorithm, so the standalone
client test can load real synthetic sprite/texture data without game linkage.
The [source ledger](../sources/m9_effect_owners_sources.json) records provenance.

## Verification

Linux x86_64 with GCC 16.2.1, Clang 22.1.8 and native NVIDIA RTX 3060 Laptop
Vulkan. The [validation record](evidence/m9_effect_owners_validation_2026-10-30.json)
distinguishes synthetic and private-data checks:

- **342/342 headless cases pass**: 46 targeted ownership/save cases and 296
  remaining selected cases, without repeating the completed targeted selection.
- **12/12 selected GPU cases pass after one fixture correction**. The initial
  `gpu.explosive_effects` failure is retained: its smoke fixture inserted particles
  with ID zero, bypassing the normal publisher. It now supplies that ID, like the
  previously corrected flare/energy fixtures. Production validation is unchanged.
- **3/3 ASan/UBSan cases pass** in the isolated server-only tree, including
  180 ticks each in two retail worlds and the standalone owner-generation test.
- **All five preservation traces remain byte-identical**, using the prior
  240 FX / 360 audio / 360 movement / 720 environment / 300 camera-HUD frame
  references. SHA-256 values are recorded in the validation JSON.
- The sanitized dedicated `multi/bb_crypt` smoke completes 60 ticks with zero
  clients, unavailable audio/video drivers and zero discarded ticks.
- The standalone owner test has no GameSession, ServerAdapter, Objects or server
  shell symbols. All 6,233 protected hashes, including the 39 pre-existing PBR
  files and earlier evidence, remain unchanged.

The builds use the isolated `build/m9-fx-runtime` and `build/m9-fx-runtime-asan`
trees described in the [preceding lot](m9_effect_queries.md). An initial compile
identified two remaining owner-handle consumers in world validation; both now use
checked identities. Existing fixtures and inspection paths are adapted to the
same boundary. No source effect calculation is changed.

Evidence: [targeted](evidence/m9_effect_owners_targeted_2026-10-30.txt),
[headless remainder](evidence/m9_effect_owners_headless_2026-10-30.txt),
[initial GPU](evidence/m9_effect_owners_gpu_2026-10-30.txt),
[GPU rerun](evidence/m9_effect_owners_gpu_retry_2026-10-30.txt),
[sanitizers](evidence/m9_effect_owners_asan_tests_2026-10-30.txt),
[dedicated](evidence/m9_effect_owners_dedicated_2026-10-30.txt).

To reproduce, rebuild the [listed targets](evidence/m9_effect_owners_targets_names_2026-10-30.txt)
and pass the [headless names](evidence/m9_effect_owners_headless_names_2026-10-30.txt)
and [GPU names](evidence/m9_effect_owners_gpu_names_2026-10-30.txt) to
`ctest --tests-from-file` in that build tree (GPU serially). The sanitizer subset
is `ctest -R '^network\.(effect_owners|effects_local|effects_private)'`.
No simulator or emulator is used. The new independently linked test creates all five families, changes
owner pose/visibility, replaces a generation, changes world epoch and resets the
runtime. It checks the source-specific PolyGrid rotation and hidden-update rules
and rejects duplicate stream/surface publications.

## Remaining work

Blood spurts, flesh/destruction fragments, casings and the flashlight still have
legacy collision/audio or child-effect service dependencies. Other effect creation
continues through the immediate local publisher. Typed network effect events,
remaining shell services, admission and multiplayer gameplay remain pending.
This lot is not remote-client or LAN qualification.

Only Linux local execution is covered. Windows/macOS/mobile/Quest, XR hardware and
human LAN qualification remain separate. The earlier `gpu.polygrid` exact-pixel
failure, M2-GPU-001 and M3 acceptance are unchanged. Existing PBR files and earlier
evidence are preserved by hash and excluded from this lot.
