# M2: Original Flare Gun and projectile lifecycle

Project commit: `9a37206a25da6bc38bd23cd2491682a326dd7bb4`.

The original Flare Gun is now available to the player and Cultists, with both
firing modes, retained fire audio, pickup/drop, inventory restore and in-game
inspection. CityHub's `CultistAI0` no longer fails on `AIWeapon1=FLAREGUN`.
This continuation does not complete M2.

Read the [contracts](../../contracts/flare_contracts.md) and
[source ledger](../sources/m2_flare_sources.json). The complete original
weapon definition is retained. Primary fire consumes one round; alternate fire
consumes eight and creates one parent projectile which later splits into eight
half-damage fragments. Neither mode is replaced with hitscan damage.

Primary flares stick to characters/corpses, follow the original world-space
offset, restart their attached WAV after contact, and deliver original strict
one-second FIRE damage ticks. Death fizzles on the following update without a
second damage burst; target removal and strict lifetime expiry remove without
fizzle. Wall contacts use FIRE-type radius damage. Alternate flares cache their
first velocity and burst strictly after .1 seconds. Tests preserve source child
order, half damage, fragment god mode, independent lifetime and the unusual
FRAG-before-BURST effect ordering when contact coincides with splitting.

The existing checked point-flight phase still integrates once. AddImpact is now
separate from Explode, allowing the original alternate effect-only call without
inventing parent damage. The core contact state distinguishes sticking from an
explosion so a touched object receives one callback. Typed sprite color/alpha
now reaches SDL_GPU; models retain their separate additive-color convention.
The renderer never advances these clocks or callbacks.

CityHub has **422 created / 31 transferred sounds / 541 editor-only /
31 unsupported / one failed** entry. The remaining failed `CDestructableBrush
fallengrills` has no unambiguous matching world model; no replacement geometry or
alias is invented. Structural preparation remains 454 checked / 31 unknown /
zero invalid entries. The new Cultist is created through normal world callbacks,
not injected by the test. All previous compound-geometry assertions remain.

In the labelled synthetic arena using retail resources, the original Cultist
performs **546 updates and eight shots**, reducing player health from 100 to zero.
This is an autonomous AI_Update/FIRE_KEY/projectile test, not a direct damage
command or a claim of complete CityHub encounter fidelity. Two actual SubwayA
load/intro/weapon/unload cycles exercise all four hand/mode combinations: each
primary interval emits three shots, each alternate interval one, with exact
marker and ammo counts and no projectile/audio creation failure.

The source's 60-unit player near-wall correction uses this port's body eye in
place of its absent server view-model proxy. A dedicated test supplies an offset
muzzle beyond a nearby blocker and checks the corrected spawn. Lost alternate
shooters and failed child creation fail explicitly; already-created fragments
remain independent. No mutable original global was introduced, so destination
rehearsal isolation retains its existing ownership model.

## Validation

Linux builds completed in Debug (inspector enabled), Release (inspector disabled)
and Clang ASan/UBSan. Commands and raw results are retained in
[evidence](evidence/m2-flares.json).

| Check | Result |
| --- | --- |
| Debug headless | 691/693 on the full run; both corrected catalog/protocol assertions then passed (3.99 s) |
| Release headless | 693/693, including 112 private-data checks (15.42 s) |
| Debug GPU | 11/11, including eight private-data checks (58.32 s) |
| Release GPU | 11/11, including eight private-data checks (25.28 s) |
| Clang ASan/UBSan, leak detection | 54/54, including 12 private-data checks (278.62 s), no findings |
| Format/reference integrity | 36 C++ files formatted; 10,923 reference files unchanged; six reference repositories clean |

The Debug full-run exceptions were the old pickup catalog total and old invalid
weapon ID 3. Their corrected targeted rerun passed; Release additionally ran the
entire suite with both corrections. All behavioral and lifetime assertions remain.
The GPU suite includes the application/inspector lifecycle and weapon smoke,
compound geometry, existing energy/sprite/model-alpha paths, both Mac-10 hands,
colored flare flight and both actual Flare Gun view models. Visually inspected
Release captures remain in ignored build directories. Both weapon models produce
16,191/16,196 pixels on the left/right halves on each load; the moving retail red
sprite produces 39,232 then 17,492 red pixels as it recedes. No validation error was
reported. Only Linux/Vulkan was verified.

The initial ASan pass found borrowed-pointer reads in two new tests after
`Objects::advance` had already reclaimed the projectile. Those assertions now
resolve checked handles and durable terminal reports; the lifecycle checks were
retained. A fixture also attempted to initialize an existing session twice;
resource mounting is now separate. The initial 320×240 synthetic GPU tint probe
had too few bright red pixels after perspective shrinkage. It now uses 640×480
with the same minimum pixel assertions. The actual retail sprite and both view
models passed their initial GPU runs. Initial failures remain in retained logs.

The pickup catalog additionally creates both authored FlareGunPU entries: 1,265
created pickups across 57 readable worlds, no missing models, 176 unchanged
TypeAction rejections and 60 supported destruction spawns. The negative weapon
protocol test now uses unsupported ID 4 rather than newly supported ID 3.

## Remaining M2 work

Original spark trails, attached lights and flare explosion recipes 13–15 remain
explicitly unavailable. Source color/scale and WAV playback are real; component
request records are not successful effects. Chrome, muzzle illumination,
view-model key sounds, fire/gib death presentation and complete combat parity
remain unfinished. Player-initiated drops and projectile save/load are not added.
The 31 unsupported CityHub entries, missing model, safe live NEW_LEVEL transfer
and the unchanged observable M2 exit criteria remain open. M3–M8 are unchanged.
