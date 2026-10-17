# Original authored particle streams

Selected original `CClientParticleStreamSFX` and `CParticleStreamFX` now execute
through an owned typed descriptor and the existing particle/billboard services.
See [provenance](M2_PARTICLE_STREAM_SOURCES.json) and
[validation](../../Documentation/Implementation/M2_PARTICLE_STREAMS.md).
**M2 remains incomplete.** CityHub still has ten unsupported entries and one
missing world model; its NEW_LEVEL transition continues to refuse safely.

## Source rules

World-file creation reads the original registration defaults and authored values.
Initial update disables server scheduling, clears gravity, sets FORCECLIENTUPDATE
and SAVEABLE, and publishes the effect before inherited difficulty filtering.
USRFLG_VISIBLE alone controls emission. The server's whole command uses
case-sensitive prefix comparisons: TOGGLE (six characters), ON (two), OFF (three).
Unknown commands still reapply the current state. The copied trigger string is
released exactly once. Source strings become owned values, without packet ABI or
borrowed client pointers. Runtime/string/save creation rejects explicitly because
the original only initializes these fields on world-file creation.

An initially enabled stream starts directly in state 2 and emits immediately;
it does not ramp up. An initially disabled stream starts empty. Client states are
0 off, 1 ramp up, 2 on, 3 ramp down. Visibility reversals reset trigger time and
restart a whole ramp. Zero ramp time switches immediately. Ramp completion is
checked only when a burst is due, using strict `time > last_add + delay`.
No catch-up loop is added. Off and completed ramp-down leave last-add unchanged.
A zero-count ramp burst still advances last-add. Amount truncates toward zero;
offset, velocity/spread and lifetime scale only under their respective four bits.

Each particle draws lifetime, X/Y/Z offset, X/Y/Z spread, speed, then red color.
Velocity is normalized (forward * maxVelocity + spread) * sampled speed. The
original BaseParticleSystemFX color formula clamps negative channel ranges to
zero, samples red, then uses rangeY*red/rangeX and rangeZ*red/rangeX **without adding
the minimum green/blue**. If the red range is zero, the complete minimum color is
used, including CityHub's always-yellow spark particles. RGB is normalized and
saturated for the existing renderer. The independent seeded RNG preserves this
local sequence, not the retail global RNG.

The system translates with its current checked owner. Existing particles retain
local offsets and world-oriented velocities; owner rotation only changes new
emission directions. System rotation stays identity. Positive gravity increases
Y velocity. Existing particles age even when off; removing the owner destroys
the entire system. Rendering/inspection never advance simulation. System alpha
is constant; no invented per-particle fade, collision, damage or lighting exists.

The original server writes SoundRadius and three sound strings, but the original
SFXMgr PARTICLESTREAM branch stops reading after ParticleFile. Consequently no
sound reaches this client effect. Preserve and inspect the unused tail; do not
claim playback or invent a sound path. Texture precaching is replaced by checked
external DTX loading. An empty authored texture remains a failure.

## Engine adaptations and lifetime

Uninitialized original trigger/last-add clocks are initialized to zero. A zero
velocity vector remains zero instead of normalizing into non-finite values. These
are explicit deterministic portability adaptations. Other numeric fields,
coordinates, rotations, resource names and clocks are checked before use.

One source has at most one effect. Limits are 128 systems, 256 particles per burst,
2048 per system, 8192 across streams, 600-second lifetime/ramp/delay and bounded
positions/velocities. Burst capacity is checked before allocation/random draws.
Failures retain named reports and fail creation or the session update; no silent
particle truncation or success-shaped empty effect is substituted. Reports retain
the latest 128 deliveries and their failure/removal state; the latest update error
also survives report eviction. IDs are not reused on reload.

The shared particle integrator preserves lifetime-first, position-before-gravity,
strict negative-lifetime removal, with a maximum 60-second step. The separate
original effect cadence is 1 ms, at most one callback per session step. These are
replacement engine policies, not verified retail physics/raster equivalence.

Pending/deleted owner handles never render, and the next simulation update removes
their effects. Difficulty filtering retains original publish-before-filter order;
the filtered owner cannot keep particles alive. World unload clears effects,
reports and random state. Session isolation/NEW_LEVEL preserves source-owned arrays
and destroys candidates under their own adapter. Initial destination validation
also counts stream textures against its shared image budget, including off streams.
The existing global sprite mesh limit remains 16384 quads, with explicit overflow.

## Inspection

The Scene explosions panel includes an Original particle streams tree with owner
identity, live/user visibility, off/ramping/on state, emission clocks, bounds,
resources, authored sound tail, emitted/live/prepared counts and every particle's
position/velocity/lifetime/color. General object properties expose all original
fields and difficulty settings. Named failures survive object deletion. Hidden UI
and inspector-free builds run the same simulation; presentation counters indicate
prepared geometry, not GPU visibility or gameplay success.
