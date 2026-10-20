# Original dynamic light contracts

Continuation: [explosion point lights](explosion_light_contracts.md) now evaluate
selected original transient colors/radii with independent lifetime. Earlier light
omissions are superseded only for that state; surface illumination stays unavailable.

M2 is incomplete. Read [object contracts](object_contracts.md),
[keyframe contracts](keyframer_contracts.md), [shared damage](brush_contracts.md),
[pickup/spawn contracts](pickup_contracts.md), [source selections](../implementation/sources/m2_light_sources.json)
and [implementation evidence](../implementation/reports/m2_lights.md).

## Original server behavior

ClientLightFX and its thirteen original waveform/compatibility subclasses use the
original property defaults, CDestructable aggregate, B2BaseClass filtering and
callback order. CClientSFX ancestry is preserved, but ClientLightFX explicitly
calls B2BaseClass as the source does, bypassing CClientSFX's FORCECLIENTUPDATE hook.
World/string creation reads properties; runtime creation retains constructor
values, including NONE waves, radius minimum 0, frequency 0 and lifetime -1.
World defaults instead use radius minimum 200, frequency 4 and lifetime 0.
A restore reads no property, keeps PostPropRead and publishes the saved effect message
again from its Load ([save/load lot 10](save_load_contracts.md#implemented-effect-publishers-and-audio-emitters-lot-10)); cache and the general SFX manager are not implemented.

Source clamps remain asymmetric: LifeTime and IntensityMin/RadiusMin are at least
zero, IntensityMax is at most 255. Do not sort min/max or impose a new gameplay
range. Color divides by **511**, and authored phases convert degrees to radians.
Frequencies are radians per second; no extra 2*pi multiplier is applied. Initially
on lights receive visible/saveable user bits and a first server update after
0.1 seconds plus a world/string-only random offset in [0.01,0.5]. Initially off
lights receive neither bit and no schedule. Runtime on lights start after 0.1.

Triggers compare case-sensitive **prefixes** TOGGLE, ON and OFF, update the visible
user bit and source on-state, and do not rearm scheduling or restart lifetime.
Thus an initially off light turned on later never starts its server lifetime
schedule. An initially on light turned off continues scheduled lifetime checks.
Each server update rearms at 0.1; positive lifetime expires at elapsed >= duration.
Shared damage runs once before checking death/removing the light. KILL uses the
aggregate's existing case-insensitive whole-token death damage and requires a live
sender. CanDamage, TriggerDestroyOnly, original death/damage triggers and supported
SpawnObject behavior remain shared. Lights do not gain invented solid/ray-hit flags.

## Evaluated client state and engine boundary

Original CLightFX wave tables/formulas and update rules evaluate a real owned
point-light record, bound to a checked source handle. This replaces the SFX packet
and separate legacy client OT_LIGHT object; it is not a generic packet-ABI port.
No GPU world illumination service is claimed. CastShadows, SolidLight,
OnlyLightWorld, DontLightBackfacing and FogLight bits are retained metadata.

Creation evaluates both waves, including NONE (maximum), and begins visible as in
the source CreateObject. The first normal update applies actual server visibility.
Every step follows source position even while off. While on, only non-NONE waves
update: phase is absolute session time * frequency + phase, not time since ON.
While off, radius becomes the minimum, color holds and off_start_time records the
current time; that field does not drive waveform evaluation. Consequently NONE
radius stays at its minimum after OFF/ON. Evaluated radius clamps to [0,10000];
each color channel clamps 0.5 + normalized source color * intensity to [0,1].

Wave identifiers 0..11, original sample tables, square boundary <= pi, saw ramps,
strobe and search behavior are preserved. Negative phases are deliberately wrapped
into [0,2*pi) before table access, preventing the source's undefined negative array
index. A rounded upper endpoint is wrapped to zero. This safe extension is not
claimed retail numerical parity. Nonfinite or abs(value)>1,000,000 property values
reject before source clamps; color/position use existing physics-vector bounds.
Sound names are owned, limited to 100 bytes and reject embedded NUL. Light flags
outside 0xf8 and unknown wave IDs reject; no byte-wrapped aliases are invented.

PublishLightEffect accepts one descriptor per live ClientLightFX-derived source,
up to 4,096 records. Invalid/duplicate publication fails explicitly. The local
RandomFloat service validates finite ordered bounded inputs and uses a portable
seeded minstd_rand stream reset on world unload. This reproduces scheduling jitter,
not the retail CRT/global random sequence. No waveform depends on this jitter.

Client state advances after normal simulation and deferred destruction, independent
of UI/client entry. Pending sources immediately fail GetLightEffect; destruction
removes their record and aggregate list membership. World reset clears values and
random state. Do not retain pointers into the light vector across publish/remove.
Ramp sound paths are inspectable/indexed only. The original PlayRampSound method
has no callers in the imported update path; no sound event/playback is invented.

## Inspection and validation

Lights includes hidden/off/inactive/pending sources, class, health/damage gates,
lifetime/schedule, flags, normalized parameters, waveform plots, evaluated color,
radius, position, update counts and indexed/missing ramp resources. An XZ plot shows
all source positions and the selected radius. This is an inspection overlay, not
world lighting. ON/OFF/TOGGLE/KILL are deferred UI commands through original messages.
Source-world properties and failed/missing classes remain visible in World.

Eight synthetic cases cover all waveform families/boundaries, negative phases,
clamps/defaults/aliases, runtime/string differences, command/lifetime quirks,
damage/chained death/spawns, invalid descriptors/handles, two-target keyframing,
linked target removal, filtering and repeated teardown. The private catalog loads
72 lights across 57 readable worlds (two initially off, zero filtered/rejected),
checks OFF/ON and cleanup; 16_bridge.dat remains a strict parser rejection.
Two complete SubwayA reloads verify twelve moving GlowingLight targets, current
client positions, commands, nine consumed Keys and exact live-object counts.
Their authored intensity/radius ranges are constant; varying waves are verified
in synthetic cases. The expanded private weapon scenario remains under its unchanged
90-second timeout when sanitizer tests run sequentially (89.07 seconds in the
full run); the initial parallel run timed out. Use the documented sequential
ASan command and retain this narrow timing margin as a known validation limit. GPU smoke checks actual moving state while UI is hidden and
after reload, also in the build without ImGui.

These are dynamic point lights. Preprocessed Light/DirLight and the implicit main
directional light are separate: the owner's clarification identifies DirLight as
spots. World illumination, shadows/fog, general client FX/audio, cinematic/world
model motion, AI, transitions and other required M2 services remain pending. No
retail executable, additional platform, mobile or XR validation is claimed.


The [flashlight continuation](flashlight_contracts.md) adds original item2, L toggle,
active-only charge, local audio, checked child lifetime and forced-inactive
Restore/KeepAlive. One radial point light is rendered over the unlit world/models;
GetPointShade, shadows and general dynamic illumination remain unavailable.
Earlier blanket flashlight omissions are superseded only by this bounded subset.

The [baked lighting continuation](baked_lighting_contracts.md) renders the preprocessed light of `Light`/`DirLight` objects that the DAT bakes into per-vertex colors, and lights models from it. Light objects themselves still have no runtime service, and dynamic lights other than the flashlight still illuminate nothing.

The [static light continuation](static_light_contracts.md) reads `Light`/`DirLight`/`ObjectLight` properties directly to light models with the verified original term. These objects still create no runtime object.
