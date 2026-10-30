# M9 — independent client scene inputs

2026-10-30, following `a8a52254`. ScenePresenter and the model, sprite, PolyGrid
and model-lighting consumers now accept client-owned presentation inputs without
GameSession, ServerAdapter or original server objects. This prepares the graphical
remote client; it does not yet provide the host/join frontend or network combat.
See the [contract](../../contracts/multiplayer_contracts.md) and
[source ledger](../sources/m9_scene_inputs_sources.json).

## Boundary and preserved behavior

`ClientScene` borrows the client's decoded DAT/resources, passive ClientWorld and
ClientEffects, camera/settings and up to two client weapon visuals for one call.
It stores no callbacks or handles. The renderer continues to own its GPU registry;
world epoch/revision still release world textures, geometry and dependent caches.
Mixed effect/world identities are refused before presentation, and record/record_eyes
refuse inputs from a different world than the prepared GPU resources.

The original CViewWeapon visual members now form `ClientWeaponView`, inherited by
the existing runtime. AdjustAngleAndSetPos and FlashOffset move without arithmetic
changes; original handed offsets, muzzle-node sampling, desktop x10 projection,
XR arm poses/scales and fallback behavior remain. The independent consumer checks
flash type/index before indexing the two slots. Firing, ammo, animation-key timing
and the local weapon/server association are unchanged and still require their
subsequent remote extraction.

Small compatibility functions in separate `local_scene_*` archive members retain
existing solo entrypoints. Their publication happens at the same place; the
ScenePresenter bridge publishes once, then passes the delivered values to every
consumer. Individual scene helpers still publish when called directly. Scene light
scale/FOV are sampled by the bridge; consumers have no way to read server clocks.
The GPU compatibility member is excluded from server-only builds.

No shader, PBR file, light-selection scope, draw ordering or GPU allocation policy
is replaced. Original/Enhanced shading, post-processing, shadow selection, sky
ordering, per-eye effects and tracked-hand placement keep their existing code.
ClientScene is an in-process presentation interface, not a network/GPU protocol.

## Verification

The final native selection passes **56/56 tests**, including six GPU tests,
model/sprite/light views, weapon rules/effects, movement and player restore. The
server-only ASan/UBSan selection passes **6/6**: independent scene inputs,
server movement/cameras/private events and dedicated-plus-two-client process
probes. The final world-identity checks pass an additional focused sanitizer
rerun (1/1). Native client/server and sanitized server-only builds pass.

All five solo oracles remain byte-identical: effects 240 frames, audio 360,
movement 360, environment 720 and view 300. All **6,551** protected PBR files,
captures and historical references/evidence match their pre-edit hashes.
`nm -C` finds no GameSession, ServerAdapter, CBloodServerShell, Objects methods
or CViewWeapon runtime in either independent scene executable. The dedicated
binary has no ScenePresenter symbol, and its CMake sources exclude the GPU bridge.
Evidence uses `evidence/m9_scene_inputs_*`; commercial content remains external.

Commands (from Blood2, complete build/test logs retained):

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server network_scene_inputs_tests network_scene_gpu_tests model_view_tests sprite_view_tests world_lighting_tests surface_gpu_tests post_gpu_tests model_shadow_gpu_tests third_person_gpu_tests player_restore_tests weapon_tests weapon_sprite_tests -j6
ctest --test-dir build/m9-fx-runtime -R '^network\.(scene_inputs)$|^gpu\.network_scene$|^model_view\.|^sprite_view\.|^world_lighting\.|^gpu\.(surface|post|model_shadows|third_person)$|^movement\.|^player_restore\.|^gpu\.player_restore$|^weapons\.|^weapon_sprites\.' --output-on-failure -j3
cmake --build build/m9-fx-runtime-asan --target blood2_server network_scene_inputs_tests network_admission_process_tests network_server_camera_tests network_server_player_event_tests network_server_movement_tests -j4
ctest --test-dir build/m9-fx-runtime-asan -R '^network\.(scene_inputs|server_camera|server_player_events|server_movement|admission_processes|movement_processes)$' --output-on-failure -j3
```

The independent fixture decodes an encoded replica, loads its own retail Crypt
DAT and weapon assets, and prepares model/sprite/PolyGrid/light views without any
server library. Its GPU form draws the static world and a view weapon and checks
an Original → Enhanced → Original cycle for exact first/last pixel equality,
with a different Enhanced output. Invalid flash indices and mixed/prepared world
identities are refused. This is a presentation fixture, not a playable session.

The initial native selection had two fixture failures because the fixture tried
to move ClientEffects from generation 8 back to 7. The corrected fixture keeps a
separate mismatched world and preserves the monotonic generation contract. The
initial compile also exposed a missing direct <cmath> include after removing the
server header, and omitted aggregate defaults were made explicit. Failed logs are
retained. The temporary effects oracle similarly needed an explicit server header
for its own local GameSession producer; no oracle computation changed.

Hardware results are limited to this Linux machine. The existing surface test
covers desktop profiles and simulated stereo targets; this does not qualify an
XR headset, mobile/Quest, Windows/macOS, impaired combat or human LAN. M2-GPU-001
and M3 acceptance remain separate.
