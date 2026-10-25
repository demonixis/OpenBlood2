# Original destruction debris: wood, metal and glass

Beta VFX lot 1 (2026-10-25), under M3.7/M4. The existing CDebris requests now create
visible client models for surfaces 20 (metal), 30 (wood) and 50 (glass). Existing
WorldModelDebris, flesh-gib behavior, damage, spawning and save payloads are unchanged.
See [source hashes](../implementation/sources/destruction_debris_sources.json) and
[verification](../implementation/reports/destruction_debris.md).

## Source behavior

- CDebris retains its material iteration order, per-material negation of the shared launch
  vector, amount, dimensions and scale. Destructible brushes/models publish before deferred
  source removal; fragments survive independently. No extra damage or collision body is added.
- SFXMgr detail LOW halves the byte count (integer division); MEDIUM/HIGH preserve it.
  The high-detail random doubling belongs only to flesh, never these materials.
- CGibFX starts inside 80% of the source dimensions. Glass uses symmetric Y displacement;
  wood/metal use -half-height to full height. The near-cardinal basis and per-axis random
  velocity draw order are retained. Glass's forward launch maximum is twice the speed;
  wood/metal use four times. Zero-length normalization has the existing finite adaptation.
- Patched retail B2_LITE selects `models/gibs/{material}/gib1..3.abc`, matching DTX skins
  and `sounds/gibs/{material}/gib_impact1..3.wav`. Wood and metal use the authored uniform
  scale; glass randomizes each scale axis in 0.5..1.25 before multiplying. Glass alpha is 0.4
  and uses the existing translucent model path on both CPU and GPU skinning.
- A complete custom model/skin pair uses original custom behavior: no material selection or
  randomized/authored scale multiplication (scale remains 1). Optional custom sound is used
  verbatim. Incomplete custom resources are explicitly refused, not silently replaced.
- All three start with bounce count 1. Shared client motion integrates position before gravity,
  samples liquid at creation, uses finite primary-world segments, reflects with 0.35 normal /
  0.20 liquid damping and decrements bounces only for upward-facing contacts. Preserve the
  original InitMovingObject clearing HALFGRAVITY/STICKY and the shared low-speed rest rule.
  Impacts play independent world-owned one-shots at radius 750; these debris never emit blood.
- Resting fragments have no arbitrary timeout. Original oldest-slot replacement starts a
  150-update fade, freezes motion and applies `1 - counter / 150` (including the original
  glass-opacity increase at the first fade update). No catch-up loop; updates are at least
  0.001 s apart. Exceeding the terminating pool removes its oldest member.

## Portable boundaries and ownership

`FleshGibs` shares the existing CGibFX simulation implementation, but the server adapter owns
separate flesh and destruction instances with independent cosmetic RNG. Destruction uses
80 live slots and 50 fading slots, matching the original list capacities **in a separate pool**;
the original shared competition with blood trails/splats is not reproduced. Flesh's prior
capacity/lifetime stays unchanged. New IDs are monotone across clear, reports retain at most
64 entries, source names/custom paths and numeric requests are bounded. The source SMALL flag
has no scale branch in this CGibFX implementation and remains inert.

Fragments own shared decoded models and copied resource paths, never a destroyed C++ object
or HOBJECT. Model/skin failures are retained with paths and any partial creation count. Runtime
motion/audio errors remain inspectable; cosmetic failure does not stop campaign simulation.
Other CDebris materials are explicitly reported as unsupported by this lot. The source's
out-of-world point classification is still unavailable; finite segment collision retains the
existing replacement boundary. Moving world models and ordinary objects do not block fragments.

The adapter clears fragments/reports on world reset, including load/NEW_LEVEL. These are client
cosmetic models, not new saved server classes. No schema or save format changes. The frontend
feeds the existing DETAIL LEVEL preference to future destruction requests. Inspection exposes
requests, failures, paths, poses, velocity, rest/fade state, alpha and impacts without ticking.
Rendering uses existing model shaders and alpha ordering; no PBR, refraction or new lighting.

## Remaining work

Stone/plastic/terrain/flesh/liquid/energy CDebris requests, recursive generic gib templates,
additional destruction explosion components, bullet surface fragments, weapon casings and
muzzle effects are outside this lot. Existing flesh/blood effects remain supported on their
previous callers. Full retail visual parity and campaign traversal are not claimed; M2 stays
closed with M2-GPU-001 and milestone acceptance is unchanged.
