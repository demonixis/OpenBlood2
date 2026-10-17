# Original Gideon initialization and boss-health delivery

> Extended by [activation visibility contracts](activation_visibility_contracts.md): replacement player/camera pings now reuse the original glass/volume object filter, with atomic refresh, checked inspection and authored Gideon intro acceptance. This is not retail PVS; M2 remains incomplete.


Gideon is now a registered original NPC with the existing AI_Update scheduler,
combat actions and four supported weapons. This extends the
[attack contracts](gideon_attack_contracts.md), [NPC runtime](ai_runtime_contracts.md)
and [damage contracts](ai_hit_contracts.md). M2 remains incomplete.
See [source ledger](../implementation/sources/m2_gideon_runtime_sources.json) and
[execution evidence](../../../Documentation/Implementation/m2_gideon_runtime.md).

## Initialization and world ownership

The production factory applies the original 4000 HP / 200 armor profile, strength
10 (stored without the player upper cap), hearing/smell zero, sight/sense 10000, walk/run 75/180, roll 3.5, ALWAYSRECOIL,
non-Cabal faction and disabled damage impulses. Weapons are shield, energy blast,
Naga eye beam and wind, in source order. Original base difficulty/random-health,
ammo, focus, scale and scheduler rules still apply; RandomHitPoints=0 retains the
existing reversed-endpoint Random(1,0) adaptation. Fixed external resources are
Models/Enemies/gideon.abc and Skins/Enemies/gideon.dtx. Enhanced is an original
registered property with no consumer in the selected source. No alternative skin,
weapon or model is substituted when initialization fails. Save restoration remains
rejected before initialization; this does not implement Gideon's Save/Load fields.

Skip the base world-ground placement because Gideon can fly, then clear gravity
AFTER base initialization. Other base flags, physical body and scheduling remain.
The fifth shared animation/sound table uses sounds/enemies/Gideon and initializes
once per world, independent of Cultist/Civilian tables. A failed table preflight
cannot poison another instance. Source tables are retained and all five owners are
exchanged during disposable destination initialization, then restored after its
objects/tables are destroyed under the destination adapter. No destination tick,
world promotion or NEW_LEVEL acceptance is added.

Diagnostic boss fixtures retain an explicit DiagnosticGideon subclass and no
production initialization. They must not shadow the real Gideon class.

## Health messages and source ordering

Gideon captures m_fFullHealth before base AI initialization changes HP/difficulty.
The original CDestructable constructor's health is **1**. Do not silently normalize
that denominator to 4000: a fresh actor can broadcast ratios above 1. While
m_bCreateHealth is set, each scheduled update publishes HP/fullHealth BEFORE
AI_Update. The nonzero-denominator branch does not clear that flag. The zero branch
publishes 1, clears the flag, then captures current HP. No client does not defer or
invent a delivery; that update still follows the source flag/capture rules.

BossHealth is an owned typed float in the existing bounded local client queue.
Nonfinite health operands or ratios fail, finite ratios outside [0,1] remain unchanged. No audience is
DE_NOTFOUND and is counted separately from queued delivery. Capacity failure is
explicit and quiesces the production NPC through its existing runtime failure path.
The client retains the raw ratio and, like SMSG_BOSSHEALTH, displays a bar only for
0 < ratio < 1. Entry/unload reset this client state. Sender removal does not erase
already owned packets; client detach clears the queue. No sender handle crosses
this boundary. Only the supported single local client is implemented.

The inspector displays captured/current HP, raw ratio, create-health flag, request,
queued/no-audience counts, delivery errors and damage-special count, alongside the
actual AI state. The Session panel displays received count, original visibility
rule and a plain diagnostic progress bar. This is not the original HUD artwork.

## Damage adaptation

Original Gideon::ObjectMessageFn calls CBaseCharacter, publishes health, selects
Special1 when accumulated post-armor damage is strictly >20, then calls AI_Mgr.
That base damage branch calls CBaseCharacter again with the SAME reader. Original
engineobjects.c passes that reader to aggregates without ResetRead; Destructable
consumes vector/float/byte/object/vector. The retail exhausted-reader behavior is
not established by these sources. Replaying our immutable typed Damage twice would
invent a second independent health deduction.

The port therefore applies one accepted damage packet, invokes a protected subclass
hook for Gideon's health/strict threshold, then runs the existing base HandleDamage.
This explicit safety adaptation does not claim verified retail second-pass parity.
ALWAYSRECOIL can supersede Special1; Gideon's next Recoil state returns to Special1.
The threshold observes the existing accumulator until aggregate MID_UPDATE resets
it. Refused/nested packets and removal during a damage callback do not replay the
hook, old hit metadata or base reaction. Accepted HP changes precede delivery failure;
there is no fabricated rollback. A zero/nonfinite denominator fails visibly.

Original localized damage, non-Cabal head behavior and unsupported death branches
remain inherited. Shared hit/voice ownership and single physics/animation integration
are unchanged. Complete boss encounter/death parity, remaining required weapon FX,
illumination, full HUD and safe NEW_LEVEL are not established by registration.

## Animation-name compatibility

SubwayA's actual script requests TALKING_02 while Gideon's ABC names talking_02.
The former exact-case lookup prevented the first authored dialogue animation.
Animation lookup now compares ASCII case-insensitively and retains the first
matching index, without trimming or inventing aliases. The available later-engine
reference Model::FindAnim uses stricmp (sources/lithtech/runtime/model/src/model.cpp,
841-866); this supports the policy but is not a recovered Blood II engine binary.
The production DAT/ABC pair provides the direct compatibility evidence. Resource
path lookup and node-name ambiguity handling are unchanged.

Inventory SetStrength now retains valid high NPC values and clamps only ammunition
columns to the six-entry table, as the source does. Both MaxAmmo and decreasing
strength remain bounded; the existing typed minimum of one is retained for zero
input. Player attribute validation and supported player save ranges are unchanged.
This does not authorize saving arbitrary NPC inventory through player-only state.

## Corpse placement exposed by the integrated encounter

The existing keyboard/alternate-fire test reached an ordinary CCorpse whose raw
death-animation dimensions started inside primary-world geometry. The next generic
physics step failed, aborting unrelated player input. CreateCorpse now performs a
stationary checked MoveObject after its original transform/dimensions setup, before
accepting transfer or hiding the source. Nonsolid gravity bodies already test only
primary-world geometry in that service. The check does not integrate time or invent
a depenetrated position. A refused placement removes the partial child deferred,
retains the source visibly and records the existing death/runtime error. The engine
diagnostic now includes class/name. This is validation of the existing typed setup
contract, not an implementation of retail depenetration or successful corpse creation.
A focused floor-overlap regression verifies rollback; the unchanged input actions
then pass without health, ammo, difficulty or collision bypasses.
