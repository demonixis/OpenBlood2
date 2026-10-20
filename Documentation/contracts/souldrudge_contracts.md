# Original SoulDrudge

Selected original `ObjectDLL/SoulDrudge.cpp`, its three melee weapons
(`GameWeapons.cpp` CSoulCrowbar/CSoulAxe/CSoulPipe, `WeaponDefs.cpp`) and their
`AI_Mgr::Fire` aiming branch. Retail worlds author 119 SoulDrudges in 11 worlds
(05_CDM 4, 02_CityHubB 10).

## Construction

BEGIN_CLASS declares RandomHitPoints 50, AIState IDLE and hidden Dims (21, 36, 18).
The constructor sets hearing and seeing 1000, sensing 100, smelling 0, walk 60,
run 180, roll 3.5, strength 5, FLAG_NOAMMOCOLLECT (0x200) only, non-Cabal faction,
120 hit points and no armor, and draws `IntRandom(1, 3)` for SOUL_CROWBAR, SOUL_AXE
or SOUL_PIPE. PostPropRead then draws `IntRandom(1, 3)` again for SoulDrudge,
SoulDrudge2 or SoulDrudge3 skins on `SoulDrudge.abc` (the weapon draw comes
first). The port takes the weapon draw at the start of a fresh precreate, before any
property, which keeps the order; a save/load restore draws neither (lot 11).
Health is `(120 + Random(1, RandomHitPoints)) × difficulty` with no Cabal
0.5 damage multiplier. The class shares one animation/sound table (root
`sounds\enemies\souldrudge`), an eighth slot that the destination rehearsal swaps
with the others.

## Decisions and attacks

ComputeState: with no stimulus, Idle/SearchVisual/SearchSmell go Idle and every
other state SearchVisual; otherwise SIGHT > 0.5 selects AttackClose, else
AttackFar. Both attacks keep the source sanity check (no target: SearchVisual
when within the body height, else ComputeState), face the target, and within 75
units (or while already in MC_FIRE_STAND) play the standing attack. Otherwise they
face again, step back one command and run (close) or walk (far) unless a ledge or
the height forbids it, then idle. The third command recomputes the state.

MC_Fire_Stand is the source override: a random non-looping
`m_nAnim_FIRE_STAND[IntRandom(4, 6)]` clip, advancing when it finishes, with no
reload-timed Fire. A blocked resize selects CrawlUnder (SCHLEGZ "crawl out");
missing clips or services stay failures. Hits come from the clip's `fire_key`
model keys.

## Weapons

The three definitions are copied unchanged (TYPE_MELEE, AMMO_NONE, 75 range,
crowbar/pipe 10–20, axe 20–30, reload 1.0, knife fire sounds, third-person
crowbar/fireaxe/pipe pickups models). Each Fire first draws `IntRandom(0, 4)` for
the alternate slash sound (Slash1–4), even for primary fire. The original
`AI_Mgr::Fire` branch fires them from the body position along the cached forward
vector, not from `r_gun`: the swing node can lie inside the target's box, where a
segment never hits that object (later LithTech only tests inside starts with
CHECK_FROM_POINT_INSIDE_OBJECTS, which Blood2 never sets).

Caching and the unused per-class m_fInternalState table are not ported; save/load is lot 11's.
