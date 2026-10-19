# Original Voodoo weapon and Focus

Selective source port of ObjectDLL/GameWeapons.cpp CVoodooDoll, ClientShellDLL/
GameWeapons.cpp, WeaponPickups, WeaponDefs and InventoryMgr. See the
[source ledger](../implementation/sources/demo_playable_sources.json),
[demo contract](demo_contracts.md) and [weapon contracts](weapon_contracts.md).
This extends both editions' available original rules; demo weapon restrictions
still apply independently. No source tree, asset or graphics-stack change.

## Pickup, view and typed shot

VoodooDollPU grants weapon 14 and zero ammunition, with the source pickup/held/view
models, skins, status icons and resource ID 360. Focus is ammunition type 9.
The unmodified source definition costs 5 primary / 100 alternate, draws 10..20 /
40..60 damage, and uses .33 / 1 second reload intervals and the original sounds.
While WS_REST, the client chooses variant 0..4 and fire, fire_lleg, fire_larm,
fire_rarm or fire_rleg respectively. The real model FIRE_KEY sends the selected
variant in owned `WeaponFire::voodoo_hit`; out-of-range data is rejected before
ammo consumption. The common view animation/held model/audio services are reused.

Primary selects the closest visible CBaseCharacter strictly below 3,000 units
with forward dot >= .9; alternate visits all visible characters <=3,000 with dot
>= .65. Candidates follow the original destructible-list order and null-handle
terminator. Owner position and GetPlayerForward-equivalent pitch/yaw form the ray,
which extends 15 units beyond the target and excludes nonsolids/liquid volumes.
The hit must identify that target. No generic muzzle/eye ray replaces this rule.
The alternate performs the original additional server damage draw.

Variants use Normal, Normal, Blind, DropWeapon, Slow, and alternate Vision. Variant
1 doubles damage; variant 4 clears target velocity after damage/FX. Primary miss
hurts the owner by damage/3 including the inventory multiplier. Alternate miss
sends 100 Explode damage directly, without that multiplier. Original base damage
processing decides health/armor effects. The source's uninitialized primary miss
direction becomes explicit zero, and zero-distance normalization is rejected.

Solo has no other local player to blind/slow or apply wonky vision to. Those
player-only multiplayer messages are not implemented by this scope. The original
arm-hit holster action is itself an empty comment; no NPC disarm is invented.

## Effects and lifetime

ObjectFX 12..17 retain the original high-detail Voodoo recipes: red/red/blue/white/
green/purple lensflare particles, duration .75, ramp .25, delay .1, radius 1000,
position radius 20, gravity -150, local-up offset 5 for recipe12 / 20 otherwise,
count12 / 8, density10, speed25, life .5..1.5 and Y rotation .025 per update.
EMITCYLINDER spans twice the parent-to-system offset. It preserves cached-right
emission before rotating the next direction, the constant-speed RNG draw, shared
color interpolation and existing bounded CPU/GPU particle services. Portable
owned RNG and a stable vertical alignment basis are adaptations, not CRT/retail
bitwise parity. No new rendering pipeline or dependency is added.

Snapshot handles before damage callbacks; resolve them again before every use.
Pending targets cannot receive new particles, and removed owners stop later
attacks. Particle capacity, pose, visibility and damage-delivery failures remain
explicit. Effects terminate through checked owner/session lifetime. Retained
shot/target records are historical diagnostics, never owning pointers. The existing
Weapons inspector displays the next variant, last effect/damage type, delivery
count, checked historical targets and failure reason; its ammunition table exposes
Focus and its owned maximum. DropWeapon
and Vision IDs also pass typed damage/save validation.

## Focus clock

The inventory's original MID_UPDATE aggregate callback regenerates Focus exactly
once per character simulation update while alive. Magic columns 1..6 produce
2/4/6/8/10/12 units per second, capped at 100/200/300/400/500/600. Supported inventory
items have no continuous Focus burn. Death suppresses regeneration; the application's
pause skips the callback entirely. Inventory revision changes publish updated
stats. NEW_LEVEL keeps the existing saved amount and magic attribute; a fresh
Enter retry resets them with the original initial profile.

## Validation

The demo weapon test checks real resource loading, pickup grant, primary variants,
closest/all-target selection, obstruction, dry and invalid fire, consumption,
source self damage, decoded sound, cylinder counts/ramp/lifetime, and real FIRE_KEY
combat against a source Fanatic. Rule tests check one-update rates, cap and death.
The complete demo transfer/elevator test checks inventory/Focus preservation.
These are deterministic diagnostic setups; they do not prove a human walkthrough,
multiplayer behavior, complete world damage coverage or retail pixel parity.
