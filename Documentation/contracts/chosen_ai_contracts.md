# Gabriella and Ophelia original AI

The separately requested complete-level continuation integrates GabriellaAI,
GabriellaREV, OpheliaAI and OpheliaREV. `ChosenAI` shares identical original
methods; the four registered classes retain separate original animation/sound
tables, skins and variant branches. See the
[source ledger](../implementation/sources/all_levels_chosen_ai_sources.json).
M2 closure and its GPU reservation remain unchanged.

## Profiles and ownership

Preserve original PASSIVE defaults, Cabal membership, hearing/seeing 1500,
sensing 200, zero smell, speeds 75/180/105/400, strength 5, mass 250 and flags
CRAWL/JUMP/DODGE. Gabriella starts with 600 HP/armor and Minigun/Napalm;
Ophelia with 400 HP/armor and Tesla/Sniper. Original ammo arrays, reversed
Random(1,0) HP request and common difficulty factors remain. These classes do
not receive the Cultist-only half damage multiplier. Authored properties can
select NONE weapons, as in Arena. Tables are independent for each of the four
classes and use the common world-owned snapshot/reset lifecycle.

## Decisions and combat

ComputeState retains strict health thresholds .15/.5, sight threshold .5,
height comparison and the missing break after EscapeRun's Idle transition:
SearchVisual follows it, with Idle retained as the previous state.

Close/far attack phases preserve .33/.75 seeing-distance thresholds, injured
walking, crawling, ledge tests and standing/crouched fire. Ophelia always crouches
with the sniper and returns Idle without a weapon. Ordinary variants may finish
standing fire after losing their target; revenants leave immediately. Common
Fire ignores its supplied alternate argument: stationary owner and target
(speeds <=5) choose alternate when ammo permits, except Assault/Napalm/Sniper.
Missing target velocity selects primary rather than using uninitialized memory.
Shots use the actual existing weapon, damage, projectile, audio and FX paths.

MC_BestWeapon is virtual as in original AI_Mgr. Empty first-weapon ammo starts
a switch animation; completion selects Napalm or Sniper, hides its hand model,
and continues through a second switch clip before advancing the phase. Empty
second-weapon ammo selects FindAmmo. OpheliaREV's null-weapon branch returns Idle;
the other variants' initial null-weapon branch makes no progress, as authored.
A missing weapon during an active switch is an explicit service failure.

EscapeRun and EscapeHide retain their different ExitHint filters and fallback
states. Dodge retains roll priority and the otherwise separate left/right
sidestep methods: commands 21/22, speed 105, a one-second clock independent of
clip completion, and no velocity stop at completion. Original empty Special1..8
methods remain empty. Shared AI lifecycle, scripts, damage/recoil/death and
model-key paths remain checked rather than bypassed by registration.

## Inspection and limits

The inspector exposes attack metrics, selected alternate count, switches and
sidesteps even for hidden/inactive actors. Invalid owners, poses, animation
columns and Metacmd overflow fail explicitly. Source return-value omissions
are replaced by checked service errors. Existing original invalid-ordinal
retention remains applicable; it does not fabricate a missing animation.

The real-world fixture checks all seven authored instances in Rooftop, Arena,
Ancient One and End. Other classes still prevent complete entry into these
worlds. Focused behavioral and rendering probes do not certify their complete
encounters, campaign traversal or multiplayer.
