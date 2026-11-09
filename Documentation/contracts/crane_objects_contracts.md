# Original crane and line objects

The selected source is `ObjectDLL/VolumeBrushTypes.cpp::CraneControl`,
`ObjectDLL/WreckingBall.cpp`, `ObjectDLL/ClientCastLineSFX.cpp` and
`ObjectDLL/clientdebugline.cpp`, with their client line systems in
`ClientShellDLL/CastLineFX.cpp` and `ClientShellDLL/debugline.cpp`.

`CraneControl` finds the named crane and ball and creates an inactive tracking
camera. The source lock query and the player's camera-selection block are
commented out. The authoritative volume pass applies the original
0.03-radian yaw step, clamps at ±0.6π, and gives left precedence. The player's
head must select the control volume. The rotating brush uses the checked
world-model solver. CraneControl's save payload is unsupported.

`WreckingBall` uses the original model and skin, mass, beam offset, mirrored
forward X coordinate and 580-unit tether. The bounded engine keeps the
horizontal offset within the chain length before calculating height. A
touch damages world models within the source 300-unit radius. Loss of the
crane removes the ball and its linked cast-line carrier.

The cast line takes its length at setup and follows its carrier pose. The
source copies the start colour into both vertices and takes the magnitude
of the absolute ray hit point when no target was supplied. Debug lines
preserve the source relative-endpoint subtraction, red colour and 20-second
lifetime, with independent client presentation after the emitter retires.
The local frame carries copied endpoints and uses the shared line renderer.

The private `crane.crane` and `crane.debug` tests use the installed resources
with a small synthetic arena, exercising volume input, world-model rotation,
the moving tether, touch damage, line publication and lifetime.
