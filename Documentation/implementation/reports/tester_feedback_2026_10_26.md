# Tester feedback, 2026-10-26 (0.5.0)

Discord and X reports on 0.4.0, on top of `a50bd918`. The version is now 0.5.0.
No commercial assets or reference-source dependencies are added. M2's reservation
and milestone acceptance are unchanged.

## Museum crash after Gideon's hologram (fixed)

Both crash reports ("crashes after the cutscene ends", "crashes every time to desktop
after the cutscene, no error message") match 03_Museum's third hologram. Touching
`hologideon3trigger`/`Trigger3` at (176,448,3200) starts the `gideoncam3` camera and,
at +7 s, sends `spawn50` Trigger (two Spawners at y 888) and KILL to the skylight
glass `SL42`–`SL44` (y 864) at the same instant. The spawned Fanatic's box starts
inside the glass brush, whose removal is deferred to the frame end. Its next
`MoveObject` refused the start penetration (depth 0, no normal), the simulation step
failed and the frontend exited, with nothing but a log line.

- `MoveObject` now lets a body that starts inside **another object's world model**
  (brush, mover) move through that one model, counted in
  `ObjectRecord::brush_start_passes` and shown in the NPC inspector. The primary
  world, characters and a second overlapping model still refuse. Replacement physics:
  the original engine did not stop such a body either.
- A fatal frontend error in ordinary play now opens an error dialog naming the
  failure (world and first diagnostics) and the log folder. Smoke, capture and XR
  runs keep the log-only behaviour.
- `skylight_spawn.museum` (private data) plays the cutscene and spawn for 20 s and
  checks that both Fanatics passed out of the glass and the camera returned.

The first two holograms and the final confrontation (`confronttrig`, Prophet,
Gabriella, `removeendstuff`) also run 60 s without failure at every character and at
easy/hard difficulty. A crash elsewhere would now show its diagnostics.

## Widescreen view and the view weapon

The original stretched a fixed 90 × 72° view (`m_fovYScale` 0.8) to any resolution.
The port kept 90° across with square pixels, so a 16:9 or wider screen lost vertical
field and the view weapon grew until its bottom was cut. The projection is now Hor+
anchored on 4:3 (`horizontal_projection_scale`): wider screens keep the 4:3 vertical
field (73.7°) and widen the horizontal one (106° at 16:9), narrower ones keep 90°
across. 4:3 output is unchanged. Frustum culling, tracers, ambient occlusion and
reflections use the same scale; XR eyes keep their own projections. The report also
mentions the HUD losing its top and bottom in fullscreen. This could not be
reproduced here, and the HUD layout is unchanged.

## Letterbox bars

Letterboxed cameras showed the world fog colour (green in CityHub/Museum) above and
below the picture because the scene pass clears to the fog colour. Black bars now
cover the presenter's integer height/6 rows (`draw_letterbox`); XR has no letterbox.

## Sprite and FX filtering

Muzzle flashes, blood pools and other sprites used a clamped point sampler and
looked blocky up close. The original Direct3D renderer filtered them. `SpriteAlpha`
world and model passes now use a trilinear clamped sampler. Each DTX is its own
texture, so nothing bleeds; the shadow atlas and post-processing keep point taps.

## CityHub escalators (fog)

The escalators are `Conveyor` volumes (`escalator1`–`6`) with `FogEnable 0` in a
fogged world (teal, 1–5000). The original client ran `FogEnable 0` inside any
visible fog-off volume, so the fog vanished as soon as the camera entered the ramp.
Testers saw this as a shader change. Departure: a fog-off volume now keeps the
current fog; fog-on volumes (water) still replace it.

## Hovering bosses (resize anchor)

The original `SetObjectDims2` keeps an object's centre. Our ResizeObject anchored the
feet, which is equivalent for gravity bodies but moved hovering actors (Gideon,
Zealot, Thief, …) up or down with every clip size. Bodies without FLAG_GRAVITY now
keep the centre; gravity bodies and the player stay foot-anchored. Gideon's authored
hover (64 units over the floor in CityHub and the Museum) and the Museum hologram
`hologid3` (an authored, gravity-free CDestructableModel) are original data and stay
as authored.

## Data mods

A root `cres.dll` in a data-only REZ mod is accepted. It is only read as string and
bitmap resources by the bounded PE reader and never executed (B2K5 ships one). Other
`.dll`, `.exe` and now `.lto` files still refuse the mod. Nightmares (`BLOOD2NM.REZ`)
is the official expansion with its own `object.lto`, `cshell.dll` and `cres.dll`,
plus new classes (Nightmare, Gremlin, RetroCultist, Prophet variants, Flayer, Combat
Shotgun); the original source's `_ADD_ON` branches cover it. It remains refused with
an explicit message. Supporting it is a separate port.

## Not fixed in this increment

- **Activation visibility.** The CityHub intro shows Gideon in his initial
  `run_knife1` clip (clip 0, from `SetAnimation(0)`) during the 15 s `Wait`. The two
  Train Station guards (`pride`, `wrath`; `Trigger10` scripts) also stay put until
  seen. In both cases the actor is asleep and only `PingObjects` wakes it. Retail
  uses the DAT leaf visibility lists, whose compression is not decoded. Our
  four-segment ray stand-in is blocked by the walls in both places. The fix is
  decoding those lists, which needs Server.dll analysis.
- **Chosen mode.** Single Player > CHOSEN is `GAMETYPE_ACTION`. `TypeActionMode=false`
  objects (cameras, cinematic and conversation triggers) are rejected, so there are no
  cut scenes. Action mode counts as multiplayer in many original branches (player
  init, start points, corpses, damage), which need a per-site audit before it can be
  enabled. The port still starts every menu game as `GAMETYPE_SINGLE`.
- **Idle before activation** matches the original: inactive objects get neither
  updates nor model animation. The visible difference comes from the same activation
  stand-in.
- Item lighting, cutscene and movement-weight remarks have no reproducible detail yet.

## Verification

`release` preset on macOS/Metal with private data and GPU tests: 1548 tests. The
sprite pixel tests (exact texels of the 4×2 fixture) now opt into
`WorldRenderer::point_sprites`. `gpu.sprite_alpha` checks that the shipped sampler
blends the red/black column edge. `explosion_flames.private` expected the pre-lot-10
"light illumination" gap and now expects none. `gpu.viewport`, `gpu.polygrid` and
`gpu.inventory_use` already fail on `a50bd918` without these changes and remain open.
