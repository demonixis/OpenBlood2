# Original explosive weapon effects — lot 7

2026-10-25. [Source hashes](../implementation/sources/explosive_effect_sources.json),
[verification](../implementation/reports/explosive_effects.md). Reuses the existing
explosion model/sprite/ring/fragment/light stores and owner-bound particle service.

## High-detail impacts

All normals are normalized at recipe selection. Model defaults remain the original
explosion sphere/skin unless specified; model fade and random rotation are 1.
Sprite fades are 1 and alignment is 0. Geometry illumination is still separate.

| Recipe | Model (scale, duration, alpha, waveform) | Sprites | Rings / fragments |
| --- | --- | --- | --- |
| 5, Napalm alternate | Offset 10, Exp_sphere_2 / Explosion skin, 25→250, 1 s, .5, 1 | Offset 20: fireball2 .4→.4, 1.5 s; offset 25: smokepuff2 .5→2.5, 5 s, alpha .85, waveform 1 | None |
| 9, Howitzer primary | Offset 10, 50→400, .5 s, .75, 1 | Offset 25: howitzerexp .5→.75, 1.5 s; same smoke as 5 | Two lightn32 rings: radius 4000/5000, delay .15/.30 s. Metal fragments described below. |
| 10, Howitzer alternate | Same model as 9 | howitzerexp .5→independent random X/Y .6–.9; same offset/duration/smoke as 9 | No cosmetic rings/fragments; damaging child projectiles already exist |
| 11, Howitzer mini | Offset 15, 10→150, .5 s, .75, random waveform 0–2 | Offset 15: howitzerexp .15→.5, 1.5 s | Offset 10: 12 Smoke64_4 particles, radius 8000, position radius 5, velocity 60, 1.5 s, alpha .65 |

Howitzer primary rings use offset 15, color (1,1,.5), position radius 10, velocity
300, zero gravity, 32 particles, .5 s duration, alpha 1, fade/align 1. Source increments
radius/delay *before* each publication; no initial radius-3000/delay-zero ring is added.
Mini smoke ring uses white, zero gravity and fade/align 1.

Primary fragments use gibmetal2/gibmetal3, scale (.75,.75,1), maximum rotation (1,1,1),
bounce .25, gravity 1, duration 10 s, fade time 2 s, initial alpha 1 and smoke trail 1.
The loop redraws its inclusive 5–10 bound each iteration; spread is 40–80 and speed
20–40. Existing bounded collision, detached ownership, trail budgets and resource
failure reporting apply. Gameplay fragments are not substituted with cosmetic ones.

Light states use offset 10, orange→white and radius→0. Recipe 5 starts at 300,
9/10 at 250 (all 1.25 s); 11 starts at 150 over 1.75 s. Scorch states last 25 s,
colors .2→.5 gray, radius 100→0 (5/9/10) or 75→0 (11). Preserve the duplicate
Radius1 assignment in the source; it does not set Radius2. None of these lights
illuminates geometry yet. Absent component families return successful empty results.

## Smoke connections

- General owner publication now accepts existing SMOKETRAIL_1 (1): the M16 grenade
  and Howitzer damaging fragments publish it, in addition to detached cosmetic trails.
  It keeps original location emission and distance/density sampling, not a fixed count
  of 12 particles per frame. Source requested alpha .25 stays unused without a ramp.
- Napalm primary and alternate publish SMOKETRAIL_2 (2): comet tails, radius 1200,
  position radius 12.5, gravity 25, delay .05, eight particles per distance sample,
  density 3.5, gray 96–192, zero speed, lifetime .15–.35 proportional to radial distance,
  smoke64_2 and ALL flags. Original cached axes precede random rotations; alignment
  follows the owner while the particle-system transform stays at its creation origin.
- Stop freezes emission position and sets strict expiry to elapsed + max lifetime;
  neither smoke recipe has an alpha ramp. Empty travel emits nothing and consumes no
  particle RNG. The original unused division by zero remains skipped. Owner removal
  immediately clears its effect; publication/memory budgets stay explicit.
- Grenade, Napalm and Howitzer fragments republish one trail on restore, without new
  saved bytes. Missing textures remain inspectable cosmetic failures. Owner-light
  omissions remain visible in the inspector.

Existing Howitzer rays/child damage, eight Napalm fireballs, grenade/bomb collision
and timers, primary Napalm's three-model impact and fragment damage are unchanged.
Portable cosmetic RNG, replacement collision/rasterization and high-detail-only scope
remain explicit. This does not reopen M2 or certify M3 campaign completion.
