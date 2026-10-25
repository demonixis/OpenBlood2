# Display and headset options — 2026-10-25

Implemented the separately requested window/fullscreen, resolution, VSync/FPS and
VR display preferences. This does not reopen M2, clear M2-GPU-001 or accept M3/M8.
The maintained rules are in the [menu contract](../../contracts/menu_contracts.md#window-frame-pacing-and-headset-display-options)
and [OpenXR contract](../../contracts/xr_runtime_contracts.md#headset-display-options).

## Behavior

- Display adds fullscreen, borderless/exclusive mode, drawable pixel resolution,
  VSync and 30/60/120/240/unlimited FPS ceilings, plus enumerated monitor rates.
  Unsupported controls are disabled; refused changes restore prior settings.
- The SDL adapter uses enumerated modes even for the default exclusive resolution.
  The macOS test initially exposed that passing the desktop mode directly could
  fail SDL's fullscreen-mode matching; enumeration fixed that failure.
- VR adds runtime-enumerated refresh requests and a separate actual-frequency line.
  Optional extension absence is visible. AUTO leaves the choice to the runtime.
- VR render scale is 50–100% per dimension in 10% steps. Smaller eye/overlay subrects
  preserve swapchain lifetime and the fixed-resolution menu/HUD quad. There is no
  supersampling or dynamic resolution; headset swapchain memory stays allocated.
- Preferences v10 read v1–v9 with defaults, reject malformed new keys transactionally,
  and keep atomic replacement. Small-font lists scroll only when their actual font
  metrics require it; the retail fourteen-row Display page fits without scrolling.

## Validation

Inspector-free macOS Release built with both `BLOOD2_XR=OFF` and `ON` (the latter
checks compilation; macOS is not claimed as an OpenXR runtime). Quest arm64 built
through `cmake --preset quest` and `cmake --build --preset quest`, including the
`assembleQuestDebug` APK task. No simulator, emulator or headset was launched.

Headless tests: **36 passed**:

- `menus.state`, `menus.preferences`, `menus.credits_text`, `menus.artwork`;
- `demo_menu.manifest`, `demo_menu.flow`, `demo_menu.assets`;
- the fourteen `xr.*` math/input/player cases;
- the eleven `input_actions.*` cases;
- private-data `menus.session`, `menus.cheats`, `menus.credits`, `menus.profiles`.

Desktop GPU tests (macOS/Metal): **5 passed**:

- `gpu.display_options`: real window pixel sizing (Retina included), borderless and
  exclusive transitions, supported VSync toggle, unsupported resolution rollback,
  presentation after each change, and frame-budget pacing;
- `gpu.menus`, `gpu.menu_traversal`, `gpu.demo_menu`: bitmap layout/captures and real
  frontend navigation, with isolated user directories;
- `gpu.surface`: original/enhanced switching still restores byte-identical original
  frames. Added stereo checks in CityHub and SubwayA: a 50% subrect in a fixed-size
  two-layer color target matches a native-size smaller target byte for byte, and
  returning to 100% restores both eye layers exactly.

The actual OpenXR refresh-rate request, compositor subrect presentation and worn
Quest behavior still require device qualification. FPS choices are ceilings, not
measured performance guarantees. Recentring and adjustable haptics remain suggestions,
not additions in this change.
