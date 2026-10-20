# M2 — Original corpse blood pools

The original corpse's ground-hit request now creates `CClientSplatFX` and executes
`CBloodSplatFX`, producing a real, surface-oriented blood sprite. Original growth,
nearby-splat reuse and termination formulas are retained. The pool survives the
publisher and corpse, and unload releases it. The in-game Sprites inspector shows
its actual state/resources and failures; the corpse panel links its request ID.
**M2 remains incomplete.**

Read the [service contracts](../../contracts/blood_splat_contracts.md),
[source hashes](../sources/m2_blood_splat_sources.json) and
[validation ledger](evidence/m2-blood-splats.json). The ledger retains exact patch,
file hashes, commands, failures, final checks and sanitized logs.

## Implementation and preservation

- CCorpse preserves its animation-completion branch, downward 40-unit ray,
  primary-world-only hit, Y+.1 placement and scale/growth .1/.02. Burning corpses
  retain repeated pool requests; pool publication failure does not change corpse collision or removal.
- Runtime-only CClientSplatFX publishes a copied typed request immediately and
  removes itself on its scheduled .0001-second callback. Its original base chain
  and deferred destruction remain intact. Detached effects have no owner pointer.
- CBloodSplatFX preserves insertion order, strict squared-distance <100, the
  eleventh-existing-neighbor threshold, first nearest tie and early scan stop.
  Reuse moves the existing sprite without restarting or replacing its state.
- Growth keeps the original delta-squared multiplier and .25-second cap. Term
  keeps its previous-clock alpha and uncapped clock increment. Pools otherwise
  persist. General shared SFX-list eviction remains unavailable; capacity fails
  explicitly rather than inventing an eviction policy.
- The parallel-axis fallback already tested for CMovement is now a shared engine
  function. It supplies the source's identical forward/up normal and preserves
  movement behavior. Actual SPR/DTX frames render through the existing alpha pass,
  with original (.1,.1,.1) tint and surface rotation. No shader change is needed.

The engine keeps bounded per-session values (256 active effects, 64 reports),
non-reused identities and explicit resource/clock errors. Inspector reads do not
advance simulation. Portable RNG, alignment fallback and rasterization are stated
adaptations, not a claim of retail executable parity.

## Verification

Builds use the existing Debug inspector, inspector-free Release and Clang
ASan/UBSan configurations. Targeted CTest runs cover original splat formulas,
region boundaries/ties/early termination, failed resources/bounds, source removal,
render snapshots, object lifetime, ordinary/fire/gib deaths, AI navigation and
parallel-axis cases, corpse clearance and failure-safe world transitions.

| Configuration | Headless passed | Private-data cases |
| --- | ---: | ---: |
| Debug | 51 / 51 | 14 |
| Release without inspector | 51 / 51 | 14 |
| Clang ASan/UBSan with leak detection | 51 / 51 | 14 |

Each private pool run executes three load/unload cycles with original Cultist death
and corpse callbacks on a labelled diagnostic primary-world floor. It verifies
real pool creation after animation, .1 Y placement, all three external SPR/DTX
resources, growth completion, source-independent lifetime and empty state after
unload. This is not a complete authored combat-route test.

NVIDIA Vulkan checks pass eight distinct scenarios in Debug and Release: new
splat tint/alpha and private resource readback, application lifecycle/character
inspection, mouse combat, corpse clearance, death effects and safe level change.
Debug also reruns splat checks after the final clock-bound and device-log edits.
Logs identify **NVIDIA GeForce RTX 3060 Laptop GPU**. The synthetic blend check
verifies 20,000 opaque texels against original tint/alpha; each of three real
resources changes more than 11,000 pixels in the top-down diagnostic capture.
These captures are local, not committed commercial assets.

An early test passed 100 seconds in one update, exceeding the existing SPR
reader's 60-second bound. It now verifies the same 100-second persistence with two
valid 50-second updates; the production bound is preserved and made explicit.
The first build also caught use of ordinary quaternion field names instead of the
legacy DRotation members; the corrected implementation uses m_Vec/m_Spin. Initial
failure logs and successful final runs remain in the ledger. No acceptance check,
physics limit or timeout was relaxed.

Reference integrity verifies all 10,923 inventoried files and six reference
repositories unchanged. Commercial assets remain external. No native Metal run is
claimed for this increment; the existing shaders and dependencies are unchanged.
Blood-spurt particles, gib impact callers, generic/recursive gibs, shared SFX-list
eviction, complete save/restore and the remaining M2 services still need work.
The continuation heartbeat remains active.

Implementation commit: `933a422252e4a9eb39c23aa6267535171c173670` in the independent Blood2 repository.
