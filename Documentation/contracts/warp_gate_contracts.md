# Original world WarpGate effects

`CClientWarpGateSFX`, `CWarpGateSpriteFX` and `CWarpGateParticleFX` selectively
reuse the original server/client code. [Source ledger](../implementation/sources/all_levels_warp_gate_sources.json).
This is part of the separately requested complete-level loading continuation;
it does not reopen M2 or certify full campaign traversal.

The world-file schema retains every sprite, two particle-system and sound
property. Constructor ON defaults true; world `InitiallyOn` defaults false.
String creation does not read custom fields, matching the original. Initial
update marks FORCECLIENTUPDATE/SAVEABLE and schedules one second; that first
scheduled update publishes all three independent children, then schedules 0.1 s.
Case-sensitive prefix TOGGLE/ON/OFF commands alter server state immediately but
rewrite the unchanged visible flags: the next scheduled update publishes visibility.
Sound fields are retained and inspected, but the original controller never plays
them. No replacement audio is supplied.

## Client timing and presentation

The sprite follows owner position and optionally current rotation. It uses the
actual SPR clock, chromakey, original scale/alpha and world depth. ON resets its
trigger clock; the sprite snaps to authored alpha/maximum scale after ramp-up.
OFF interpolates over RampDownTime **using RampUpTime as denominator**. The
update ending ramp-down leaves the last scale; the following update sets zero.
Negative X/Y scale is presented as positive extents plus a half-turn about the
sprite normal, without changing the interpolation.

Particle position follows owner plus world-space offset. Optional alignment
copies initial owner rotation only. Authored XYZ local rotations times pi apply
once per effect update, including while OFF. Emit types preserve source ring,
filled ring, sphere and filled sphere branches, and the cached axes sampled
before each random rotation. Color preserves the source helper's missing G/B
minimum offsets and red-dependent ratios. Gravity, life and the actual authored
DTX/SPR texture are used. Per-effect updates have the original 0.001 s minimum;
particle physics and texture clocks continue between them.

Particle alpha starts at the engine default one and has no post-ramp-up snap.
OFF alpha also divides by RampUpTime. The ending ramp-down update may emit.
`last_add` is never reset with the trigger clock, including after OFF/ON, so
reactivation can pause emission. Existing particles remain until lifetime ends
or the source owner is removed. Each child has independent resource failure.

## Retail SPR particle evidence

Arena authors `Sprites\\flaregunflare.spr` as a particle texture. Later-engine
`runtime/client/src/particlesystem.cpp::ps_SetTexture` accepts both DTX and SPR;
client-manager animation retains the previous texture if the tracker has no
current frame. The installed retail `Client.exe` confirms SPR support directly:
SHA-256 `f7442f26dcbef72c3b69b4560d0fa41d6c9dafa45e603fb12bd32fe6eb2dd7`;
SetupParticleSystem at VA `0x40a1b0` calls `0x443180`, whose SPR branch at
`0x44321e` calls LoadSprite (`0x458970`) and InitTracker (`0x463c60`) before
binding the frame texture. This is recorded analysis, not copied binary content.
No filename alias or invented DTX is used.

Abandoned Buildings' `SingularityMuzzleFlash` authors PS2 count zero with
`SpriteTextures\\` as its filename. Retail's unknown-suffix path succeeds with
no texture. The zero-count system remains inspectable and cannot sample a
texture, so no file is required. A nonzero system with that path fails explicitly.

## Bounds, ownership and adaptations

Finite zero-duration ramps use ratio one instead of source division by zero.
Uninitialized constructor offsets/rotations become zero; sprite `started` starts
false. Parallel forward/up alignment uses the engine's deterministic finite
basis. Cosmetic random draws use owned portable state, not a retail RNG claim.

At most 128 gates, 512 particles per emission, 2,048 live particles per system
and 8,192 across gates are accepted. Lifetime is bounded to 60 s; resources,
poses, scales and colors are checked. Exhaustion is a failure, never dropped
particles reported as success. IDs survive unload, records are bounded, and
owner removal clears children on the next client advance. Teardown clears all
owned resources/particles. Required-resource and runtime failures remain visible
to world validation and the inspector. The original Save/Load block is ported; a restored
gate past its first update publishes its children again (decision-4 departure,
[save/load lot 10](save_load_contracts.md#implemented-effect-publishers-and-audio-emitters-lot-10)).
