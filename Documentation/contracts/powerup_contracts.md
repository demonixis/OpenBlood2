# Original timed powers (Willpower, Stealth, Anger)

Selected original `PowerupPickups.cpp` StealthPU and `CPlayerObj`
AddPowerup/CheckPowerups/ResetPowerups/Start/EndPowerupSound
(PlayerObj.cpp:3670–4046). Willpower (POWERUP_INVULNERABILITY, PU_INVULNERABLE)
and Stealth (POWERUP_STEALTH, PU_INVISIBLE) are collected and timed.
[Anger](airship_contracts.md#anger) now adds the original triple-damage lifecycle;
Revenant, incorporeal and silence remain unported and keep refusing death, capture
and restore.

## Willpower

InvulnerabilityPU (EnhancementBase, model `Invulnerability_pu`, resource 313,
30 seconds; 13 retail instances, and a 03_Museum container spawn) sets
`m_fNighInvulnerableTime = now + value`, the CDestructable nigh invulnerability
(non-Death character damage × 0.05), activates PU_INVULNERABLE, pushes its sound
(loop `invulnerabilityloop1.wav`) and sets USRFLG_WILLPOWERGLOW (1 << 9).
CheckPowerups tests it before invisibility: on expiry it clears the flag, the
nigh invulnerability and the glow, ends the sound and plays the VME_POWERUP voice
group (single player); IDS_GENERAL_WILLPOWER_EXP is not delivered (counted).
ResetPowerups (death) clears it first. Capture drops the power, but the source
CDestructable Save keeps `m_bNighInvulnerable`, so a transferred player stays
nigh invulnerable with no timer — a limitation of the current partial snapshot.

## Pickup

StealthPU derives EnhancementBase/PowerupPickup (rotating), model `Stealth_pu`,
resource 314, default `powerup4.wav` pickup sound, and offers POWERUP_STEALTH for
30 seconds. Typed pickup messages accept the original powerup range 1–8; the player
acknowledges only the types it implements. EnhancementBase's multiplayer
respawn/scaling branch is unported. Retail worlds author 14 StealthPU (05_CDM 1).

## Invisibility

AddPowerup sets `m_fInvisibleTime = now + value`, color (0,0,0) with alpha 0.08,
clears FLAG_SHADOW, activates PU_INVISIBLE and pushes its sound. CheckPowerups runs
after UpdateStats every player update. After expiry it deactivates, ends the sound
and restores alpha 1 and FLAG_SHADOW; the IDS_GENERAL_INVISIBILITY_EXP console text
is not delivered (counted). In the last five seconds the player blinks: visible
when `frac(2·time)·2 > 1`. AI perception already ignores a target whose
PU_INVISIBLE is active. Death (DoDeath) calls ResetPowerups, which ends it.

## Power sounds

The source stack (15 entries) records each activation. A retained start sound
(`specialpowerupstart1.wav`, 1000 radius, player-low) plays if none is retained;
when it finishes, or when nothing plays while the stack is non-empty, the top
active power's loop (`revenantloop1b.wav` for Stealth, player-medium) starts,
popping inactive or silent entries. The source spins forever when that loop fails
to start; here a failed start leaves the loop and retries on the next update
(counted as a loop failure). Ending a power pops it only if it is on top,
kills the loop and plays the retained end sound. The player destructor kills all
three handles.

## Save, KeepAlive and restore

The original Save writes power flags, expiry offsets and sound-stack entries
(PlayerObj.cpp:6588–6601). The partial typed snapshot omits this block and drops an active
invisibility; the engine snapshot keeps the translucent color, so a restored or
transferred player stays translucent with PU_INVISIBLE inactive (AI sees it) until
something resets its color — a partial-port limitation. Later
LithTech serializes object color (game_serialize.cpp), corroborating this.
Active Anger capture remains explicitly refused so its damage factor cannot become
permanent. Full timed-power serialization is outside the Airship entry continuation.
