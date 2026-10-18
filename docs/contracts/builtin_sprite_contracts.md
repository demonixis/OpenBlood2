# Original built-in Sprite and FastApproxArea

The registered `Sprite` class runs the original `spr_MessageFn` from
`AppHeaders/engineobjects.c`. `BuiltinSprite` is only its C++ adapter name; the
engine's decoded SPR manifest keeps the name `Sprite`. This is distinct from
[DetailSprite](sprite_contracts.md) (a B2BaseClass object with flush/repeat rules).
`FastApproxArea` is an original `CF_NORUNTIME` class. Read the
[object](object_contracts.md), [sprite presentation](sprite_view_contracts.md) and
[demo](demo_contracts.md) contracts before extending either.

## Sprite source behavior

The parent is BaseClass. Registration defaults are an empty Filename, Flags
FLAG_VISIBLE, Scale (1,1,1), Color (255,255,255) and Alpha 1, with source
`CF_ALWAYSLOAD` intent supplied by static registration.

PRECREATE forces OT_SPRITE, then reads Color, Scale and Alpha for **every**
creation reason before delegating to `bc_EngineMessageFn`. Runtime creation
therefore also reads the registered defaults, so Scale (1,1,1) replaces a
supplied structure scale. Color components and `Alpha * 255` are truncated to
bytes. The world/string base reader then applies Name, Pos, Rotation, Flags,
the optional boolean flag overrides (Visible, RotateableSprite, Chromakey...)
and Filename.

INITIALUPDATE calls `SetObjectColor(r/255, g/255, g/255, a/255)`. The source
passes the **green byte as blue**; the port keeps this quirk and the inspector
names it. Neither callback schedules updates. There is no B2 difficulty filter,
damage aggregate or light emission.

The source casts unchecked floats to bytes. Color outside 0..255, Alpha outside
0..1 and nonfinite values have no defined result there, so they reject creation
explicitly instead of guessing a wrap. A failed SetObjectColor latches an error
and the world entry is Failed. Missing/malformed SPR manifests use the existing
OT_SPRITE binding and reject creation, as for DetailSprite.

## Resources, presentation and lifetime

The generic OT_SPRITE path binds the SPR manifest after precreate and before
initial update, advances the frame clock in the existing model traversal, and
draws through the existing sprite scene with the object tint and alpha. Flags
without FLAG_ROTATEABLESPRITE face the camera. Hidden objects keep inspection
but emit no quad; pending objects stop drawing immediately; world unload clears
caches. No chromakey, glow, lighting or retail blending parity is claimed (see
[sprite presentation](sprite_view_contracts.md)).

The Sprites inspector tab lists built-in Sprites beside DetailSprites, with
their source dispositions, original RGBA bytes, SetObjectColor result and the
applied tint/alpha.

## FastApproxArea

`FastApproxArea` (and `OutsideDef`, the light classes) are preprocessor input
that the original engine never instantiates. They are recorded as editor-only
world entries with their properties retained and are not registered runtime
classes. No visibility optimisation is derived from them.

## Evidence

Demo_02 and its retail counterpart 05_CDMB each contain 28 Sprites (sprites
`redlight1/2.spr`, `shiplightred/white.spr`, Flags 1, white, Alpha 1, scales .15,
.25 and 1) and one FastApproxArea. All 28 now create, bind their manifests and
prepare quads; FastApproxArea is editor-only. Tests: `builtin_sprite.rules`,
`rejects`, `scene` and `editor_only` (synthetic), `builtin_sprite.private`
(retail 05_CDMB, `BLOOD2_TEST_DATA_DIR`) and `builtin_sprite.demo` (Demo_02,
`BLOOD2_DEMO_DATA_DIR`), each repeated across a world unload/reload.
See the [source ledger](../implementation/sources/demo_world_sources.json).
