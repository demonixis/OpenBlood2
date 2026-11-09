# Legacy smoke trails and view weapon models

`CClientSmokeTrail` uses the setup from `ObjectDLL/ClientSmokeTrail.cpp` and
the client clocks from `ClientShellDLL/SmokeTrailFX.cpp` and
`SmokeTrailSegmentFX.cpp`. The local client frame advances the segments
and submits the same typed billboard batches used by other particles.
Initial velocity selects moving or stationary puffs. The small flag halves
the radius, and liquid selects bubble art, lower gravity and a triple puff
count. The retail archives store the bubble under `SpriteTextures/Particles`,
which the port checks when the source path is absent. Each segment
interpolates across carrier movement and fades after emission stops or its
carrier is removed. Particle colour saturates to the range supported by the
shared renderer. The current projectile and debris emitters have their own
effects.

`CViewWeaponModel` uses the close-model flags and key names from
`ObjectDLL/ViewWeaponModel.cpp`. A checked player link and inventory hand
replace the borrowed weapon pointer. A server caller queues typed shot
parameters before the model's `FIRE_KEY`. The key consumes that request
once through the player's normal server fire handler. Sound keys attach to
the player and retain one loop until `SOUNDSTOP_KEY`, rebinding or teardown.
Node visibility keys use checked model-node services. The normal first-person
weapon uses the existing client `CViewWeapon` path.

`legacy_smoke.trail` checks mounted smoke and bubble textures, interpolation,
small and stationary puffs, and fading after removal. `weapons.legacy_view_model`
uses a synthetic model animation and audio archive to check both inventory
hands, one-shot and looping sound, node visibility and owner cleanup.
