# Original Gideon combat actions

> Extended by [Gideon lifecycle contracts](gideon_runtime_contracts.md): production initialization, AI_Update, health delivery and fifth-table isolation are now integrated. Earlier blanket boss-registration omissions are historical. M2 remains incomplete.

Selected original `Gideon::ComputeState`, `MC_Fire_Stand`, `Fire`, `MC_Extra`
and `MagicPowerup` now use the typed engine services. See the
[source ledger](../implementation/sources/m2_gideon_combat_sources.json) and
[execution evidence](../../../Documentation/Implementation/m2_gideon_combat.md).
Read the [AI state](ai_state_contracts.md), [weapon](ai_fire_contracts.md),
[shield](shield_weapon_contracts.md) and [lifetime](object_contracts.md) contracts.

**This is a combat-method dependency, not a registered production boss.** Tests
register their own explicitly diagnostic Gideon with an injected inventory/table;
normal worlds still report Gideon unsupported. Full original initialization,
shared boss table, boss-health messages, damage dispatch, dodge/attack sequencing,
fade/teleport and save/load remain pending. No additional mutable game global is
introduced. Destination rehearsal still exchanges the existing four class/gender
tables; a later production boss table must join that ownership audit.

## Decisions

No stimulus selects Special1 from Idle, SearchVisual from Teleport, Idle from
SearchVisual and Teleport otherwise. With a stimulus, health<.25 or threat>.75
first considers the strict `now-lastShield > shieldDuration` gate and selects
AttackClose when the shield is available. Otherwise health<.05 can select Teleport
only when `lastPanic+15 >= now`, updating lastPanic. This reversed-looking panic
inequality is original and is retained. The other low-health/threat cases select
AttackClose. SearchVisual with an ordinary stimulus selects Special1; otherwise
sight>.94 selects close, sight>.75 selects far, and the remainder selects Special1.

All transitions use the existing original SetNewState behavior, including its
Idle deadline and velocity policy. A selected state does not imply that its action
is implemented. Invalid/nonfinite input, stale owner and failed ComputeStimuli
reject; a failed query is never interpreted as a usable stimulus.

## Standing fire and armor

A new fire command checks armor<=25 and a strictly expired 60-second shield
interval. That branch selects Shield, adds exactly 100 armor without a cap, records
the current time and selects FIRE_STAND[TYPE_RIFLE=0]. It consumes no chance draw.
Otherwise AttackFar consumes one original 0..99 chance draw: <70 selects Energy
Blast and FIRE_STAND[TYPE_MAGIC=8], even though the weapon definition's fire type is
MELEE. The other far branch selects Naga/RIFLE. Other states select Wind/RIFLE for
<50 and Naga/RIFLE otherwise. The portable generator is not retail CRT bit parity.

The animation is nonlooping. Continuing this command never fires on a reload
clock: Gideon's original override fires exclusively through model keys. Completion
tries Soul Hook (base-game ID23), clears animating and increments Metacmd once.
The original ignores the missing weapon; the typed inventory reports Invalid for
this unsupported definition, retains the current weapon and exposes the actual
result. No Soul Hook implementation or fallback weapon is fabricated.

Missing inventory/table/animation services fail explicitly. Selection and shield
armor changes precede SetAnimation in the original; they remain committed if its
checked replacement resize fails. The port does not set animating on that failure,
where the source ignored its bRet result. The shield timestamp prevents an immediate
second grant. Metacmd overflow rejects before completion mutation. Calling the
shield weapon itself still does not change armor.

## Node aiming, keys and presentation

The default fire node is `rr_gun`. A successful current node query aims toward the
stored target position. If the node is missing, use current body position and
**CMovement's cached forward**, not the current rotation or target direction.
The cache must be initialized and sampled. Validate and normalize the direction;
invalid pose, degenerate aim and pending owner/target fail. This boss override never
calls generic CheckClearShot or substitutes head_gun/r_hand_extra2. Actual weapons
retain their own collision, callbacks, ammo, audio and checked owner rules.

The original accepts inventory dry attempts. The port distinguishes fired, dry
and failed services in the shared AI inspection fields; invalid/unavailable shots
are not accepted. It uses the existing typed seeded WeaponFire transport, with
explicit virtual dispatch from the base model-key handler.

`extra_key` invokes the subclass virtual method after the existing fire/show branch
priority. A case-sensitive `powerup` prefix publishes ObjectFX36 on the checked
owner; any other value replaces the fire-node name and immediately calls Fire.
Unlike fire_key, extra_key has no no-target guard. Source `fire_key:head` does not
select a head node: its value is ignored, preserving the previously selected node.
The old unbounded scalar/string buffers are replaced by nonempty, NUL-free inputs
of at most 63 bytes. Unknown nodes still use the source cached-forward fallback.
Powerup publication failure is visible as a failed key/effect, not a successful
placeholder. Other AI subclasses retain explicit unsupported MC_Extra responses.
Base character motion keys continue after the action; fire_key's no-target early
return and pending-target rejection still stop that forwarding.

Owner-bound effects follow the existing shield/powerup lifetime. Pending owner
removal ends them, while detached beam/projectile presentation keeps its existing
ownership. Removal during a shot callback prevents later animation keys from using
the pending actor. Inspector/GPU preparation only reads current state and clocks.
The Skeleton inspector shows selected node/weapon/chance, shield interval/grants,
armor, panic time, extra keys, historical powerup ID/errors and completion selection.

## Sparse NPC weapon IDs

The original initialization path can now acquire the supported non-player weapon
IDs 27/38/41/42 safely. An 18-element array indexed by raw IDs was left over from the
player-only subset; the newer name mappings could access it out of bounds. Duplicate
suppression now stores at most the five requested IDs and compares values. A private
regression initializes three real Cultists with all four IDs and a duplicate fifth
slot, verifies four correctly ordered weapons and unloads them. This does not change
their authored defaults or give those models Gideon's node override.

SubwayA remains 203 created / 4 transferred sounds / 372 editor-only / 1 unsupported
Gideon / 0 creation failures. Required FX, production boss services, safe NEW_LEVEL
and the unchanged M2 acceptance gate remain open. No retail playthrough, other
platform or XR validation is claimed.

The [Gideon fade/teleport continuation](gideon_teleport_contracts.md) adds the original selected states, live model color/alpha, real transparent model presentation and supported player color preservation. Earlier blanket omissions are historical; production boss lifecycle and M2 remain incomplete.

The [Gideon attack continuation](gideon_attack_contracts.md) supersedes the selected attack/dodge/recoil sequencing omissions and adds original Bold taunt. Production boss lifecycle and the M2 gate remain incomplete.
