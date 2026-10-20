# Original Undead Gideon weapons and imprisonment

The complete-level continuation adds the original weapon definitions 43 (Vomit)
and 45 (Gideon spear), inventory factories and AI weapon-name prefixes. Required
commercial resources are resolved through the configured edition resource root.
This is a prerequisite for UndeadGideon, not registration of that boss or campaign
acceptance. M2 closure and its GPU reservation are unchanged.

The spear uses its original 200-unit preliminary solid-object ray with no gameplay
filter, sends MID_IMMOBILIZE for four seconds to a hit player, then runs the separate
ordinary CWeapon damage ray. Interior starts follow the existing checked engine
ray policy. Transport deliveries are counted separately from accepted effects:
a player already imprisoned, or with the original player-dead flag, ignores the
request. The default damage type stays Normal. No invented spear projectile is used.

MID_IMMOBILIZE is an owned typed float, separate from byte/key/pickup/damage payloads.
Nonfinite/negative durations, mixed payloads, absent payloads and stale endpoints
fail explicitly. The player captures the level clock once. Repeated hits do not
refresh its duration; expiration is strictly elapsed > length, including the
zero-duration boundary. The source Update order checks powers before imprisonment.
Movement and new command gates combine imprisonment with existing independent
blocking reasons. Aim remains available; gravity and existing velocity still use
the single shared player integration. Expiring imprisonment does not clear an
unrelated blocking reason.

Original Save/Load stores the active flag, start minus current time and duration;
load adds its current clock. Fields are validated before mutation. This extends
the existing typed player transfer only, not whole-world boss/projectile saves.
The original strings 265 (immobile) and 264 (can move) are resolved from cres.dll
and exposed with timing in inspection. A missing string remains a diagnostic.
The original console-message overlay is still unavailable; no HUD substitute is
invented by this loading continuation.

Vomit is the source OT_NORMAL CVomitProjectile with a ten-second lifetime, point
collision, no sliding/gravity and a scale of zero before 0.01 seconds, then 0.25.
The weapon's Fire damage type is not passed by original projectile Setup, so the
projectile retains Normal damage. Speed/range are 2000, radius zero, damage 15–40
(primary) or 10–20 (alternate). Its ObjectFX3 trail uses the original 0.25 lifetime
multiplier and actual Firepart64 texture, with owner-bound checked cleanup.
The original explosion/client-FX/shockwave flags are all false: direct impact
still damages its target and removes the projectile, without impact explosion
visuals or audio. Failed required trail resources refuse initialization.

[Source hashes](../implementation/sources/all_levels_gideon_weapons_sources.json)
and the [complete-level record](../implementation/reports/all_level_loading.md)
keep this source selection distinct from replacement engine physics and full
retail fidelity. Graphics pipelines, dependencies and commercial/reference data
are unchanged.
