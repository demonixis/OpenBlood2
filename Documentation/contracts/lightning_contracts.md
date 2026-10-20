# Original world lightning

The separately requested level-loading continuation ports original
ObjectDLL/ClientLightningSFX.cpp and ClientShellDLL/LightningFX.cpp /
LightningSegmentFX.cpp using existing ray, damage, particle and light primitives.
It does not reopen M2 or change the graphics stack/dependency pins.

## Server source behavior

Defaults remain shape2/form1/type2, damage500/radius300, Sounds/Thunder.wav and
sound radius1000. Precreate retains byte conversion for shape/form/type. Initial
update clears gravity, sets FORCECLIENTUPDATE/SAVEABLE and schedules .1-second
updates. Case-sensitive FIRE prefixes cast 5000 units along the actual forward
vector. The query includes solid objects and uses the original LiquidFilterFn
exclusion of every VolumeBrush subclass; it retains the actual hit point or far
endpoint. The bolt publishes before radius damage, followed by a detached sound
at the source position, inner radius10%, MiscHigh priority2.

Damage uses the existing original squared-distance falloff and no visibility
filter. The lightning object is message sender and the triggering object is
responsible, including a null responsible object. The typed adapter accepts this
cross-owner identity only for Normal area damage while that emitter is delivering
its synchronous original radius call. Other forged identities remain rejected.
Reentrant self-activation is explicitly rejected; checked snapshots and deferred
removal preserve the existing lifetime boundary. Damage/visual/audio failures
are distinct, durable diagnostics. The source's .1 polling remains, also after a
restore; the original Save/Load block is ported ([save/load lot 10](save_load_contracts.md#implemented-effect-publishers-and-audio-emitters-lot-10)).

## Client source behavior

Type0/1/2 selects segment lengths500/250/100, truncated then clamped to5..25.
The maximum particle radius scales by the original **integer** segment_count/5.
Flat, boxed, spiral and star shapes retain source draw order and cumulative
rotation. Four forms interpolate radius/offset and blue-to-white color; unknown
shape/form uses the source default branch. Random form retains the source typo:
it changes type after the segment count was calculated, leaving form0 to use the
default wide-to-thin branch. The last endpoint is exactly the destination.

Each segment truncates distance/(2.5*radius/1000) particles and omits the far
endpoint. Particles have zero velocity/gravity and live .25 seconds; alpha .75
starts fading at .1 and reaches zero at .25. Three white point-light states span
source/midpoint/destination, duration .5, radii200 to100, through the original
CExplosionLightFX evaluator. Their illumination remains unavailable like the
existing explosion-light path; the inspector labels that limit. It is not claimed
as a newly working lighting renderer.

The source uninitialized local particle position is deterministically zero.
A zero-count segment stays empty with finite increment0 rather than dividing by
zero. Zero-length bolt axes, unknown types, invalid clocks/vectors and bounds
fail explicitly. Portable cosmetic RNG replaces unavailable retail RNG state;
bit-identical random sequences are not claimed. The typed instant effect is
prepared at publication in the same simulation step, replacing the transient
message/first-client-update carrier. The store retains detached children until
the last light expires; source removal cannot truncate them.

The existing SDL_GPU depth-tested sprite pass draws lightn32.dtx billboards at
radius/640, per-particle RGB and segment alpha. No shaders changed. Bounds are
128 bolts,16384 live particles,64 retained reports,100-byte source labels and
finite endpoint distance<=1e7; IDs survive unload. Presentation is read-only.

## Exact missing authored audio

The actual external archives lack Power Station CVBC's Sounds/lightpop2.wav and
KABOOM's Sounds/Thunder.wav, plus Ancient One's five named Lightning1..5
Thunder.wav references. Original callers continue particles/damage after failed
sound playback. Startup preserves those components and appends an explicit
partial sound omission to the original source row. Acceptance requires exact
world/class/name/path, the same live source-entry identity, actual resource absence
and either preflight or the precise missing-resource failure. Runtime lookalikes,
other paths, decoding errors and visual failures are not waived. No sound alias,
replacement file or fabricated successful voice is supplied.

The earlier [SparksObj contract](world_actor_contracts.md) blanket blocking
statement is superseded only for that exact CVBC source row. The sound failure
remains inspectable and recorded by the audio service.

## Verification boundary

Tests cover all shape/form/type combinations, integer segment sizing, colors,
endpoint continuity, zero particles, missing textures, clock reversal, fade/light
lifetimes, source removal/unload, strict trigger case, actual wall impact,
responsible/null attribution and rejected forged damage. Catalog tests fire all
ten live authored emitters: four real decoded voices and six missing Thunder
references, with actual visual components in every case. Power Station and
Laboratory pass three-difficulty entry/simulation; GPU readback verifies visible
particles, fade and cleared pixels after expiry. This is level-entry and component
coverage, not complete campaign traversal or retail renderer parity.
