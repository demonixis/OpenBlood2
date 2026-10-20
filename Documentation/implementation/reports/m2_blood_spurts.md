# M2 — Original blood spurts and debris impacts

Vector weapons now request the original entry/exit blood jets on flesh impacts.
Client flesh fragments emit blood at launch and leave larger impact marks;
server limbs request a smaller jet on the update following a qualifying collision.
The selected original `CParticleExplosionFX` recipe emits real blood-drop particles,
plays impact audio and leaves the existing surface-oriented blood sprites.
**M2 remains incomplete.** Implementation commit: `a52344a`.

Read the [contracts](../../contracts/blood_spurt_contracts.md),
[source ledger](../sources/m2_blood_spurt_sources.json) and
[validation ledger](evidence/m2-blood-spurts.json). The ledger includes the exact
implementation patch, file hashes, commands, development failures and sanitized logs.

## Preservation and implementation

- Weapon selection preserves the flesh-only random draw before testing raw damage
  >25. It retains the two original velocity ranges, independent lifetimes/fades,
  common texture and immediate updates. Damage multipliers affect combat separately.
  Selection uses the existing per-weapon cosmetic RNG as an explicit adaptation:
  newly connected visuals must not alter the established AI/damage random stream.
- Fragments preserve launch velocity*.6 and their .04–.24 impact-splat scale.
  Limbs retain the qualified-contact flag, next-update publication and server/client
  velocity scaling (.1 then .6). Source removal leaves these client effects alive.
- Particle explosions retain six samples/two drops, emission before emitter motion,
  fixed color128/255, independent particle gravity, half-gravity emitter, first-impact
  stop and strict fade/end boundaries. Impact marks use scale0–.1 and original
  normal/normal alignment. Particles grow marks at normal.y >.5; fragments use >=.5.
- Original impact sound selection includes gib_impact0.wav. Missing audio and splat
  failures are recorded separately and cannot cancel damage or dead-actor removal.
- Per-session owned values and monotonic IDs replace packet/client handles.
  Budgets and invalid clocks/resources fail visibly. World unload clears effects;
  the inspector reads pose, particles, resources, fades, request IDs and terminal
  history without advancing simulation. Weapon, fragment and limb panels expose
  their originating requests. No shaders, pinned dependencies or references changed.

## Verification

Three configurations build the application and affected tests: Debug with ImGui,
Release without ImGui, and Clang ASan/UBSan with leak detection. The main 58-case
suite covers new formulas, lifetime/failure boundaries, weapon recipes, real NPC
weapon/damage/debris callers, existing pools/deaths/corpses, object lifetime and
safe/failed level transitions. Seven additional input/combat cases cover
keyboard/mouse gates and existing authored encounter/corpse-clearance regressions.
The final combined suite has 65 cases, including 17 using external game data.

| Configuration | Headless | NVIDIA GPU |
| --- | ---: | ---: |
| Debug with inspector | 65 / 65 | 8 / 8 |
| Release without inspector | 65 / 65 | 8 / 8 |
| Clang ASan/UBSan/leak detection | 65 / 65 | Not enabled |

No sanitizer diagnostic or Vulkan validation error was reported. Existing authored
combat tests are unchanged: primary/secondary mouse fire kills actual Cultists,
and movement passes their independent corpses, including two successive encounters.
A development run initially perturbed AI randomness; the explicit cosmetic-stream
adaptation above fixes that regression while retaining every acceptance assertion.


The private blood test uses actual original Cultists, Beretta, limb code and game
assets on an explicitly diagnostic floor. It checks raw visual damage despite a
combat multiplier, deferred limb publication, detached effects after source death,
three real drop textures and three unload/reload cycles. Peak particles are 8580
in the first cycle and 8664 in the next two; each produces three emitter collisions
and four live splats in this deterministic fixture. These are diagnostic results,
not a claim of full campaign traversal.

GPU checks explicitly select NVIDIA via
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json`; logs confirm
**NVIDIA GeForce RTX 3060 Laptop GPU**, Vulkan. They cover three blood-drop variants,
actual collision-created marks, source-independent rendering and repeated unload,
existing splat tint/alpha, burning/dismemberment, safe NEW_LEVEL and mouse-combat/
corpse-clearance regressions. Captures stay in ignored build directories because
private runs render commercial assets. No native Metal run was performed here.

## Limits and remaining work

This is the selected flesh recipe, not the complete generic particle-explosion
system. Separate flesh SparksFX, generic/recursive debris, outside-world point
classification, original shared SFX-list eviction and full save/restore remain
unavailable. Disabled arterial-spurt callers stay disabled. Portable random streams,
discrete ray/particle integration and the existing fullbright alpha renderer are
explicit adaptations; retail executable parity is not established. The actual
M2 exit criteria remain unchanged.
