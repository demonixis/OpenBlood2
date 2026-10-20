# M2 continuation: animated models and first-person weapons

**Status: implemented for the bounded Linux presentation path; M2 remains incomplete.**
The navigable textured level now displays existing ABC instances and original
Beretta/knife view models. This makes placement, animation and attachment defects
visible while completing required gameplay services. It does not satisfy M3's
combat scenario or replace the unchanged M2 exit gate.

## What changed

The new ModelView snapshot consumes current original object/weapon animation tracks,
face UVs, skin resources and attachment poses. It uses the existing SDL_GPU shader,
shared world depth for ordinary models and separate close-weapon depth. The source
CViewWeapon placement formula and handedness are retained, with zero bob. Textures
cache per world revision; dynamic vertex buffers are reused. The inspector reports
hidden/missing records and real resource failures. No dependency pins changed.

The first real model capture exposed a preexisting ABC inverse-rotation omission.
Absent TransformInfo now selects the legacy quaternion conjugation at decode time;
explicit zero retains the direct convention. This correction applies equally to
rendering, head-node queries and attachments. The Beretta's authored static-key
bounds are an independent regression oracle: the old pose reached Z=8.334, beyond
the authored maximum 4.237; the corrected maximum is 4.079 before basis normalization. Synthetic nonidentity
rotations exercise default/explicit-zero and hierarchical deformation paths.
A second visual check found the model still facing opposite the +Z firing direction.
The reader now normalizes ABC -Z to engine +Z consistently across geometry, bounds,
node rotations/positions and packed deformation. The private muzzle/hand ordering
and synthetic nonidentity yaw test cover this separately; no per-weapon workaround
is used. Existing engine-space fixture assertions are preserved by encoding their
Z values in file coordinates. The inspected Caleb image improved too; retail visual/animation parity remains
unverified. Nonzero explicit transform flags still fail.

Source provenance and the exact boundary are in
[model-view contracts](../../contracts/model_view_contracts.md) and the
[source ledger](../sources/m2_model_view_sources.json). All renderer/buffer
code is new engine work; no donor renderer or ECS was imported.

## Observed content and limits

SubwayA retains 187 live-created source entries, four transferred sounds, 372
editor-only, 17 unsupported and zero failed entries, plus seven owned SelfTriggers.
The first inventory update adds one held Beretta. The deliberately visible Caleb/
held diagnostic draws 1,009 triangles with ten inspected entries and 557,060 skin
bytes. Ordinary first person draws the existing 878-triangle view weapon and hides
the local body/held model; seven PathPoints remain hidden. Unsupported CivilianAI,
CultistAI, Gideon and ExitTrigger are not synthesized as successful objects.

Images show unlit opaque skins, no normal/chrome/alpha effects or lighting. Bob,
recoil, retail close-view projection, mouse-look/fire bindings, client FX, complete
NPC behavior and transitions are still pending. Source-controlled firing inspection
remains available; this increment adds no new gameplay input. The isolated Caleb
capture is diagnostic presentation, not an encountered enemy. Other platforms,
GPU backends and XR remain unverified.

## Validation

Final GCC Debug: **350/350**, 166.11s. Inspector-free GCC Release:
**350/350**, 100.87s. Each full suite has 274 synthetic headless,
49 private headless and 27 serial GPU cases. Targeted Clang ASan/UBSan with leak
checks: **116/116**, 198.78s. This includes original object/player/weapon
lifetime, model readers, character poses/audio/motion, AI model-key/sight/target
queries, attachments, model/world presentation and the DTX corpus; the full
323-case headless sanitizer suite was not rerun. No final compiler, sanitizer
or Vulkan validation findings were observed.

The final nonidentity-yaw/basis regression and file-authored Beretta bound check
also pass in the targeted Debug rerun after the full suite. Linux Wayland/Vulkan
used the NVIDIA GeForce RTX 3060 Laptop GPU. The source inventory remains unchanged
(10,923 reference files, six clean reference repositories). CLI player inspection
succeeds; full startup still exits 6. Formatting and local Markdown links were checked.
Dependency revisions and shaders are unchanged.

Commands run from Blood2:

```sh
cmake --build --preset debug -j4
ctest --test-dir build/debug --output-on-failure -j4
cmake --preset local-release
cmake --build --preset release -j4
ctest --test-dir build/release --output-on-failure -j4
cmake -S . -B build/sanitize
cmake --build build/sanitize -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(objects|services|player|weapons|locomotion|character|character_audio|character_motion|ai_keys|ai_sight|attachments|ai_target|model_view|world_view|texture)\.' --output-on-failure -j1
```

[First-person capture](evidence/m2-model-view-subway.png) and
[isolated model capture](evidence/m2-model-view-caleb.png) show the final diagnostic
presentation. They are rendered test evidence, not imported game resources.

Early new assertions mistakenly expected NPC models in SubwayA and retained two
world draws after pending player removal. Actual state is two when the local body
is shown, zero in first person and one independent child until deferred destruction.
Those new assertions were corrected; existing gameplay assertions remain intact.
Visual checks revealed the inverse-rotation and model-basis defects above; early
successful suites preceded the basis correction and are retained as intermediate evidence. Intermediate
failure logs are retained alongside final successful runs.

The [execution ledger](evidence/m2-model-view.json) records commands, results,
source integrity, project revision, patch and artifact hashes. Commercial assets
remain external; only rendered diagnostic captures and counts are retained.

Next M2 work returns to the required original NPC state/acquisition behavior,
client FX and ExitTrigger/session services. Further renderer polish does not close
those gates and is not a substitute for the original gameplay integration.
