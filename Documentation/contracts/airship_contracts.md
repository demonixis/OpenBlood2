# Airship entry, sniper rifle and Anger

This continuation addresses the 06_Airship1 and 06_Airship2 initialization failures.
It extends the [weapon](weapon_contracts.md), [AI fire](ai_fire_contracts.md),
[timed power](powerup_contracts.md) and [initial placement](contact_overlap_contracts.md)
contracts. M2 scope and the existing missing-BSP omission policy are unchanged.

## Sniper rifle

The full original WEAP_SNIPERRIFLE definition (ID 5) uses BMG ammunition, one round,
100–125 base damage, no spread, 10,000-unit range and one-second reload. It uses
the existing vector fire, owned held/view models, original rifle animation column,
impact, damage and audio paths. AI_Mgr's original five-byte `SNIPE` prefix resolves
to the rifle; Fanatic aiming uses the original default r_gun branch. The existing
SniperRiflePU offer now gives a usable rifle and ten BMG rounds, including drops
and bounded SpawnObject creation. Chrome/environment mapping remains unrendered.

Alternate fire is a zoom command, not a weapon shot: its original definition has
zero alternate vectors/ammunition. A typed press-edge command calls the original
PlayerObj COMMAND_ALTFIRE branch, plays the unretained attached zoom sound and
selects cursor 99 through the existing zoom/FOV path. Active binoculars prevent
it from toggling their zoom off. Dead, movement-blocked, cinematic and seeing-eye
states reject the command. ViewWeapon excludes alternate zoom from firing
animation selection; direct alternate WeaponFire requests return Blocked without
ammunition use. Switching weapons and death retain their existing zoom resets.
Sound failure stays on `zoom_sound_error`, independently of zoom state.

## Anger

AngerPU derives EnhancementBase, with the original Anger_pu model, name resource
315 and POWERUP_ANGER value 7 for 30 seconds. PlayerObj multiplies the inventory
factor by three only on first activation; subsequent pickups replace the deadline
without stacking damage. Activation retains the original power sound stack,
powerup voice and USRFLG_ANGERGLOW bit 8. Expiry is strict `time > deadline`, after
invisibility, and divides the factor by three. ResetPowerups/death also divides
exactly once. Console expiry text remains explicitly unavailable; the shared sound
stack executes the existing start/loop/end paths.

Preserve the source time quirk. Stealth's last-five-second blink calculation overwrites
the local time used by the following Anger expiry check, so it can defer expiry.
The original Player Save writes power activation, expiry offsets and sound-stack
entries (PlayerObj.cpp:6588–6601). [Save/load lot 5](save_load_contracts.md#implemented-player-and-client-lot-5) ports that block, so active
Anger now travels with its expiry and the inventory multiplier; the capture/restore
refusal described here is superseded.
Capture after reset/expiry works. The existing Stealth/Willpower snapshot omissions
are partial-port limitations, not original Save behavior; this change does not
extend their serialization.

## Initial collision placement

The existing primary-world separation remains atomic and restricted to initial
placement. Each attempt is limited to eight face corrections. Accumulated horizontal
travel is bounded by the smaller horizontal half extent plus 0.01; accumulated
vertical travel by the vertical half extent plus 0.01. These independent budgets
admit a ceiling and its sloped trim, and include the 0.001 clearance per correction.
The final box must be clear. A failed attempt changes neither position nor counters.

If the first contact is centered on a thin face (penetration equals the box's
projected radius within 0.001), the two separating directions tie. Only after the
first attempt fails, retry the opposite initial normal with the same budgets.
No arbitrary opposite-side search is permitted for off-center contacts. This
handles the Airship2 actor centered on polygon 6792, whose first side is obstructed
by polygon 9177. Source coordinates and body dimensions remain intact in the DAT.

For AI ground placement, an initial overlap with another character can try four
horizontal face-adjacent positions around that character, shortest first. The
maximum travel is the larger combined horizontal half extent plus 0.01. Require
both final scene clearance and a swept body route clear of all geometry except
the initially overlapping character. Do not move the older actor, shrink bodies,
or accept unresolved placement. This separates the two authored Fanatic3 entries
in Airship2. Corrections remain inspectable through placement_nudge/placement_nudges.

Ground rays skip enclosing object boxes, matching the existing entry-face segment
policy and later LithTech's opt-in CHECK_FROM_POINT_INSIDE_OBJECTS behavior.
Ordinary movement, world-model/container validation and explicit startup admission
remain unchanged. These bounded placement rules are replacement physics, not a
claim of retail collision parity.

Real-clock frames can be shorter than float position precision. Physics integration
retains velocity but skips MoveObject when all three destination coordinates round
back to the current position, including solid bodies. This prevents a no-motion
first frame from becoming a placement query against an authored face that the next
representable descent can leave. Explicit MoveObject queries and every representable
move retain their strict collision checks; no penetration is accepted for movement.

## Regression coverage

`airship.06_airship1` and `airship.06_airship2` exercise the inspector's actual
start_new_game transaction, all three difficulties, complete source accounting,
corrected boxes, player input, three seconds of simulation and reload/teardown.
Initial zero/sub-position frame deltas cover the first-frame graphical failure.
`airship.weapons` checks real sniper pickup, damage/ammunition/audio, Fanatic fire,
zoom press/hold/release, binocular/camera gates and triple damage delivery.
`airship.anger` checks pickup, refresh, strict expiry, the Stealth interaction,
active-capture refusal, reset/capture/restore and death cleanup. These tests require
externally mounted data.
`services.start_departure` additionally checks clear and blocked centered-face
retries, atomic refusal beyond the displacement budget and unchanged launch velocity.
They do not establish complete campaign traversal or completed boss encounters.
