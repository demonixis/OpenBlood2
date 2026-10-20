# M2: Initial corpse clearance in the train

Project commit: `996ebbdbadb344239f113ba0c9746de209bdbb9a`.

Ordinary keyboard firing in SubwayA now transfers the killed NPC to its original
animated corpse without leaving it intersecting train geometry. The raw death
animation box is wider than the living actor: the reproduced body changes from
half dimensions approximately (14.512,35.647,14.512) to (36,5,36). The previous
placement check rejected that overlap and retained a suspended visible NPC.

The engine now attempts bounded local clearance while preserving the original
cached transform, scale, raw dimensions and requested foot-offset formula. Each
correction stays on a separating side of the clear source body, then validates the
full result and a clear center segment from the source. A nearest-side-only attempt
was rejected during development because it pushed the candidate beneath the floor.
That failure is retained as diagnosis, and a synthetic regression covers it.

The reproduced final fit moves from approximately (-73.172,133.002,-2412.32) to
(-59.999,133.002,-2428), with two corrections totaling 28.8523 original units.
Y and raw dimensions remain unchanged. The resulting body continues ordinary
gravity/animation after transfer. This is a replacement collision policy, not a
measurement of retail SetObjectDims2 or corpse placement.

See [contracts](../../contracts/corpse_placement_contracts.md),
[source ledger](../sources/m2_corpse_placement_sources.json) and
[retained evidence](evidence/m2-corpse-placement.json).

The Skeletons inspector shows outcome, cached anchor, requested/candidate position,
correction count/distance and last polygon for corpses and failed NPC transfers.
These are owned historical values: reading them neither performs collision queries
nor advances simulation. Failure still leaves the actor visible and removes the
partial child deferred. No dimension shrinking, general teleport change or
suppression of physics failures is used.

Two new synthetic services cases cover corner clearance, source-side floor
selection, impossible narrow spaces, obstructed center paths, invalid inputs,
unavailable collision, atomic refusal, continued gravity and removed handles.
Existing corpse transfer/failure cases check the cached anchor and retained report;
existing source-removal, audio, animation, private-model and GPU reload checks
remain enabled. The existing private player-controls scenario requires an actually
corrected corpse from its ordinary F/G input sequence, alongside 12 FIRE_KEY shots,
38 remaining bullets and 12 real fire voices. No aim/health/AI override was added.

Debug and inspector-free Release each pass **703/703 checks**: 646 headless and
57 serial GPU, including 144 private-data cases (103 headless). Debug takes
96.39s headless / 184.74s GPU;
Release takes 14.48s / 124.08s.
Clang ASan/UBSan with leak detection passes **76/76** selected checks in
115.01s. Retained final build/runtime logs contain no compiler
warning/error, GPU validation error/VUID or sanitizer diagnostic. The separately
labelled development logs retain the rejected nearest-side attempt and the
first-suite assertion failure: a new transfer-only assertion was mistakenly added
to direct diagnostic construction, then corrected to require Unqueried there.

Commands from Blood2: `cmake --build build/debug -j4` and
`cmake --build build/release -j4`, followed by
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Sanitizer build uses `-j2` for services_tests, object_contract_tests, corpse_tests,
player_controls_tests, ai_hit_tests, ai_state_tests, ai_runtime_tests,
session_playback_tests and world_tests. Sanitizer CTest uses `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1` and
`^(corpses|ai_hit|ai_state|ai_runtime|objects|services|session_playback|player_controls)\.`
plus `^world.destination_(trials|lifetime|trial_private)$`.

Changed C++ files pass clang-format 23.1.3; all 2792 local Markdown links resolve.
All 10923 reference-file hashes and six reference repositories remain unchanged.
Only Linux/Vulkan was tested. Debug/Release skeleton captures were visually checked;
they remain ignored because they display commercial resources.

SubwayA authored accounting is unchanged: 204 created, four transferred sounds,
372 editor-only, zero unsupported and zero failed entries. Dynamic corpses are
not extra authored class coverage. M2 remains incomplete: this bounded placement
can refuse difficult fits, uses primary-world triangles rather than BSP solid-volume
contents or dynamic supports, and does not establish full corpse/fire/gib behavior,
retail collision parity, safe NEW_LEVEL or all remaining required services/FX.
