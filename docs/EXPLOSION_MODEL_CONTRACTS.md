# Original explosion model contracts

This extends [sprite effects](EXPLOSION_SPRITE_CONTRACTS.md) and
[scene explosions](SCENE_EXPLOSION_CONTRACTS.md). See
[evidence](../../Documentation/Implementation/M2_EXPLOSION_MODELS.md) and
[source selections](M2_EXPLOSION_MODEL_SOURCES.json). **M2 remains incomplete.**

## Original behavior

CExplosionModelFX retains selected original Init/CreateObject/Update behavior:
scale interpolation, fade modes, initial orientation, incremental rotation, full
bright flags and end duration. It owns an immutable decoded ABC, an animation track,
strings and checked visual identity instead of a legacy client HOBJECT/HSTRING.
The uninitialized false case of m_bScale is explicitly initialized.

Scale waves are linear, square root, square and triangular versions of those three
(waves 3, 4, 5). Other bytes retain linear fallback. Fades 0..5 match the existing
sprite formulas. Initial object alpha precedes the first Update; fade 2 then starts
at one and fade 3 at zero. The final scale/rotation/fade update runs before the
strict time < duration result. Overshooting triangular-square-root scale can be
nonfinite in that final source calculation; the expired object is removed before
presentation, never submitted to the GPU.

Random rotation 0 preserves identity. Mode 1 draws pitch, yaw, roll in that order
from [-pi,pi]. Mode 2 aligns to the normal using +Y up, then rotates around the
resulting forward axis by a draw from [0,pi]. Each Update caches **all three axes
before rotating**, then applies rotations.x around up, rotations.y around right,
and rotations.z around forward. Source angles are per update, **not multiplied by
delta time**. The existing quaternion convention supplies these engine operations;
this is not verified retail matrix parity. The source 1 ms effect-update cadence
is preserved; ABC animation advances by elapsed simulation time independently of
whether an effect update is due. Visual keys do not dispatch gameplay callbacks.

Original ClientUtilities integer modulo and float range formulas use a separate
portable minstd_rand client-effects stream. One recipe waveform draw precedes the
three Euler draws. The stream resets to seed 1 on world unload without reusing IDs
and never consumes server gameplay RNG. The generator differs from platform rand;
retail random sequences and draws of unported composite components are not claimed.

Null model/skin settings select original defaults exp_sphere.abc and Explosion_1.dtx;
empty strings fail. Position, normal, rotations and nonnegative scales obey existing
finite physics limits; duration is (0,600], alpha [0,1], fade 0..5 and random mode
0..2. Paths are at most 1,000 bytes with no embedded NUL. Alignment mode 2 and
composite recipes reject a zero normal. Invalid/backward clocks fail explicitly.

## Selected recipes and delivery

The same high-detail composite subset as sprite effects is routed from original
CClientExplosionSFX, before sprite publication, original null-damager damage and
sound. Direction normalizes once per selected recipe. Models are independent
children; their failure cannot cancel sibling sprites or sound.

| Type | Normal offset | Uniform scale | Duration | Model / skin |
| --- | --- | --- | --- | --- |
| 0 small | 10 | 25 to 200 | .5 s | Original defaults |
| 1 medium / 3 grenade | 10 | 50 to 400 | .5 s | Original defaults |
| 2 large | 10 | 100 to 600 | .5 s | Original defaults |
| 8 Tesla alternate | 30 | 50 to 1000 | 3 s | duel_sphere.abc / SpriteTextures\waterblue.dtx |

All use alpha .75, fade 1 and random orientation 1. Types 0..3 choose waveform
0..2 with the original integer rule; type 8 uses linear. Type 6 has **no authored
model branch**: its report succeeds with no created model, not a fabricated effect.
Other types, including RIFT_2, still reject. No generic brush/weapon FX routing or
quality menu is introduced. Complete-composite availability stays false: rings,
particle flames, fragments/trails and lights are still missing.

At most 128 models and 64 retained model-delivery reports are owned by the session.
IDs are never reused, including across unload. Model/event IDs and failure strings
are copied into inspection, with no retained emitter pointer. Source removal leaves
models alive; expiry counts separately from failure. Update failure retains a
message and fails the session step. Unload clears models, reports and RNG state.
Missing or malformed ABC fails creation; an indexed skin can still fail decoding
later, which remains a separate visible presentation failure.

## Presentation and inspection

A separate read-only ModelView snapshot reuses the bounded ABC/DTX readers and
geometry sampler. Optional per-draw alpha extends ModelView without changing opaque
world/character/view-weapon behavior. FX require a valid decoded skin; missing skins
emit no guessed magenta model. Zero scale retains inspection but emits no triangles.
The existing source-alpha shader/pipeline (named SpriteAlpha) draws these textured
triangles with shared world/model depth, without writing depth. Fullbright/no-light
flags use the existing unlit policy; global light scale is not implemented.

FX models sort stably by camera-center depth and draw before the sprite pass and
close weapons. Cross-type model/sprite ordering, per-triangle sorting, two-sided
self-overlap and global lighting are **not retail transparency parity**. This is
bounded diagnostic presentation of real original effects, not a complete renderer.
Resources and GPU buffers clear on world revision; simulation and presentation run
outside ImGui. No extra player integration or server world object is created.

The Scene explosions panel shows model identity/event, time, scale, alpha, waveform,
model/skin names, triangles/preparation/failure, totals and retained reports. Model
and sprite failures have distinct delivery IDs. Sprite reports list missing
particle/light/fragment components; model availability is reported separately.

All nine SubwayA emitters are type 3. Three load/player/unload cycles verify nine
168-triangle exp_sphere models, valid Explosion_1 skins, alpha .585 after .11 s,
independent source removal and expiry at .5 s while sprites continue. Synthetic
checks cover the other recipes, all wave/fade modes, mixed cached-axis rotations,
RNG draw counts, resource/clock/budget failures and non-reused IDs. GPU readback
verifies linear half-opacity with real model triangles and separately captures the
private original sphere. Hidden/reloaded application smoke requires a model draw.

World accounting stays 203 created / four transferred sounds / 372 editor-only /
one unsupported Gideon / zero failed sources. Safe automatic NEW_LEVEL and other
required FX/services keep the actual M2 gate open.
