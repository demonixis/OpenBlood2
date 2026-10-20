# Original EnhancedGideon

EnhancedGideon is the original Rooftop/Ancient City boss, with its own registered
class, fixed Gideon model/GideonE skin and independent shared animation/sound table
(`sounds/enemies/EnhancedGideon`). The port shares identical source algorithms
with Gideon through C++ inheritance; the original class registration still names
AI_Mgr as its parent. This is code reuse, not an alias to ordinary Gideon.

The original profile is 9000 HP, zero armor, strength 10, mass 250, walk/run/roll
175/380/200, sight/sense 10000, zero hearing/smell, non-Cabal, ALWAYSRECOIL and no
damage impulses. The source four-weapon loadout is shield, energy blast, Naga blast
and wind. Existing original difficulty, random-health and scale rules apply.
Skip initial grounding and clear gravity after base initialization, as for Gideon.
Required resources and services still fail explicitly; save restoration is unavailable.

EnhancedGideon starts with fullHealth zero. Its first scheduled update publishes
1.0, clears createHealth and captures current difficulty-adjusted HP. It does not
inherit ordinary Gideon's pre-base denominator of 1. Damage publishes the actual
ratio once through the existing typed damage boundary, without ordinary Gideon's
extra >20-damage Special1 transition. The base recoil reaction remains unchanged.
See [Gideon lifetime/damage contracts](gideon_runtime_contracts.md).

Combat, shield selection, extra-key aiming, fade, teleport, timed dodges and
recoil share the original equivalent Gideon methods. Close attack faces the
stored target before every phase, preserving Metacmd via the source decrement;
ordinary Gideon retains its original narrower facing branch. Source powerup,
projectile and sound services are real existing implementations. Missing sounds
retain their original failure diagnostics; no replacement audio is chosen.

## Script suppression and locomotion

The Enhanced override sets noFire before selecting a script command and restores
gravity after selection, before command dispatch. DONE/default clears noFire
before the final ComputeState. SETSTATE/TARGET can leave that flag latched, just
as in the source. PLAYANIMATIONLOOPING retains the original default completion
branch. Parsing, lifetime/reentrancy checks and command ownership remain shared
with [AI scripts](ai_script_contracts.md); the small shared runner parameter keeps
these guards identical while preserving the Enhanced-specific ordering.

While noFire is set, both attack states and MC_Fire_Stand select Passive (after
the original missing-target guard for attack states). Fire returns an intentional
Blocked result, with no shot or service error; extra-key callers preserve the
source's ignored boolean. MagicPowerup does no work. The inspector exposes the
flag and suppression counts separately from successful shots/effects.

All four movement helpers select the MELEE walk/run column, independent of the
current weapon. Initial walking uses walkSpeed/3 for any injured leg, then full
walkSpeed. Initial running uses **walkSpeed**, then runSpeed. Looping playback
completion stops horizontal velocity and clears animating without incrementing
Metacmd. Script_Walk always enables gravity; MC_Walk does so only in Script.
MC_Run clears gravity only in Script; Script_Run leaves it alone. Neither inserts
the ordinary humanoid obstruction/crawl logic. The virtual script-locomotion
boundary also runs inside FOLLOWPATH/MOVETOOBJECT; public reentrancy checks remain.
Movement still writes velocity once and uses the existing replacement physics.

Shared table ownership covers world unload and disposable destination contexts.
This continuation does not establish complete boss battle/traversal parity,
network play, save support, point-light illumination or a new milestone acceptance.
