# M2: Playtest mouse entry and first Cultist combat

The user confirmed that F3 enabled mouse look/fire in the previous build. The
playtest still exposed three paths not covered by the previous keyboard-fire smoke: player entry left the mouse owned by inspection;
startup capture could be discarded before focus; and an authored Cultist encounter
could suspend either in dodge or while creating its corpse. These paths are now
corrected. See [contracts](../../contracts/playtest_input_contracts.md) and
[source provenance](../sources/m2_playtest_input_sources.json).
**M2 remains incomplete.**

## Playing the current build

Use **Enter player and play** in the inspector, or launch `--view-world`. Once the
window is focused, the mouse is captured and inspection hides. Move the mouse to
look, use left click for primary fire and right click for secondary fire. Escape
releases the cursor for inspection; Play / resume, F1 or closing inspection returns
to play. F3 remains a capture toggle. A focus/entry click is never itself a shot.
Folding still leaves the cursor available for the inspector's title arrow.

WASD/ZQSD, Shift, Space, Ctrl and E retain movement/run/jump/crouch/USE. F/G remain
keyboard firing alternatives. The original intro blocks movement/fire for about
12 seconds; it is not bypassed in normal play. Explicit focus loss releases capture
and requires a new play request. Missing SDL capture is reported in the UI/log.

## Reproduced combat failures

The new test enters SubwayA normally, waits for the original camera sequence and
walks from the actual spawn to the first visible authored Cultist, `gak`, using W/E.
Mouse motion aims at its current animated head; only mouse buttons request firing.
No NPC health, armor, inventory, position, activation or target is edited. No damage
message is injected directly and the player is never teleported.

Before correction, primary fire reduced its HP to zero but corpse placement
oscillated between train-seat faces for 16 corrections, suspended the actor and
retained the visible source. The accepted fallback now explores other source-side
separating axes with the original body dimensions. It checks at most 128 additional
candidates; each path retains 16 corrections and the original distance cap. The
final full box and center path must pass unchanged collision checks. Exact touching
fits add no artificial skin. A genuinely impossible fit still fails. Candidate
counts and fallback usage are exposed in Skeleton inspection. This is replacement
physics, not established retail solver parity.

After that fix, secondary fire revealed impact hearing selecting original Dodge
state 10, whose dispatch was missing. Original Cultist dodge, CalculateDodge and
left/right rolls are now connected. Strict direction comparisons, obstruction
flags, lateral-only roll selection, grounded velocity refresh and independent
obstruction/completion phase increments are preserved. Invalid poses, required
services and undefined original zero-direction cases remain explicit failures.
The ray uses the documented replacement entry-face policy; no retail ray parity
is claimed. Dodge flags, queries and updates are inspectable even with rendering
hidden. No mutable global or dependency was added.

Both test passes now kill the same authored Cultist, verify player-attributed
health loss, original FIRE_KEY/ammo/voice requests, checked source removal and an
independent nonpenetrating corpse. W/E then walks beyond the former enemy and
through the next door to Z=-1861.92. The secondary pass executes 12 dodge updates.
Unload/reload invalidates the previous player, enemy and corpse identities.

## Verification

Blood2 commit: `4b1675cdd4efbeb1ae3006417cb95a3736764952`.

Debug and inspector-free Release builds pass. Each complete headless suite passes
**734/734 tests**, including **119 private-data cases**. The focused Clang
ASan/UBSan/leak run passes **61/61**, including seven private-data cases, with no
reported findings. The six selected SDL/Vulkan tests pass in each build: lifecycle,
weapons, movement, viewport, player controls and the new mouse encounter. Both GPU
runs select **NVIDIA GeForce RTX 3060 Laptop GPU**, confirmed in application logs.

[Machine-readable evidence and logs](evidence/m2-playtest-input.json) record the
commands, timings, source integrity and private capture hashes. All 16 changed
C++ files pass clang-format; 10,923 reference files and six reference repositories
are unchanged. Local Markdown targets and authored-file whitespace checks pass;
raw build logs and the generated patch retain their original whitespace.

Commands from `Blood2/`:

```sh
cmake --build build/debug -j3
cmake --build build/release -j4
ctest --test-dir build/debug -L headless -j4 --output-on-failure
ctest --test-dir build/release -L headless -j4 --output-on-failure
cmake --build build/sanitize -j3 --target player_controls_tests services_tests \
  object_contract_tests corpse_tests ai_hit_tests ai_locomotion_tests session_playback_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -j2 --output-on-failure \
  -R '^(player_controls|services|objects|corpses|ai_hit|ai_locomotion|session_playback)\.'
# Repeat sequentially with build/release; this local driver path is machine-specific.
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json \
  ctest --test-dir build/debug --output-on-failure \
  -R '^gpu\.(weapons|player_controls|mouse_combat|movement|viewport|lifecycle)$'
```

Tests use synthetic SDL events, not physical-device automation or a human
walkthrough. The mouse encounter variant checks native relative-mode state but
does not render the encounter itself. Separate application smoke tests check
actual rendering and inspection; normal `--view-world` captures provide a visual
startup check. Some smoke captures land during the deliberate 1x1 resize, so those
are not presented as scene-image evidence. No other platform or XR is claimed.

Required world services, other unsupported AI actions/deaths, complete campaign
behavior and safe NEW_LEVEL remain open. Source-entry counts and the M2 exit gate
are unchanged. Reference repositories and external commercial data are preserved.

## Follow-up: stationary train appearance

The user also reported that the train appears stationary, especially during the
intro. A read-only probe of the external SubwayA DAT found **60 authored `pan`
surface effects**: 24 `up -1024`, 18 `left -1024`, and 18 `left 1024`. They are
attached to tunnel concrete (`brushcrete2X.dtx`) and track (`tracks2X.dtx`) surfaces.
The current v56 reader consumes but discards both effect strings, and the world
renderer uses static UVs. This is a concrete missing source of apparent motion.
The existing `htrain1` / `happytrain` path is a separate later scripted object;
activating it at entry would not implement the missing effect. The looping
`sublight` KeyFramer also remains distinct from texture motion.

Next visual correction: preserve these authored effects, establish the compatible
original direction/speed convention, evaluate from the session clock and expose
unsupported effects honestly. Verify rendered motion during the normal intro and
across pause/reload on NVIDIA. No panning or train-motion fix is claimed in this
combat increment, and no commercial payload was copied into either repository.

Follow-up implemented in [surface panning](m2_surface_effects.md): the original numeric P/Q convention is now evaluated and inspected. The earlier static-UV limitation above records the state of this combat increment.

## Subsequent user report — 17 October

The user confirms mouse control works after capture, but reports intermittent frozen
dead Cultists still blocking the train, including later enemies. This is an open
regression despite the deterministic cases above. Broader positions/impact regions
and death placement must be reproduced; no new fix is claimed by this note.
