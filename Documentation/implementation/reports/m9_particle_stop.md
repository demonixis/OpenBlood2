# M9 — Original multiplayer particle stop/resume

Original `ClientShellDLL/ObjectFX.cpp:105–202` distinguishes multiplayer from solo
when an untimed trail observes an unchanged owner position. The existing port had
only the solo branch. Sparse network poses would therefore freeze a trail and
start its terminal lifetime on the first repeated position.

The original multiplayer branch now has an explicit, default-off argument through
`CObjectParticleFX::Update`, owner-bound `ObjectParticles::advance` and fragment
`ExplosionFragments::advance`. `NetworkEffects` enables it for its existing
replicated explosion-fragment children. Solo callers retain their previous default;
no global game mode, server pointer or original callback enters the client effect.
Protocol remains **22**.

The first stopped observation records its absolute clock and freezes movement.
A moving observation before terminal duration restores movement and clears the
stop clock. Exactly one second is still inside the grace period; only
`now > stop_time + 1` sets `duration = elapsed + ramp_down + max_life` for the selected
stop/ramp recipes. Once set, duration is terminal and later motion cannot restart
it. Existing first-update behavior, per-update emission, particle integration,
source RNG order, strict duration expiry and immediate owner removal remain.
Authored finite-duration spells bypass this motion branch. A negative clock is the
checked replacement for the original first-stop boolean; reinitialization resets it.

This prerequisite fixes already-networked fragment trails and prepares projectile
trail delivery. It does **not** send owner-bound ObjectFX recipes yet. That next
lot must preserve multiple timed effects per owner, distinct publication identities,
creation/removal generations and late-arrival semantics. The audit found repeated
Voodoo/shield effects and paired Death Ray recipes on one owner, so a single optional
recipe per object would be insufficient. Sources remain unmodified.

## Evidence

The [source ledger](../sources/m9_particle_stop_sources.json) records exact source
hashes. The [validation manifest](evidence/m9_particle_stop_checks_2026-10-31.json)
lists the commands, cases, raw results, artifact hashes and preservation checks.

A new independently linked private-data test covers all eleven selected moving
trail recipes, stop/resume/reset, the exact one-second boundary, terminal tails,
unchanged solo stopping and immediate owner loss. Fourteen finite spell recipes
retain their authored duration. Tests exercise mode forwarding through owner and
fragment pools; resting fragments use unavailable collision services deliberately,
so accidental movement would fail rather than silently pass. The existing complete
instant-effects test still checks collision, child effects/audio, particle budgets,
26-second expiry and world cleanup.

Final results: **32 native tests, 3 ASan/UBSan tests and 32 inspector-free Release
tests pass**. Native/Release include original particle/fragment rules and save/load
regressions; the server-only sanitizer build registers the three independent network
runtime cases. Native client/dedicated and Release client targets build. The five
fresh solo traces (effects, audio, movement, environment and view) match the prior
lot byte for byte; all **6,946 protected hashes** remain unchanged. Independent test
binaries contain no server adapter/session/player/weapon/inventory symbols.
No new GPU readback or process-play qualification is claimed for this prerequisite.

No renderer, shaders, PBR files, data, movement integration, gameplay rules or
logical server audio/RNG change. This Linux test lot does not qualify human LAN,
other platforms, impaired multiplayer combat, all-map traversal or 16-client load.
M9 remains active; independent M2-GPU-001/M3 gates remain unchanged.
