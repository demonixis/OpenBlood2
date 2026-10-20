# Player Locomotion Boundary

> Known traversal defect, 18 October 2026: the grounded-only stair restriction
> blocks Caleb's 96-unit planter-to-dumpster jump on CityHub's museum approach;
> the unobstructed jump rises 90 units. An isolated airborne-step experiment
> clears that obstacle without changing jump/gravity, but is not integrated or
> regression-tested. See the [diagnosis and traces](../implementation/reports/cityhub_jump.md).

> Extended by [character animation contracts](character_animation_contracts.md):
> original CAnim_Sound tables now drive locomotion; current model node queries and
> original sphere measurements are inspectable. AI/damage-localization callers
> and complete M2 remain unavailable.

> Extended by [container_contracts.md](container_contracts.md): selected original
> liquids/ladders and head/body queries are now connected. Earlier pending
> container statements describe the dry-land increment.


This extends [movement_contracts.md](movement_contracts.md). The local shared
CPlayerObj now selects original locomotion clips, resizes against collision,
crouches, and uses grounded stair stepping. M2 remains incomplete. Source ranges
and hashes are in [m2_locomotion_sources.json](../implementation/sources/m2_locomotion_sources.json); execution
results are in [m2_locomotion.md](../implementation/reports/m2_locomotion.md).

## Original game rules

`PlayerLocomotion.cpp` selects the healthy, alive, dry-land branches of
`CPlayerObj::UpdateAnim`, `IdleTime`, `Do*Anim`, and `SetAnimation` from the
original `ObjectDLL/PlayerObj.cpp`. The command priority is jump, moving crouch,
strafe, run/walk, stationary crouch, and idle; firing selects the corresponding
body clips. Selection follows commands even when a wall stops actual motion.
The selected clips loop for walking/running/crawling/strafing and idle; jump,
stationary crouch and stationary firing do not. A held jump does not continually
restart its completed clip. Standing firing also uses the original reload timer.
Body keys continue through the existing original aggregate dispatch. They do not
create extra shots: the view weapon's FIRE_KEY path remains the firing authority.

Animation names and weapon columns come from `CAnim_Sound::SetAnimationIndexes`.
Its `CheckAnims` fallback copies the previous valid column. The installed Caleb
has no `walk_pistol`, `jmp_pistol`, `crouch_1pistol` or knife locomotion clips.
Consequently walking/running/jumping/crouching/crawling select the corresponding
**rifle** clips; missing knife firing clips fall back through the **two-pistol**
column. These are original fallback rules, not guessed file aliases. The source
macro also maps left pistol/knife strafe to `strafe_right_1pistol`; that alias is
preserved and both requested and resolved names are visible.

A wholly absent chain leaves the source index invalid. The installed Ishmael and
Gabriella name their jumps `jump_rifle`/`jump_nogun`, so every `jmp_*` JUMP slot is
invalid for them. The original `DoJumpAnim` ignores `SetAnimation`'s result.
`SetAnimation` resets the current clip, and the engine ignores an out-of-range
`SetModelAnimation` (later LithTech `trk_SetCurAnim` returns without changing the
tracker). Dimensions and looping then apply to the **current** clip. The port
reproduces that: the move state changes (jump physics is unaffected), the current
clip restarts with the requested looping and keeps its body dimensions, and
`locomotion_clip_retained`/`retained_clip_requests` expose it in the Movement panel.
Clip zero is never substituted. Only a body without any valid current clip still
fails with a diagnostic. Idle choice uses an owned seeded generator, not CRT random
bit parity. Dual pistol body columns, injured/slow-death/death animations and
other weapon families are outside this increment.

MoveMgr retains source control bit `0x0004`, halves dry-land speed when crouched
and multiplies the jump impulse by **0.75** while crouched. Running still
multiplies speed by **1.5** before the crouch factor. Default speeds are therefore
280/420 standing and 140/210 crouched; jump impulses are 600/450. The local
`PlayerControls` message carries device-independent commands, firing and yaw
through the checked server shell to the actual player. Rendering and ImGui do
not participate in the game rule. Accepted yaw updates the player's rotation.

## One body, animation dimensions and clearance

The original separate client physics model (`UpdatePlayerAnimation`/`ResetDims`)
is unnecessary here: MoveMgr and CPlayerObj share one checked body. Selection
precedes motion, so dimensions, crouch speed, camera and firing agree in the same
frame. `ResizeObject` provides a typed `Resized/Blocked/Invalid/Unavailable`
result. It anchors the feet, validates finite positive dimensions, and checks the
entire requested AABB against primary-world triangles and live solid boxes.
It commits position and dimensions together only when clear. A blocked change
keeps the old body and animation clock. Probes do not emit touch callbacks or
push other objects; accepted movement still uses the existing touch path.
This atomic operation replaces the old temporary tiny box, flag toggles,
partial resizing and `SETDIMS_PUSHOBJECTS`; it is not that complete SDK service.

ABC `AnimDims` is authoritative, not the old hardcoded crouching height. The
installed Caleb's active standing/crouched half dimensions are **(12,42,12)** and
**(12,28,12)**. The construction-time body remains (12,41,12) until its first
locomotion selection. The synthetic fixture deliberately uses 41/26 to test
resource-driven sizing. Releasing crouch retries standing each frame. When
clearance is insufficient, the player retains the short body, short eye height,
crouch speed/jump and an appropriate crouch/crawl clip. It stands when clear.
Caleb's source eye offsets **36/12** feed both camera and server shot alignment.

Deliberate ordering fixes: crouch plus jump applies the accepted short shape and
reduced impulse immediately; the old duplicated client/server delay is removed.
Forced crouch selects crouch/crawl rather than trying to play standing on a short
body. A blocked animation change does not reset the old clip. Fire-jump retains
its jump state instead of the source's apparent `PMOVE_FIRE_RUN` assignment.
No animation blend/camera smoothing, pushing resize, injury or death flow is
claimed. Leaving/re-entering clears input, crouch state and manager handles.

## Stair service

Blood II's SDK supplies `FLAG_STAIRSTEP` but not its complete implementation.
The new engine implements a bounded **up / horizontal / down** sweep against
the same primary-world triangles and solid boxes used by normal movement.
Eligibility requires the flag, a solid body, walkable support, horizontal motion,
and no upward movement/velocity. Unsupported/jumping players cannot step up.
All three paths must clear, and landing must have a walkable normal and a
positive rise within **half the current half-height** (21 units for retail
standing Caleb, 14 while crouched). Over-height walls and low ceilings continue
through ordinary collision/slide resolution. Descending steps uses gravity and
normal landing; there is no snap down over a cliff.

The half-height default is evidenced by later LithTech's `collision.cpp`; its
solver and additional 0.5-unit tolerance were **not** imported. Blood II retail
stair parity is unverified. Probe failures have no speculative touch callbacks;
accepted paths collect contacts and dispatch only after committing the move.
Reentrant motion/resize is rejected during contact dispatch, and callback removal
remains deferred. Attempts, accepted steps and last rise are inspection counters.
Moving/rotating world models, pushers, crush response, non-step surface flags,
material friction and general depenetration remain unavailable.

## Inspection and tests

The Movement panel shows requested/actual/forced crouch, eye height, dimensions,
source/resolved clip, playback time/loop/done, failed resize reason, stair counters
and limit. Its wireframe samples the **actual player** ABC track and overlays the
collision box and eye. The Models panel remains a separate resource preview.
**Ctrl** crouches; **F2** enables keyboard control, WASD/ZQSD walks, Shift runs,
Space jumps, arrows look, and F1 hides inspection. Panel buttons are held inputs.
Focus loss and text editing suppress commands. Simulation also runs without ImGui.

Seven synthetic cases cover ascending multiple box/triangle stairs, descending,
over-height/low-ceiling/disabled-flag/crouched/airborne rejection, clearance,
feet/eye consistency, reduced speed/jump, clip state/looping/keys, missing chains
(current clip kept and restarted once; failure only without a valid current clip),
exit/re-entry, Ctrl routing and speculative versus accepted touch callbacks.
The private case verifies actual Caleb dimensions/fallback clips and world
movement. `locomotion.chosen` runs, jumps and lands as each of the four Chosen:
Caleb/Ophelia resolve `jmp_rifle`, while Ishmael/Gabriella keep `run_rifle`. The strengthened GPU movement smoke checks SDL walk/run/jump/crouch,
hidden stance changes, live player pose and unload/re-entry. Other-platform, XR
and retail executable feel comparisons have not been performed.
