# Original Naga eye weapon and red laser beam

This ports `CNagaEyes::FireVector`, the complete base-game Naga weapon definition,
`NAGA_BLAST` name selection, the `head_gun` AI aiming branch and the red-small
`CLaserBeamFX` model. Gideon uses this weapon, but **Gideon itself is not registered**.
Its other weapons and state/lifecycle services remain required. M2 stays incomplete.
See [source selections](M2_NAGA_WEAPON_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_NAGA_WEAPON.md).

## Combat and two different rays

Base-game ID 38 is `WEAP_LASTPLAYERWEAPON` (17) + 21; add-on numbering is not used.
The definition retains every field: no-ammo pool, one vector, 30–45 normal damage,
2,000-unit range, primary/alternate reload 0.2/0.05 seconds, zero spread, primary
laser WAV and intentionally empty alternate WAV. `CheckAmmo` returns one for
AMMO_NONE; no pool is consumed. Existing shot sampling, damage multiplier,
[fire sound](WEAPON_AUDIO_CONTRACTS.md), [impact continuation](WEAPON_IMPACT_CONTRACTS.md)
and deferred callback guards remain in force. No new timer suppresses FIRE_KEYs.

The additional visual ray selects solid objects only and excludes the owner's
class **and derived classes** through original `IsKindOf`. Its destination is the
first remaining hit or the original full-range endpoint. It does not replace the
ordinary damage ray: that ray includes SOLID or RAYHIT objects and excludes only
the owner. A same-class actor may therefore take damage while the visible beam
continues beyond it. Baked world geometry remains eligible in both queries.
`IgnoreOwnerClass` requires a checked live owner; invalid/unavailable queries fail.

The visual beam is requested before the base vector damage. Missing beam resources
record failure without suppressing damage, as original client SFX are independent
of server damage. The additional original `EXP_NAGA_EYE_BEAM` composite is still
unavailable and explicitly reported for each shot. The source supplies its endpoint
as **both position and normal**, including on misses. No normal correction or
unrelated explosion is substituted. Success of `Fire` is not complete FX support.

Original AI `NAGA_BLAST` prefix selection acquires this actual weapon. Generic AI
Fire uses `head_gun` and the cached target position, then ordinary clearance and
FIRE_KEY delivery. Missing head_gun fails explicitly instead of using the source's
unchecked output. Gideon's own `rr_gun`/extra-key aiming override is not implemented
by this branch. The weapon's null hand-model/skin deliberately creates no child,
as in original CreateHandModel. Player acquisition refuses this NPC-only weapon;
player view/profile support is not silently broadened. Other unknown weapons still
reject and are never instantiated as the knife.

## Client model and ownership

The beam loads external `Models/Ammo/beam.abc` and `Skins/Ammo/beamred.dtx`, uses
existing normalized model coordinates and aligns local +Z from source to endpoint.
Its first Update sets the source position, fixed length and lifetime origin.
Width grows from 0.05 to 1 during 0.25 seconds; alpha stays 0.5 until 0.25 seconds,
then reaches zero at the strict 0.5-second expiry. A delayed first update does not
consume the effect lifetime. Model animation follows the existing engine clock;
rendering never advances it. No gameplay collision body or damage is attached.

Session-owned beams detach from the shooter/weapon and continue after deferred
removal. There are at most 128 active beams and 64 owned historical reports.
Successful IDs are monotonic across unloads. Failed publication has no success ID;
capacity, resource, endpoint and clock failures are explicit. Zero-length beams
reject rather than aligning a degenerate vector. Backward clocks remove the failed
instance and retain its update diagnostic. Unload destroys beams before caches.
No process-global mutable game state is added, so destination rehearsal retains
its original-global isolation contract.

The existing translucent model pass draws beams with actual DTX alpha, shared
world depth and no depth writes. Unupdated beams are hidden until their first
source update. All beam reports/clocks are inspectable independently of UI state;
per-weapon inspection exposes the distinct visual endpoint and unavailable impact
composite. Model/skin decode errors remain visible instead of a success placeholder.
The model effect pass still lacks global ordering with world glass and other FX.

## Boundaries

This is a tested weapon dependency for later Gideon integration, not a new
production Naga/Gideon actor or proof of an authored boss fight. Broader weapons,
Gideon shield/wind/projectiles, eye-impact composite, lighting, safe NEW_LEVEL and
the actual M2 gate remain open. Portable RNG, current hit shapes and rendering are
replacement-engine policies, not verified retail parity. No commercial resource
is copied into either repository.

[Energy projectiles](ENERGY_PROJECTILE_CONTRACTS.md) subsequently implement the non-Zealot energy weapon dependency. Other Gideon weapons, boss lifecycle and required FX remain unavailable.
