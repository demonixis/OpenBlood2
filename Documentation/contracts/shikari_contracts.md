# Original ShikariAI

Selected original `ObjectDLL/ShikariAI.cpp`, its claw and spit weapons
(`GameWeapons.cpp` CShikariClaw/CShikariSpit, `WeaponDefs.cpp`), the
`CShikariLoogieProjectile` and the two `AI_Mgr::Fire` aiming branches. Retail
worlds author 58 Shikari in 10 worlds (05_CDM 1).

## Construction

BEGIN_CLASS declares RandomHitPoints 50, AIState IDLE and hidden Dims (19, 30, 24).
The constructor sets hearing 2000, sensing 500, smelling 2000, seeing 1000, walk 180,
run and roll 320, jump 600, weapons SHIKARI_CLAW then SHIKARI_SPIT, FLAG_CRAWL |
FLAG_JUMP | FLAG_DODGE, non-Cabal, 175 hit points and 50 armor. Model and skin are
fixed (`Shikari.abc`/`Shikari.dtx`). Health is `(175 + Random(1, RandomHitPoints))
× difficulty`. The class shares a ninth animation/sound table (root
`sounds\enemies\shikari`).

## Decisions and attacks

ComputeState: with no stimulus, Idle/SearchVisual/SearchSmell go Idle and other
states SearchSmell. Otherwise, as in the source, the smell branch tests the
*argument* (not the computed stimulus), so only an explicit STIM_SMELL call selects
SearchSmell; SIGHT ≤ 0.66 or a height difference above the body selects AttackFar,
else AttackClose.

Both attacks face the target every update and cancel the command advance. Close:
within 75, while firing, or with the target above the body height, the standing
attack; beyond a third of the seeing distance recompute, else run. Far: within
0.75 × seeing distance (or firing, or target above) the standing attack; nearer
than a third recompute, else walk; its second command recomputes when near (and not
jumping) or with a high target, pounces when nearer than 0.4 × seeing, when
already jumping or at a ledge, and otherwise rolls forward (looping).

MC_Fire_Stand switches to the spit for AttackFar, Guard or a high target
(FIRE_STAND[0]), else to the claw with a random FIRE_STAND[4..6]; the claw is
restored when the clip ends. A blocked resize selects CrawlUnder. The pounce
(source MC_Jump override) plays JUMP[4], launches at jump speed upward and
3 × run speed forward, holds the horizontal velocity every update and ends on
landing with a velocity stop. Both dodges play `m_nAnim_DODGE_RIGHT` (source quirk)
and hop sideways at run speed, ending on landing with a zero move. Dodge selects
right, then left, from CalculateDodge and returns to the last state. Guard faces
the target, taunts once it has a non-smell stimulus, fires while within seeing
distance and recomputes when health is at most half. Special1 recomputes once
damaged, else repeats MC_Special(0) every 0.1 s.

## Weapons

Definitions are unchanged: claw TYPE_MELEE 30–60 at 100 range; spit TYPE_RIFLE,
1000 u/s, 75 range, 30–50, `CShikariLoogieProjectile`. Each claw Fire draws
`IntRandom(0, 4)`; the primary writes the alternate slash sound and the alternate
writes the primary one (source swap). The claw fires from the body along the cached
forward vector; the spit from the `head` node toward the target. The loogie is an
OT_NORMAL Acid projectile scaled 0 then 0.25, damaging what it hits directly; it
has no explosion (its placeholder EXP_FLAME_SMALL is unreachable with
m_bExplosion false) and its bug-spray trail is client-only.
