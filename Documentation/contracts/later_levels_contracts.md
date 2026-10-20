# Crypt, Bridge, Entrance and Lobby continuation

This separately requested continuation fixes startup of `15_crypt`, `16_bridge`,
`17_entrance`, `19_lobby` and `19_lobbyc`; `19_lobbyb` is a regression case. It
extends the existing world, AI, weapon, particle and brush contracts. M2 closure
and reservation M2-GPU-001 are unchanged. Successful entry and focused combat
tests do not certify complete level traversal or every scripted encounter.

## Original actors and weapons

Behemoth, DrudgePriest, DeathShroud and TheHandAI have original constructor
profiles, models, skins, shared animation tables, stimuli decisions and selected
attack/death callbacks. They use the existing checked AI lifecycle, original
aggregate chain and deferred removal. Unavailable clips remain explicit failures.
TheHand and DeathShroud lack the humanoid head needed by GenerateHitSpheres:
they use the existing nonhumanoid body-damage policy, without fabricated nodes.

Behemoth chooses claw or ground shockwave by attack state and restores the claw
after animation completion. Far attacks retain the low-health animation choice
and its original ComputeState argument/computed-stimulus distinction. The existing
shockwave projectile supplies ground contact, damage and effects. The original
shockwave definition mistakenly contains WEAP_MELEE in its type field; this sparse
typed table uses the actual inventory selection ID 36.

DrudgePriest stays airborne, switches between head-aimed lightning and fireball,
and uses the source 30-second BoneLeech spawn gate, nearby-monster exclusion,
taunt clip and 1.5-second spawn interval. The authored `dp-looptry1.wav` is absent
in the retail archives; its optional audio failure remains explicit. Lightning
damage executes; the separate boxed/thin/high-segment lightning visual remains an
explicit unsupported effect, not a substitute beam.

DeathShroud retains its looping audio, .9 alpha, solid/non-solid fade, skull fire
and shrinking death. Initial scale metadata is changed after base initialization,
as in the source. Death chooses corpse metadata without starting a death clip.
Reappearance still requires the existing bounded world-overlap resolution.
Skulls reacquire the nearest moving character head within 200 units, preserving
speed while steering. Contact moves to the head plus 7.5 forward units, delivers
the original immediate damage and then follows the explosion callback. Checked
links, fire-trail type 3, flare-burst type 14 and loop cleanup are retained. The
source override has no base projectile timeout; none is invented here.

TheHand uses the existing BoneLeech handshake, checked player attachment,
periodic 5–10 damage, choke sound, USE/detach and recovery lifecycle. Its jump
attack adds the original taunt phase. Its first-person model and camera offset
are specific to TheHand, with the source .9-per-second darkening.

Minigun ID 11, its full definition and MiniGunPU now work through shared owned
inventory, held/view models, pickups, drops, NPC aiming and both fire modes. Each
fire produces four vectors, consuming four bullets at the source per-vector cost.
The source empty AddImpact override suppresses impact effects while damage and
hearing still run. Chrome/environment mapping remains outside this rendering
subset. BehemothClaw, BehemothShockwave, DrudgeLightning, HandSqueeze and Skull
also have their original definitions and typed inventory factories.

## Authored data and geometry

Only Light/DirLight's vector `Pos` may retain nonfinite editor metadata. The
reader marks it invalid, hides it from typed position lookup and excludes it
from dynamic object lighting. Raw values remain available for inspection; no
zero-position light is synthesized. Other vectors, types, lengths, duplicates
and runtime positions remain strict. Property errors now include class, property
and the property byte offset. Bridge has two such preprocessor lights.

Non-box world-model bodies use authored triangle boundaries even when their
stored BSP planes are stale, extending the existing dynamic polygon-body path
to static bodies such as Entrance's barb3. Open boundaries are not converted to
closed volumes. The complete-level continuation now uses those validated
triangles for BoxPhysics models too: their collision still uses their authored
box bounds and needs no closed polygon volume. Container volumes retain strict
planar/closed validation; invalid coordinates/indices remain errors.

Sky portal polygons never sample their authored ordinary texture: the mesh
builder now avoids loading it. Bridge's two sky faces name the absent `Default`
token. The same missing texture on an ordinary visible face still fails.

PolyGrid declares the original unused PlasmaInfo editor group. Zero-area grids
retain their wave state but generate no triangles. Their failed sprite lookup
is retained in `texture_error` and displayed in the inspector; no replacement
texture is generated. Lobby's all-zero grid references absent Water1.spr. A
nonzero-area grid still requires its sprite. The other Lobby grid uses Flag1.spr.

Finite Trigger ResetTime values are no longer limited to one million seconds:
Bridge authors approximately 1e14 seconds. This is one double-precision deadline,
not a loop/allocation count. Nonfinite times remain invalid; nonpositive resets
retain the existing disabled-timer adaptation. The exact absent source class
`BatterAmmoPU` joins the existing destruction-spawn typo list: the brush remains
real and destructible, and its death reports a null source GetClass result. There
is no alias to BatteryAmmoPU or placeholder pickup.

Known missing resources are explicit, counted MissingResource source entries,
matched by class, property, exact path, missing lookup and failure category:

| Class/property | Authored missing path (escaped control characters) |
| --- | --- |
| CDestructableModel/InitFilename | `models\worldobjects\coffeemachine.abc\r\n`, `models\worldobects\ceilingfan1.abc`, `models\worldobjects\chairexec.abcm` |
| CDestructableModel/InitFilename | `models\worldobjects\woodcratesmalll.abc`, `models\worldobjects\noparking.abc\r\n`, `models\worldobjects\antennabig.abc\r\n`, `models\worldobjects\garbagebag.abc\r\n`, `models\worldobjects\garbagebag.abc\r\n\r\n` |
| CClientParticleStreamSFX/ParticleFile | `SpriteTextures\singlemus02.dtxr`, empty string, `spritetextures\particlesmoke.spr` |
| Sound/Filename | `sounds\hum.wav`, `sounds\ambient\sewage1` |
| ParticleSystem/TextureName | `Sprite Textures\lightn32.dtx` |

Resources and source DATs are never changed or resolved by fuzzy names. Other
missing assets, classes and required services still reject startup. Earlier
missing-world-model omissions remain accounted separately.

## ParticleSystem and alpha

The original ParticleSystem descriptor now publishes owned client particles. Its
Static field is retained but not transmitted, matching the commented-out source
message fields. Only render flags zero are supported. Creation from world/string
properties is supported; runtime/save descriptors explicitly reject. The source
MID_UPDATE removal callback is retained without inventing an initial timer.

The client uses floor(rate × elapsed), fixed particle lifetime, cube emission
offsets, vertical min/max velocity, horizontal spread, burst waits and Y rotation.
It neither carries fractional emissions nor catches up with an unbounded loop.
The later engine's six random draws and reused RGB samples are reproduced using
the existing portable RNG, not claimed CRT-bit-identical. Hidden systems keep
emitting; the hide-transition update alone resets the emission clock. Rendering
uses the rotating local frame and hides invisible systems. Existing per-system
2,048/global 8,192 particle budgets remain strict.

Bridge's streams author SystemAlpha=7. Later LithTech's SetObjectColor stores an
unsigned byte. The adapter explicitly truncates alpha×255 and wraps modulo 256,
then normalizes the byte (7 becomes 249/255). This defines the out-of-range
conversion portably instead of invoking undefined C++ float-to-byte conversion;
it is not proof of the original executable's behavior. Nonfinite alpha still
rejects, and the original descriptor remains available independently of effective
render alpha.

## Verification boundary

`later_levels.*` exercises all six maps through start_new_game at all three
difficulties, tiny initial frame deltas, movement, three seconds of simulation,
new actor runtime status, replacement and complete teardown. Focused cases cover
Behemoth attacks, Priest spawning/lightning/fireball, Shroud fade/skull tracking,
contact and death, TheHand attachment and Minigun firing. Synthetic tests cover
nonfinite editor positions versus invalid gameplay data, sky-only resources,
zero-area PolyGrids, particle clocks/budgets/visibility, long resets and stale
brush planes versus strict volumes. Run object lifetime tests and Clang
ASan/UBSan separately from the inspector-free real-display Release checks.
