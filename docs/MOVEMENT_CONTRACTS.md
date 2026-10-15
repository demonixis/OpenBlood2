# Original MoveMgr Boundary

This continuation executes the standing, dry-land branches of the original
`CMoveMgr`, through the local shells and `CPlayerObj`. It adds walking, running,
digital strafing, jump/release behavior, gravity and collision inspection.
It does not implement the complete original controller or close M2's gameplay
gate. See [source selections](M2_MOVEMENT_SOURCES.json) and
[execution evidence](../../Documentation/Implementation/M2_MOVEMENT.md).

## Original behavior retained

`UpdateControlFlags`, `UpdateOnGround`, selected `UpdateMotion` branches,
`MoveLocalSolidObject`, `WritePositionInfo`, physics updates and forced-position
handling are selectively adapted from `ClientShellDLL/MoveMgr.cpp`. Control bits
come from `Shared/SharedMovement.h`; slope projection follows
`ClientUtilities::TiltVectorToPlane`.

- Caleb's speed attribute 3 supplies walk speed **280**, run factor **1.5**,
  jump impulse **600**, and acceleration/drag coefficient **15**. Values come
  from original player physics messages, not platform bindings.
- Ground movement uses flattened camera forward/right vectors. Opposite commands
  cancel; diagonal input retains the source's **sqrt(2)** speed allowance.
- Jump adds vertical velocity once while held. Releasing rearms it; a press in
  midair cannot add an impulse. As in the source, holding a new press before
  landing can jump when ground becomes available. There is no invented cooldown,
  coyote time or double jump.
- Upward airborne velocity receives no vertical drag. Downward drag uses half
  vertical velocity clamped to `[-25,0]`. Horizontal drag continues in the air.
- Ground normals below Y **0.5** invoke the original steep-slope branch. The
  original asymmetric steep-ground condition and slope-tilt helper are retained.
  This is not certification of every retail slope or stair.
- `Update` retains the original **0.1-second** frame cap. Excess time is counted
  for inspection, not accumulated into an unbounded catch-up loop.

## Typed local integration

The shell owns `CMoveMgr` and a device-independent `MovementInput`. Entry binds
the checked client/player association before receiving owned physics and
force-position messages. `OnServerForcePos` tracks the original byte move code
and clears velocity/acceleration; byte wrap is intentional. Exit clears all
manager handles, command state and counters.

Each session step dispatches due server/object updates, generic physics and
model keys, pumps server messages, advances MoveMgr, updates the inspection
camera, and then advances view weapons. Movement therefore feeds the same-frame
camera and firing origin. Movement continues without ImGui and with it hidden.

For the local session, **one player body** replaces the original duplicated
client physics model and server character. Association marks that record as
client-integrated; generic physics skips it. Each movement substep sends a
typed `PlayerMove` through `CBloodServerShell::OnMessage` to
`CPlayerObj::HandlePlayerPositionMessage`, which calls the real collision-aware
`MoveObject` once. It never teleports through an obstacle to satisfy a leash.
No fake `ClientDE` table, client-only registry object or ECS was added.

The move code must match, and every position/velocity/acceleration must be finite
and within the engine's coordinate bounds. Unknown, disconnected or pending
players reject messages. Typed velocity retains floats instead of legacy short
packet compression. This is an in-process trusted boundary, not an authenticated
network protocol, prediction/reconciliation system or multiplayer anti-cheat.
The original network leash is unnecessary with a shared body and is not ported.

`GetMovementState` returns values and checked contact identities. A fresh **0.02
unit** downward box sweep checks support against the primary solid world and
live solid boxes, ignoring the player. Ground is invalid when velocity points
away from its plane; positive Y while walking uphill still permits support.
World support identifies a polygon, with a null object handle. Walking off an
edge or removing a supporting object cannot retain stale grounding.

The replacement for Blood II's unavailable `PhysicsLT::UpdateMovement` uses
constant-acceleration displacement (`v*dt + a*dt²/2`) and updates velocity before
collision. Substeps are at most **1/120 second**, with at most **12** per capped
frame. This deliberately differs from original frame-size-dependent integration;
30/60/120 Hz tests verify matching results for the supported command sequence.
Supported walkable slopes constrain inward gravity to the contact normal, so
stationary players do not slide under tangential gravity. Source slope forces
remain active; jumping restores full gravity. Horizontal residue below the
source drag cutoff of **0.1** sleeps when horizontal acceleration is zero.
Tiny displacements still resolve contacts instead of using the original
`offset² > 0.01` early-out. These are explicit engine adaptations, not a claim
of retail trajectory or bit-for-bit parity.

The existing swept solver clips inward velocity, slides on walls and ceilings,
and dispatches actual touch callbacks. The manager rereads the resolved body
after each move. Callback removal is deferred; a removed player detaches the
client before any later movement or weapon use. Reentrant session/unload calls
inside contacts are rejected. Invalid geometry/penetration remains an explicit
failure; there is no noclip or depenetration fallback.

## Inspection and device boundary

Enter the player through **Session**, then select **Movement**. The panel shows
position, velocity, acceleration, dimensions, gravity, controls, source speeds,
move code, support normal/object/polygon, jump/landing counts, substeps and
discarded time. A local top view draws the actual primary-world solid slice,
solid object boxes and player heading. It is a diagnostic map, not game rendering.
Hold buttons supply temporary commands at frame boundaries.

**F2** or the checkbox enables physical **WASD / AZERTY ZQSD**, **Shift** run,
**Space** jump and arrow-key look. **F1** hides the inspector without stopping
keyboard movement. Text editing suppresses keyboard commands; focus loss clears
pressed keys and suppresses panel movement. When keyboard movement is enabled,
its keys are routed away from ImGui navigation (unless editing text); Space cannot
also activate a focused inspection button. Game rules receive no SDL types.
Analog gamepad, touch, mouse-look and XR bindings remain future input adapters.
Release compiles the same movement code without ImGui.

## Deliberate incomplete branches

The standing body retains the player's `(12,41,12)` half dimensions. The original
client animation-driven `UpdatePlayerAnimation` / `ResetDims`, crouching and
forced-crouch clearance are not ported; this avoids pretending the server's
unported animation selector drives a second collision shape. Model playback and
callbacks continue independently.

Liquids, ladders, conveyors, spectator movement, trapped/wonky states, slow mode,
pushers/explosion impulses, moving-world-model transforms, standing-on-platform
following, stair stepping, friction materials, surface-type sounds, landing/fall
damage, camera effects and character locomotion animation selection remain open.
The `STAIRSTEP` source flag still does not provide a stair solver. Mass is not
an impulse implementation. Dead-player commands are suppressed, while gravity
continues; the original full death flow remains absent.

The six reference repositories, commercial content and dependency pins are
unchanged. The later-generation LithTech motion implementation was consulted
as context, not imported as Blood II engine code. Only Linux has been exercised;
the retail executable has not been run for movement-feel comparison.
