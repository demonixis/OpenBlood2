# Original Gideon shield and owner-bound spell particles

The original CGideonShield weapon and high-detail ObjectFX recipes SHIELD_1 (19)
and POWERUP_1 (36) extend the existing CObjectParticleFX adaptation. This prepares
Gideon's weapon/powerup dependencies; **Gideon is still unregistered and M2 remains
incomplete**. See [source selections](M2_SHIELD_WEAPON_SOURCES.json),
[execution evidence](../../Documentation/Implementation/M2_SHIELD_WEAPON.md) and
[existing smoke-trail contracts](OBJECT_PARTICLE_CONTRACTS.md).

## Original weapon boundary

The complete base-game WeaponData entry is retained: ID 41, TYPE_RIFLE = 0, AMMO_NONE,
10–20 nominal damage, range 75, reload 1/alt 0, zero projectile velocity/radius/spread,
null projectile and held-model slots, and shield.wav in all four sound slots.
Both modes execute original CGideonShield::FireVector and request SHIELD_1 on the
checked owner. This branch does **not** trace damage, change health or grant armor.
The source boss's MC_Fire_Stand separately adds 100 armor and selects this weapon;
that boss action is still pending. Do not invent protection in the visual service.

The generic CWeapon path retains request validation, shot RNG/order, no-ammo
semantics and existing retained WAV/hearing lifecycle. The pure visual shield
branch bypasses the replacement vector collision preflight; unavailable collision
therefore does not prevent its original effect request. Collision inspection says
unavailable/not queried, not a manufactured hit. Presentation failure is separately
reported and does not revoke an accepted server shot. Hearing callback removal of
the shooter still cancels downstream publication through the existing checked path.

Original GIDEON_SHIELD prefix selection and the generic AI r_gun/body fallback
are supported. FIRE_KEY can reach the actual weapon. This is not Gideon's own
rr_gun/extra-key override or production boss AI. Player acquisition refuses the
NPC-only weapon; save/view/profile support is not broadened.

## Selected original particle recipes

| Setting | Shield 19 | Powerup 36 |
| --- | --- | --- |
| Duration / emission interval | 5s / .1s | 2.5s / .1s |
| Shape / particles per burst | sphere shell / 24 | ring / 18 |
| Position radius / sprite radius | 45 / 500 | 25 / 1750 |
| Gravity / initial speed | 0 /0 | +150 /0 |
| Lifetime | .5–.75s | .75–1s |
| Colors, source RGB | (192,192,0)–(255,255,255) | (0,0,255)–(196,196,255) |
| Offset, source UP/RIGHT/FORWARD axes | (0,0,0) | (-25,0,0) |
| Rotation per callback, multiplied by pi | random(0,.01), random(0,.015), random(0,.01) | (0,.025,0) |
| Authored alpha / last alpha-ramp duration | 1 / .25s | .5 / .25s |
| DTX | SpriteTextures/Lensflare_1.dtx | SpriteTextures/smoke64_2.dtx |

Both recipes use flags 0 and no scaling overrides. The system is initially opaque
(alpha 1), with identity rotation and the owner's creation position. The first
Update sets absolute last-add time to zero and returns before expiry, pose changes,
rotation or emission. Later callbacks emit at most one overdue burst; no catch-up
is added. Expiry is strict elapsed>duration, so the exact deadline can still emit
zero-alpha particles before removal on the next callback.

Authored alpha only takes effect during the last .25s. Powerup therefore remains
alpha 1 until elapsed 2.25, jumps to .5 at the ramp boundary, then fades to zero.
Do not silently apply .5 from creation. Source floats retain their boundary rules.

The system follows the current owner, including the powerup's rotated local-up
offset. All previous particles move with it. Owner orientation changes the offset,
not the independent system rotation. Euler X, Y, Z rotations use freshly evaluated
local axes in that order, once per effect callback (not scaled by delta time).
The local-axis implementation follows the available later LithTech engine source;
retail engine numeric parity is unverified.

Each burst starts with the current system rotation. For each particle, cache the
right/forward axes, randomly rotate around cached forward (and cached right for
the sphere), then emit using the **previously cached right**, normalized. Carry
the rotated basis into the next particle. Draw zero-speed randomness even though
both velocity endpoints are zero, then one shared RGB interpolation ratio, then
lifetime. The shield recipe first consumes three rotation draws; powerup consumes
none. Use the local particle positions literally: the system rotation also applies
when rendering. Do not flatten that second transform into guessed world positions.
The unused uninitialized non-stream `offset += dif` is omitted; it cannot affect
any selected particle. Owned seeded randomness preserves draw order, not retail
CRT/global random bits.

Existing particle stepping remains the documented later-engine replacement:
strict lifetime<0 removal, local position before gravity, positive Y acceleration,
and sprite half extent radius/640. Source absence of fades on individual particles
is preserved. Shared-depth DTX alpha/tint uses the existing sorted sprite pass;
global ordering with glass/models and lighting parity remain unverified.

## Typed lifetime and inspection

PublishObjectParticles accepts only 19/36 and a live opaque owner handle. No raw
pointer is reinterpreted as an ID. A session-owned record holds the non-owning
checked handle and an independently monotonic effect ID. The numeric parent field
inside CObjectParticleFX identifies that binding; smoke-trail parents still name
fragment IDs. There are no new mutable original globals or engine objects.

Owner position/rotation are resolved on each update after original callbacks and
fragment movement. Pending/removed owners stop their entire particle system,
including existing tails; owner removal is not a resource failure. Missing poses,
invalid clocks/resources, unsupported recipes and capacity failures are explicit.
A failed effect is removed, its error retained, and the session step reports
failure. Unload releases systems, reports and RNG; IDs are never reused. Separate
destination rehearsal owns its own arrays/RNG and cannot change source particles.

Limits: 128 owner-bound systems, 2048 particles/system, 8192 particles across those
systems, 64 historical reports. Each burst checks remaining capacity before any
particle allocation or emission RNG. The existing 16384-entry sprite pass retains
its explicit overall capacity failure; these limits do not guarantee that every
possible simultaneous FX family fits. Missing decoded DTX resources are visible
render failures, never successful placeholder quads. Fragment smoke uses its
existing separate budgets and semantics.

The in-game scene-effect tree shows owner liveness, recipe, elapsed/duration,
updates/bursts/emitted/live counts, system transform, actual versus authored alpha,
ramp state, particle positions/velocities/colors/lifetimes, resource and prepared
quads. Weapon inspection retains publication ID/error. History says created
(historical), not currently live. Rendering/inspection does not advance clocks.
Powerup publication is tested as a typed service; the original boss extra-key
caller is still pending. No authored boss fight is claimed.

SubwayA remains 203 created / 4 transferred sounds / 372 editor-only / 1 unsupported
Gideon / 0 failures. Boss initialization/actions, required missing FX, safe NEW_LEVEL
and the actual M2 exit criteria remain open.
