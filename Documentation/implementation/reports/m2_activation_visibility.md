# M2: NPC activation through glass and Gideon's intro script

Project commit: `24f7f74215d7b80956424d22e5111077a59db143`.

The authored SubwayA intro now advances Gideon's original dialogue animation to
its scripted PASSIVE state before the first camera changes. The original camera
sequence then releases player control. No script, asset, NPC state or timer is
overridden to obtain this result.

The previous activation ray hit nglass1, a destructible glass world model, and left
Gideon asleep after two AI updates. The same ray with the existing original AI
glass/volume filter reaches Gideon. PingObjects now reuses that filter for its
replacement visibility estimate. Opaque doors still obstruct it. Retail PVS data
is not decoded, so this is an explicit replacement policy, not recovered retail
visibility behavior.

See [contracts](../../contracts/activation_visibility_contracts.md),
[source ledger](../sources/m2_activation_visibility_sources.json) and
[retained evidence](evidence/m2-activation-visibility.json).

The NPC inspector now exposes the last activation query, simulation time, checked
observer/hit identity, endpoints, polygon/fraction and committed-refresh status.
Visibility and a successful batch refresh are separate: invalid queries cannot
partly activate actors. Removed identities remain labelled historical records.
Inspection does not issue rays, wake actors or advance time.

Two new synthetic tests cover the original filter's exact property/class rules,
ordinary weapon-ray separation, primary-world obstruction, manual inactive state,
failed-batch rollback and observer/unload lifetime. The existing private SubwayA
test now runs three 13-second entry/unload cycles and checks the actual nglass1
obstruction, successful activation, PASSIVE command and camera release. The GPU
viewport checks the same script result on initial entry and after reload.

Each private cycle records 332 AI updates, 13 successful visibility pings, 293
script updates, two selected commands and 331 health messages. At two seconds,
the boss already has three pings and 91 script updates. It reaches PASSIVE while
camera1 is still active. Later auto-deactivation behind opaque geometry is retained.
These are deterministic port observations, not retail timing measurements.

Debug and inspector-free Release each pass **701/701 checks**: 644 headless and
57 serial GPU, including 144 private-data cases (103 headless). Debug takes
87.54s headless / 182.5s GPU;
Release takes 16.07s / 121.87s.
Clang ASan/UBSan with leak detection passes **72/72** selected checks in
140.25s. Retained build/runtime logs contain no compiler warning/error,
GPU validation error/VUID or sanitizer diagnostic.

Commands from Blood2: `cmake --build build/debug -j4` and
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Sanitizer build uses `-j2` for gideon_runtime_tests, ai_runtime_tests, ai_sight_tests,
ai_script_tests, object_contract_tests, services_tests, session_playback_tests,
player_tests and world_tests. Sanitizer CTest uses `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1` and
`^(gideon_runtime|ai_runtime|ai_sight|ai_script|objects|services|session_playback|player)\.`
plus `^world.destination_(trials|lifetime|trial_private)$`.

All eight changed C++ files pass clang-format 23.1.3; all 2776 local Markdown
links resolve. All 10923 reference file hashes and six reference repositories
remain unchanged. Only Linux/Vulkan was tested. The Release train viewport capture was visually checked.
GPU captures stay ignored because
they display commercial resources; logs contain the initial/reloaded script checks.

SubwayA source accounting is unchanged: 204 created, four transferred sounds,
372 editor-only, zero unknown and zero failed entries; 16 production NPCs.
M2 remains incomplete. The tested intro does not establish complete boss combat/
death parity, retail PVS activation, safe NEW_LEVEL or all required services/FX.
The previous [Gideon lifecycle report](m2_gideon_runtime.md) remains historical
evidence of the two-update stall; this continuation fixes that bounded case.
